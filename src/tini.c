/* See LICENSE file for copyright and license details. */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "tiniConfig.h"

#define PRINT_FATAL(...)    fprintf(stderr, "[FATAL] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define PRINT_WARNING(...)  if (verbosity > 0) { fprintf(stderr, "[WARN ] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
#define PRINT_INFO(...)     if (verbosity > 1) { fprintf(stdout, "[INFO ] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define PRINT_DEBUG(...)    if (verbosity > 2) { fprintf(stdout, "[DEBUG] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define PRINT_TRACE(...)    if (verbosity > 3) { fprintf(stdout, "[TRACE] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }

#define ARRAY_LEN(x)  (sizeof(x) / sizeof(x[0]))


#ifdef PR_SET_CHILD_SUBREAPER
#define HAS_SUBREAPER 1
#define OPT_STRING "hsvg"
#define SUBREAPER_ENV_VAR "TINI_SUBREAPER"
#else
#define HAS_SUBREAPER 0
#define OPT_STRING "hvg"
#endif


#if HAS_SUBREAPER
static int subreaper = 0;
#endif
static int verbosity = 1;
static int kill_process_group;

static struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };

static const char reaper_warning[] = "Tini is not running as PID 1 "
#if HAS_SUBREAPER
       "and isn't registered as a child subreaper"
#endif
       ".\n\
        Zombie processes will not be re-parented to Tini, so zombie reaping won't work.\n\
        To fix the problem, "
#if HAS_SUBREAPER
        "use -s or set the environment variable " SUBREAPER_ENV_VAR " to register Tini as a child subreaper, or "
#endif
        "run Tini as PID 1.";


int spawn(const sigset_t* const child_sigset_ptr, char* const argv[], int* const child_pid_ptr) {
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		PRINT_FATAL("Fork failed: '%s'", strerror(errno));
		return 1;
	} else if (pid == 0) {
		// Child
		if (sigprocmask(SIG_SETMASK, child_sigset_ptr, NULL)) {
			PRINT_FATAL("Setting child signal mask failed: '%s'", strerror(errno));
			return 1;
		}

		// Put the child into a new process group
		if (setpgid(0, 0) < 0) {
			PRINT_FATAL("setpgid failed: '%s'", strerror(errno));
			return 1;
		}

		execvp(argv[0], argv);
		PRINT_FATAL("Executing child process '%s' failed: '%s'", argv[0], strerror(errno));
		return 1;
	} else {
		// Parent
		PRINT_INFO("Spawned child process '%s' with pid '%i'", argv[0], pid);
		*child_pid_ptr = pid;
		return 0;
	}
}


void print_usage(char* const name, FILE* const file) {
	fprintf(file, "%s (version %s%s)\n", basename(name), TINI_VERSION, TINI_GIT);
	fprintf(file, "Usage: %s [OPTIONS] PROGRAM -- [ARGS]\n\n", basename(name));
	fprintf(file, "Execute a program under the supervision of a valid init process (%s)\n\n", basename(name));
	fprintf(file, "  -h: Show this help message and exit.\n");
#if HAS_SUBREAPER
	fprintf(file, "  -s: Register as a process subreaper (requires Linux >= 3.4).\n");
#endif
	fprintf(file, "  -v: Generate more verbose output. Repeat up to 3 times.\n");
	fprintf(file, "  -g: Send signals to the child's process group.\n");
	fprintf(file, "\n");
}


