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
#include "tiniLicense.h"

#define PRINT_FATAL(...)                         fprintf(stderr, "[FATAL tini (%i)] ", getpid()); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define PRINT_WARNING(...)  if (verbosity > 0) { fprintf(stderr, "[WARN  tini (%i)] ", getpid()); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
#define PRINT_INFO(...)     if (verbosity > 1) { fprintf(stdout, "[INFO  tini (%i)] ", getpid()); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define PRINT_DEBUG(...)    if (verbosity > 2) { fprintf(stdout, "[DEBUG tini (%i)] ", getpid()); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define PRINT_TRACE(...)    if (verbosity > 3) { fprintf(stdout, "[TRACE tini (%i)] ", getpid()); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }

#define ARRAY_LEN(x)  (sizeof(x) / sizeof(x[0]))

typedef struct {
   sigset_t* const sigmask_ptr;
   struct sigaction* const sigttin_action_ptr;
   struct sigaction* const sigttou_action_ptr;
} signal_configuration_t;


#ifdef PR_SET_CHILD_SUBREAPER
#define HAS_SUBREAPER 1
#define OPT_STRING "hsvgl"
#define SUBREAPER_ENV_VAR "TINI_SUBREAPER"
#else
#define HAS_SUBREAPER 0
#define OPT_STRING "hvgl"
#endif

#define TINI_VERSION_STRING "tini version " TINI_VERSION TINI_GIT


#if HAS_SUBREAPER
static unsigned int subreaper = 0;
#endif
static unsigned int verbosity = 1;
static unsigned int kill_process_group = 0;

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

int restore_signals(const signal_configuration_t* const sigconf_ptr) {
	if (sigprocmask(SIG_SETMASK, sigconf_ptr->sigmask_ptr, NULL)) {
		PRINT_FATAL("Restoring child signal mask failed: '%s'", strerror(errno));
		return 1;
	}

	if (sigaction(SIGTTIN, sigconf_ptr->sigttin_action_ptr, NULL)) {
		PRINT_FATAL("Restoring SIGTTIN handler failed: '%s'", strerror((errno)));
		return 1;
	}

	if (sigaction(SIGTTOU, sigconf_ptr->sigttou_action_ptr, NULL)) {
		PRINT_FATAL("Restoring SIGTTOU handler failed: '%s'", strerror((errno)));
		return 1;
	}

	return 0;
}

int isolate_child() {
	// Put the child into a new process group.
	if (setpgid(0, 0) < 0) {
		PRINT_FATAL("setpgid failed: '%s'", strerror(errno));
		return 1;
	}

	// If there is a tty, allocate it to this new process group. We
	// can do this in the child process because we're blocking
	// SIGTTIN / SIGTTOU.

	// Doing it in the child process avoids a race condition scenario
	// if Tini is calling Tini (in which case the grandparent may make the
	// parent the foreground process group, and the actual child ends up...
	// in the background!)
	if (tcsetpgrp(STDIN_FILENO, getpgrp())) {
		if (errno == ENOTTY) {
			PRINT_DEBUG("tcsetpgrp failed: no tty (ok to proceed)")
		} else {
			PRINT_FATAL("tcsetpgrp failed: '%s'", strerror(errno));
			return 1;
		}
	}

	return 0;
}


int spawn(const signal_configuration_t* const sigconf_ptr, char* const argv[], int* const child_pid_ptr) {
	pid_t pid;

	// TODO: check if tini was a foreground process to begin with (it's not OK to "steal" the foreground!")

	pid = fork();
	if (pid < 0) {
		PRINT_FATAL("Fork failed: '%s'", strerror(errno));
		return 1;
	} else if (pid == 0) {

		// Put the child in a process group and make it the foreground process if there is a tty.
		if (isolate_child()) {
			return 1;
		}

		// Restore all signal handlers to the way they were before we touched them.
		if (restore_signals(sigconf_ptr)) {
			return 1;
		}

		execvp(argv[0], argv);
	    	// execvp will only return on an error so make sure that we check the errno
		// and exit with the correct return status for the error that we encountered
		int status = 1;
		switch errno {
		case ENOENT:
			status = 127;
			break;
            	case ENOEXEC:
		case EACCES:
			status = 126;
			break;
		}
		fprintf(stderr, "%s\n", strerror(errno));
		return status;
	} else {
		// Parent
		PRINT_INFO("Spawned child process '%s' with pid '%i'", argv[0], pid);
		*child_pid_ptr = pid;
		return 0;
	}
}

