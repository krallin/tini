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

#define PRINT_FATAL(...)    fprintf(stderr, "[FATAL] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define PRINT_WARNING(...)  if (verbosity > 0) { fprintf(stderr, "[WARN ] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
#define PRINT_INFO(...)     if (verbosity > 1) { fprintf(stderr, "[INFO ] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
#define PRINT_DEBUG(...)    if (verbosity > 2) { fprintf(stderr, "[DEBUG] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }

static int verbosity = 0;

pid_t spawn(sigset_t set, char *const argv[]) {
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		PRINT_FATAL("Fork failed: '%s'", strerror(errno));
		_exit(1);
	} else if (pid == 0) {
		sigprocmask(SIG_UNBLOCK, &set, NULL);
		execvp(argv[0], argv);
		PRINT_FATAL("Executing child process failed: '%s'", strerror(errno));
		_exit(1);
	} else {
		return pid;
	}
}

void print_usage_and_exit(const char *name, FILE *file, const int status) {
	fprintf(file, "Usage: %s [-h | program arg1 arg2]\n", name);
	exit(status);
}

int main(int argc, char *argv[]) {
	char* name = argv[0];

	siginfo_t sig;

	pid_t child_pid;

	pid_t current_pid;
	int current_status;

	int exit_code = -1;

	struct timespec ts;
	ts.tv_sec = 1;
	ts.tv_nsec = 0;

	sigset_t set;

	/* Start with argument processing */
	int c;
	while ((c = getopt (argc, argv, "hv")) != -1) {
		switch (c) {
			case 'h':
				print_usage_and_exit(name, stdout, 0);
				break;
			case 'v':
				verbosity++;
				break;
			case '?':
				print_usage_and_exit(name, stderr, 1);
				break;
			default:
				/* Should never happen */
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
		/* User forgot to provide args! */
		print_usage_and_exit(name, stdout, 1);
	}

	/* Prepare signals */
	if (sigfillset(&set)) {
		PRINT_FATAL("sigfillset failed: '%s'", strerror(errno));
		return 1;
	}
	if (sigprocmask(SIG_BLOCK, &set, NULL)) {
		PRINT_FATAL("Blocking signals failed: '%s'", strerror(errno));
		return 1;
	}

	/* Spawn the main command */
	child_pid = spawn(set, child_args);
	PRINT_INFO("Spawned child process");

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
					PRINT_INFO("EINVAL on sigtimedwait!");
					return 2;
			}
		} else {
			/* There is a signal to handle here */
			switch (sig.si_signo) {
				case SIGCHLD:
					/* Special-cased, as we don't forward SIGCHLD. Instead, we'll
					 * fallthrough to reaping processes.
					 */
					PRINT_DEBUG("Received SIGCHLD");
					break;
				default:
					PRINT_DEBUG("Passing signal: '%s'", strsignal(sig.si_signo));
					/* Forward anything else */
					kill(child_pid, sig.si_signo);
					break;
			}
		}

		/* Now, reap zombies */
		while (1) {
			current_pid = waitpid(-1, &current_status, WNOHANG);
			switch (current_pid) {
				case -1:
					if (errno == ECHILD) {
						// No childs to wait. Let's break out of the loop.
						break;
					}
					/* An unknown error occured. Print it and exit. */
					PRINT_FATAL("Error while waiting for pids: '%s'", strerror(errno));
					return 1;
				case 0:
					/* No child to reap. We'll break out of the loop here. */
					break;
				default:
					/* A child was reaped. Check whether it's the main one. If it is, then
					 * set the exit_code, which will cause us to exit once we've reaped everyone else.
					 */
					PRINT_DEBUG("Reaped child with pid: '%i'", current_pid);
					if (current_pid == child_pid) {
						if (WIFEXITED(current_status)) {
							/* Our process exited normally. */
							PRINT_INFO("Main child exited normally (with status '%i')", WEXITSTATUS(current_status));
							exit_code = WEXITSTATUS(current_status);
						} else if (WIFSIGNALED(current_status)) {
							/* Our process was terminated. Emulate what sh / bash
							 * would do, which is to return 128 + signal number.
							*/
							PRINT_INFO("Main child exited with signal (with signal '%s')", strsignal(WTERMSIG(current_status)));
							exit_code = 128 + WTERMSIG(current_status);
						} else {
							PRINT_FATAL("Main child exited for unknown reason!");
							return 1;
						}
					}
					continue;
			}

			/* If exit_code is not equal to -1, then we're exiting because our main child has exited */
			if (exit_code != -1 ) {
				return exit_code;
			}

			/* If we make it here, that's because we did not continue in the switch case. */
			break;
		}

	}
	/* not reachable */
	return 0;
}