int parse_args(const int argc, char* const argv[], char* (**child_args_ptr_ptr)[], int* const parse_fail_exitcode_ptr) {
	char* name = argv[0];

	int c;
	while ((c = getopt(argc, argv, OPT_STRING)) != -1) {
		switch (c) {
			case 'h':
				/* TODO - Shouldn't cause exit with -1 ..*/
				print_usage(name, stdout);
				*parse_fail_exitcode_ptr = 0;
				return 1;
#if HAS_SUBREAPER
			case 's':
				subreaper++;
				break;
#endif
			case 'v':
				verbosity++;
				break;

			case 'g':
				kill_process_group++;
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
		PRINT_FATAL("Failed to allocate memory for child args: '%s'", strerror(errno));
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

int parse_env() {
#if HAS_SUBREAPER
	if (getenv(SUBREAPER_ENV_VAR) != NULL) {
		subreaper++;
	}
#endif
	return 0;
}


#if HAS_SUBREAPER
int register_subreaper () {
	if (subreaper > 0) {
		if (prctl(PR_SET_CHILD_SUBREAPER)) {
			if (errno == EINVAL) {
				PRINT_FATAL("PR_SET_CHILD_SUBREAPER is unavailable on this platform. Are you using Linux >= 3.4?")
			} else {
				PRINT_FATAL("Failed to register as child subreaper: %s", strerror(errno))
			}
			return 1;
		} else {
			PRINT_TRACE("Registered as child subreaper");
		}
	}
	return 0;
}
#endif


void reaper_check () {
	/* Check that we can properly reap zombies */
#if HAS_SUBREAPER
	int bit = 0;
#endif

	if (getpid() == 1) {
		return;
	}

#if HAS_SUBREAPER
	if (prctl(PR_GET_CHILD_SUBREAPER, &bit)) {
		PRINT_DEBUG("Failed to read child subreaper attribute: %s", strerror(errno));
	} else if (bit == 1) {
		return;
	}
#endif

	PRINT_WARNING(reaper_warning);
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
			default:
				PRINT_FATAL("Unexpected error in sigtimedwait: '%s'", strerror(errno));
				return 1;
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
				if (kill(kill_process_group ? -child_pid : child_pid, sig.si_signo)) {
					if (errno == ESRCH) {
						PRINT_WARNING("Child was dead when forwarding signal");
					} else {
						PRINT_FATAL("Unexpected error when forwarding signal: '%s'", strerror(errno));
						return 1;
					}
				}
				break;
		}
	}

	return 0;
}

int reap_zombies(const pid_t child_pid, int* const child_exitcode_ptr) {
	pid_t current_pid;
	int current_status;

	while (1) {
		current_pid = waitpid(-1, &current_status, WNOHANG);

		switch (current_pid) {

			case -1:
				if (errno == ECHILD) {
					PRINT_TRACE("No child to wait");
					break;
				}
				PRINT_FATAL("Error while waiting for pids: '%s'", strerror(errno));
				return 1;

			case 0:
				PRINT_TRACE("No child to reap");
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
						PRINT_FATAL("Main child exited for unknown reason");
						return 1;
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

	// Those are passed to functions to get an exitcode back.
	int child_exitcode = -1;  // This isn't a valid exitcode, and lets us tell whether the child has exited.
	int parse_exitcode = 1;   // By default, we exit with 1 if parsing fails.

	/* Parse command line arguments */
	char* (*child_args_ptr)[];
	int parse_args_ret = parse_args(argc, argv, &child_args_ptr, &parse_exitcode);
	if (parse_args_ret) {
		return parse_exitcode;
	}

	/* Parse environment */
	if (parse_env()) {
		return 1;
	}

	/* Prepare sigmask */
	sigset_t parent_sigset;
	sigset_t child_sigset;
	if (prepare_sigmask(&parent_sigset, &child_sigset)) {
		return 1;
	}

#if HAS_SUBREAPER
	/* If available and requested, register as a subreaper */
	if (register_subreaper()) {
		return 1;
	};
#endif

	/* Are we going to reap zombies properly? If not, warn. */
	reaper_check();

	/* Go on */
	if (spawn(&child_sigset, *child_args_ptr, &child_pid)) {
		return 1;
	}
	free(child_args_ptr);

	while (1) {
		/* Wait for one signal, and forward it */
		if (wait_and_forward_signal(&parent_sigset, child_pid)) {
			return 1;
		}

		/* Now, reap zombies */
		if (reap_zombies(child_pid, &child_exitcode)) {
			return 1;
		}

		if (child_exitcode != -1) {
			PRINT_TRACE("Exiting: child has exited");
			return child_exitcode;
		}
	}
}