void print_usage(char* const name, FILE* const file) {
	fprintf(file, "%s (%s)\n", basename(name), TINI_VERSION_STRING);
	fprintf(file, "Usage: %s [OPTIONS] PROGRAM -- [ARGS]\n\n", basename(name));
	fprintf(file, "Execute a program under the supervision of a valid init process (%s)\n\n", basename(name));
	fprintf(file, "  -h: Show this help message and exit.\n");
#if HAS_SUBREAPER
	fprintf(file, "  -s: Register as a process subreaper (requires Linux >= 3.4).\n");
#endif
	fprintf(file, "  -v: Generate more verbose output. Repeat up to 3 times.\n");
	fprintf(file, "  -g: Send signals to the child's process group.\n");
	fprintf(file, "  -l: Show license and exit.\n");
	fprintf(file, "\n");
}

void print_license(FILE* const file) {
	fwrite(LICENSE, sizeof(char), LICENSE_len, file);
}


int parse_args(const int argc, char* const argv[], char* (**child_args_ptr_ptr)[], int* const parse_fail_exitcode_ptr) {
	char* name = argv[0];

	// We handle --version if it's the *only* argument provided.
	if (argc == 2 && strcmp("--version", argv[1]) == 0) {
		*parse_fail_exitcode_ptr = 0;
		fprintf(stdout, "%s\n", TINI_VERSION_STRING);
		return 1;
	}

#if TINI_NO_ARGS
	*parse_fail_exitcode_ptr = 0;
#else
	int c;
	while ((c = getopt(argc, argv, OPT_STRING)) != -1) {
		switch (c) {
			case 'h':
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

			case 'l':
				print_license(stdout);
				*parse_fail_exitcode_ptr = 0;
				return 1;

			case '?':
				print_usage(name, stderr);
				return 1;
			default:
				/* Should never happen */
				return 1;
		}
	}
#endif

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


int configure_signals(sigset_t* const parent_sigset_ptr, const signal_configuration_t* const sigconf_ptr) {
	/* Block all signals that are meant to be collected by the main loop */
	if (sigfillset(parent_sigset_ptr)) {
		PRINT_FATAL("sigfillset failed: '%s'", strerror(errno));
		return 1;
	}

	// These ones shouldn't be collected by the main loop
	uint i;
	int signals_for_tini[] = {SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGABRT, SIGTRAP, SIGSYS, SIGTTIN, SIGTTOU};
	for (i = 0; i < ARRAY_LEN(signals_for_tini); i++) {
		if (sigdelset(parent_sigset_ptr, signals_for_tini[i])) {
			PRINT_FATAL("sigdelset failed: '%i'", signals_for_tini[i]);
			return 1;
		}
	}

	if (sigprocmask(SIG_SETMASK, parent_sigset_ptr, sigconf_ptr->sigmask_ptr)) {
		PRINT_FATAL("sigprocmask failed: '%s'", strerror(errno));
		return 1;
	}

	// Handle SIGTTIN and SIGTTOU separately. Since Tini makes the child process group
	// the foreground process group, there's a chance Tini can end up not controlling the tty.
	// If TOSTOP is set on the tty, this could block Tini on writing debug messages. We don't
	// want that. Ignore those signals.
	struct sigaction ign_action;
	memset(&ign_action, 0, sizeof ign_action);

	ign_action.sa_handler = SIG_IGN;
	sigemptyset(&ign_action.sa_mask);

	if (sigaction(SIGTTIN, &ign_action, sigconf_ptr->sigttin_action_ptr)) {
		PRINT_FATAL("Failed to ignore SIGTTIN");
		return 1;
	}

	if (sigaction(SIGTTOU, &ign_action, sigconf_ptr->sigttou_action_ptr)) {
		PRINT_FATAL("Failed to ignore SIGTTOU");
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

	/* Configure signals */
	sigset_t parent_sigset, child_sigset;
	struct sigaction sigttin_action, sigttou_action;
	memset(&sigttin_action, 0, sizeof sigttin_action);
	memset(&sigttou_action, 0, sizeof sigttou_action);

	signal_configuration_t child_sigconf = {
		.sigmask_ptr = &child_sigset,
		.sigttin_action_ptr = &sigttin_action,
		.sigttou_action_ptr = &sigttou_action,
	};

	if (configure_signals(&parent_sigset, &child_sigconf)) {
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
	int spawn_ret = spawn(&child_sigconf, *child_args_ptr, &child_pid);
	if (spawn_ret) {
		return spawn_ret;
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
