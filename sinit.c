/* See LICENSE file for copyright and license details. */
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static sigset_t set;

pid_t spawn(char *const argv[]) {
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		perror("[FATAL] Fork failed");
		_exit(1);
	} else if (pid == 0) {
		sigprocmask(SIG_UNBLOCK, &set, NULL);
		execvp(argv[0], argv);
		perror("[ERROR] Executing child process failed");
		_exit(1);
	} else {
		return pid;
	}
}

void print_usage_and_exit(char *name, FILE *file, int status) {
	fprintf(file, "Usage: %s [-h | program arg1 arg2]\n", name);
	exit(status);
}

int main(int argc, char *argv[]) {
	siginfo_t sig;

	pid_t child_pid;

	pid_t current_pid;
	int current_status;

	struct timespec ts;
	ts.tv_sec = 1;
	ts.tv_nsec = 0;

	char* name = argv[0];

	/* Start with argument processing */
	int c;
	while ((c = getopt (argc, argv, "h")) != -1) {
		switch (c) {
			case 'h':
				print_usage_and_exit(name, stdout, 0);
				break;
			case '?':
				print_usage_and_exit(name, stderr, 1);
				break;
			default:
				// Should never happen
				abort();
		}
	}

	int i;
	char* child_args[argc-optind+1];
	for (i = 0; i < argc - optind; i++) {
		child_args[i] = argv[optind+i];
	}
	child_args[i] = NULL;

	if (i == 0) {
		print_usage_and_exit(name, stdout, 1);
	}

	/* Prepare signals */
	if (sigfillset(&set)) {
		perror("sigfillset");
		return 1;
	}
	if (sigprocmask(SIG_BLOCK, &set, NULL)) {
		perror("sigprocmask");
		return 1;
	}

	/* Spawn the main command */
	child_pid = spawn(child_args);
	printf("[INFO ] Spawned child process\n");

	/* Loop forever:
	 * - Reap zombies
	 * - Forward signals
	 */
	while (1) {
		if (sigtimedwait(&set, &sig, &ts) == -1) {
			switch (errno) {
				case EAGAIN:
					break;
				case EINTR:
					break;
				case EINVAL:
					perror("[ERROR] Fatal!");
					return 2;
			}
		} else {
			/* There is a signal to handle here */
			switch (sig.si_signo) {
				case SIGCHLD:
					// Special-cased, as we don't forward SIGCHLD. Instead, we'll
					// fallthrough to reaping processes.
					printf("[INFO ] Received SIGCHLD\n");
					break;
				default:
					printf("[INFO ] Passing signal: %s\n", strsignal(sig.si_signo));
					// Forward anything else
					kill(child_pid, sig.si_signo);
					break;
			}
		}

		/* Now, reap zombies */
		while (1) {
			current_pid = waitpid(-1, &current_status, WNOHANG);
			switch (current_pid) {
				case -1:
					// An error occured. Print it and exit.
					perror("waitpids returned -1");
					return 1;
				case 0:
					// No child to reap. We'll break out of the loop here.
					break;
				default:
					// A child was reaped. Check whether it's the main one,
					// and go for another iteration otherwise.
					if (current_pid == child_pid) {
						printf("[INFO ] Main child has exited\n");
						if (WIFEXITED(current_status)) {
							// Our process exited normally.
							printf("[DEBUG] Main child exited normally (check exit status)\n");
							return WEXITSTATUS(current_status);
						} else if (WIFSIGNALED(current_status)) {
							// Our process was terminated. Emulate what sh / bash
							// would do, which is to return 128 + signal number.
							printf("[DEBUG] Main child exited with signal\n");
							return 128 + WTERMSIG(current_status);
						} else {
							printf("[WARNING] Main child exited for unknown reason\n");
							return 1;
						}
					}
					continue;
			}

			// If we make it here, that's because we did not continue in
			// the switch case.
			break;
		}

	}
	/* not reachable */
	return 0;
}

