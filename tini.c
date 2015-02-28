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
#include <stdbool.h>


#define PRINT_FATAL(...)    fprintf(stderr, "[FATAL] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define PRINT_WARNING(...)  if (verbosity > 0) { fprintf(stderr, "[WARN ] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
#define PRINT_INFO(...)     if (verbosity > 1) { fprintf(stderr, "[INFO ] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
#define PRINT_DEBUG(...)    if (verbosity > 2) { fprintf(stderr, "[DEBUG] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
#define PRINT_TRACE(...)    if (verbosity > 3) { fprintf(stderr, "[TRACE] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }

#define ARRAY_LEN(x)  (sizeof(x) / sizeof(x[0]))


static int verbosity = 0;
static struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };


pid_t spawn(const sigset_t* const child_sigset_ptr, char (*argv[])) {
	/* TODO - Don't exit here! */
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		PRINT_FATAL("Fork failed: '%s'", strerror(errno));
		_exit(1);
	} else if (pid == 0) {
		// Child
		if (sigprocmask(SIG_SETMASK, child_sigset_ptr, NULL)) {
			PRINT_FATAL("Setting child signal mask failed: '%s'", strerror(errno));
			_exit(1);
		}
		execvp(argv[0], argv);
		PRINT_FATAL("Executing child process '%s' failed: '%s'", argv[0], strerror(errno));
		_exit(1);
	} else {
		// Parent
		PRINT_INFO("Spawned child process '%s' with pid '%i'", argv[0], pid);
		return pid;
	}
}


void print_usage(char* const name, FILE* const file) {
	fprintf(file, "Usage: %s [-h | program arg1 arg2]\n", name);
}


int parse_args(const int argc, char* const argv[], char* (**child_args_ptr_ptr)[], int* const parse_fail_exitcode_ptr) {
	/*
	 * Returns with 0 to indicate success, a positive value to indicate the process
	 * should exit with success, and -1 to indicate it should exit with a failure.
	 */
	char* name = argv[0];

	int c;
	while ((c = getopt (argc, argv, "hv")) != -1) {
		switch (c) {
			case 'h':
				/* TODO - Shouldn't cause exit with -1 ..*/
				print_usage(name, stdout);
				*parse_fail_exitcode_ptr = 0;
				return 1;
			case 'v':
				verbosity++;
				break;
			case '?':
				print_usage(name, stderr);
				return 1;
			default:
				/* Should never happen */
				return 1;
		}
	}

	*child_args_ptr_ptr = calloc(argc-optind+1, sizeof(char*));
	if (*child_args_ptr_ptr == NULL) {
		PRINT_FATAL("Failed to allocate memory for child_args_ptr_ptr: '%s'", strerror(errno));
		return 1;
	}

	int i;
	for (i = 0; i < argc - optind; i++) {
		(**child_args_ptr_ptr)[i] = argv[optind+i];
	}
	(**child_args_ptr_ptr)[i] = NULL;

	if (i == 0) {
		/* User forgot to provide args! */
		print_usage(name, stderr);
		return 1;
	}

	return 0;
}

int prepare_sigmask(sigset_t* const parent_sigset_ptr, sigset_t* const child_sigset_ptr) {
	/* Prepare signals to block; make sure we don't block program error signals. */
	if (sigfillset(parent_sigset_ptr)) {
		PRINT_FATAL("sigfillset failed: '%s'", strerror(errno));
		return 1;
	}

	uint i;
	int ignore_signals[] = {SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGABRT, SIGTRAP, SIGSYS} ;
	for (i = 0; i < ARRAY_LEN(ignore_signals); i++) {
		if (sigdelset(parent_sigset_ptr, ignore_signals[i])) {
			PRINT_FATAL("sigdelset failed: '%i'", ignore_signals[i]);
			return 1;
		}
	}

	if (sigprocmask(SIG_SETMASK, parent_sigset_ptr, child_sigset_ptr)) {
		PRINT_FATAL("sigprocmask failed: '%s'", strerror(errno));
		return 1;
	}

	return 0;
}

int wait_and_forward_signal(sigset_t const* const parent_sigset_ptr, pid_t const child_pid) {
	siginfo_t sig;

	if (sigtimedwait(parent_sigset_ptr, &sig, &ts) == -1) {
		switch (errno) {
			case EAGAIN:
				break;
			case EINTR:
				break;
			case EINVAL:
				PRINT_FATAL("EINVAL on sigtimedwait!");
				return -1;
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
				kill(child_pid, sig.si_signo);   // TODO - Check retcode!
				break;
		}
	}

	return 0;
}

int reap_zombies(const pid_t child_pid, int* const child_exitcode_ptr) {
	/*
	 * Returns:
	 *   + = 0: The iteration completed successfully, but the child is still alive.
	 *   + > 0: The iteration completed successfully, and the child was reaped.
	 *   + < 0: An error occured
	 */
	pid_t current_pid;
	int current_status;

	while (1) {
		current_pid = waitpid(-1, &current_status, WNOHANG);

		switch (current_pid) {

			case -1:
				if (errno == ECHILD) {
					PRINT_TRACE("No child to wait.");
					break;
				}
				PRINT_FATAL("Error while waiting for pids: '%s'", strerror(errno));
				return -1;

			case 0:
				PRINT_TRACE("No child to reap.");
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
						*child_exitcode_ptr = WEXITSTATUS(current_status);
					} else if (WIFSIGNALED(current_status)) {
						/* Our process was terminated. Emulate what sh / bash
						 * would do, which is to return 128 + signal number.
						 */
						PRINT_INFO("Main child exited with signal (with signal '%s')", strsignal(WTERMSIG(current_status)));
						*child_exitcode_ptr = 128 + WTERMSIG(current_status);
					} else {
						PRINT_FATAL("Main child exited for unknown reason!");
						return -1;
					}
				}

				// Check if other childs have been reaped.
				continue;
		}

		/* If we make it here, that's because we did not continue in the switch case. */
		break;
	}

	return 0;
}


int main(int argc, char *argv[]) {
	pid_t child_pid;
	int child_exitcode = -1;
	int parse_exitcode = 1;  // By default, we exit with 1 if parsing fails

	/* Prepare sigmask */
	sigset_t parent_sigset;
	sigset_t child_sigset;
	if (prepare_sigmask(&parent_sigset, &child_sigset)) {
		return 1;
	}

	/* Parse command line arguments */
	char* (*child_args_ptr)[];
	int parse_args_ret = parse_args(argc, argv, &child_args_ptr, &parse_exitcode);
	if (parse_args_ret) {
		return parse_exitcode;
	}

	child_pid = spawn(&child_sigset, *child_args_ptr);
	free(child_args_ptr);

	while (1) {
		/* Wait for one signal, and forward it */
		if (wait_and_forward_signal(&parent_sigset, child_pid)) {
			return 1;
		}

		/* Now, reap zombies */
		if (reap_zombies(child_pid, &child_exitcode)) {
			// Oops!
			return 1;
		}

		if (child_exitcode != -1) {
			PRINT_TRACE("Child has exited. Exiting");
			return child_exitcode;
		}
	}
}
