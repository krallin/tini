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
#include "vendor/libcperciva/getopt.h"

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

typedef struct {
	char* (*child_argv_ptr)[];
	char* (*pre_argv_ptr)[];
	char* (*post_argv_ptr)[];
} exec_configuration_t;


#ifdef PR_SET_CHILD_SUBREAPER
#define HAS_SUBREAPER 1
#define SUBREAPER_ENV_VAR "TINI_SUBREAPER"
#else
#define HAS_SUBREAPER 0
#endif


/* TODO - At some points those have to stop being globals */
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


int compute_exitcode(const int status) {
	/* Returns a sh-style exit code for status, and -1 if no exit code could
	 * be determined
	 */

	if (WIFEXITED(status)) {
		/* Our process exited normally. */
		PRINT_INFO("Child exited normally (with status '%i')", WEXITSTATUS(status));
		return WEXITSTATUS(status);
	}

	if (WIFSIGNALED(status)) {
		/* Our process was terminated by a signal. Emulate what sh / bash
		 * would do, which is to return 128 + signal number.
		 */
		PRINT_INFO("Child exited with signal (with signal '%s')", strsignal(WTERMSIG(status)));
		return 128 + WTERMSIG(status);
	}

	PRINT_FATAL("Child exited for unknown reason");
	return 1;
}


int restore_signals(const signal_configuration_t* const signal_conf_ptr) {
	if (sigprocmask(SIG_SETMASK, signal_conf_ptr->sigmask_ptr, NULL)) {
		PRINT_FATAL("Restoring child signal mask failed: '%s'", strerror(errno));
		return 1;
	}

	if (sigaction(SIGTTIN, signal_conf_ptr->sigttin_action_ptr, NULL)) {
		PRINT_FATAL("Restoring SIGTTIN handler failed: '%s'", strerror((errno)));
		return 1;
	}

	if (sigaction(SIGTTOU, signal_conf_ptr->sigttou_action_ptr, NULL)) {
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


int spawn(const signal_configuration_t* const signal_conf_ptr, char* const argv[], int* const child_pid_ptr) {
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
		if (restore_signals(signal_conf_ptr)) {
			return 1;
		}

		execvp(argv[0], argv);
		PRINT_FATAL("Executing child process '%s' failed: '%s'", argv[0], strerror(errno));
		_exit(EXIT_FAILURE);
	} else {
		// Parent
		PRINT_INFO("Spawned child process '%s' with pid '%i'", argv[0], pid);
		*child_pid_ptr = pid;
		return 0;
	}
}


int spawn_and_wait(const signal_configuration_t* const signal_conf_ptr, char* const argv[]) {
	pid_t child_pid;

	/* Run pre command */
	if (spawn(signal_conf_ptr, argv, &child_pid)) {
		return 1;
	}

	int status;
	while (1) {
		if (waitpid(child_pid, &status, 0) < 0) {
			if (errno == EINTR) {
				continue;
			}
			PRINT_FATAL("Unexpected error in waitpid: '%s'", strerror(errno));
			return 1;
		}

		return compute_exitcode(status);
	}
}


void print_usage(char* const name, FILE* const file) {
	fprintf(file, "%s (version %s%s)\n", basename(name), TINI_VERSION, TINI_GIT);
	fprintf(file, "Usage: %s [OPTIONS] -- PROGRAM [ARGS]\n\n", basename(name));
	fprintf(file, "Execute a program under the supervision of a valid init process (%s)\n\n", basename(name));
	fprintf(file, "  -h, --help: Show this help message and exit.\n");
#if HAS_SUBREAPER
	fprintf(file, "  -s, --subreaper: Register as a process subreaper (requires Linux >= 3.4).\n");
#endif
	fprintf(file, "  -v: Generate more verbose output. Repeat up to 3 times.\n");
	fprintf(file, "  -g, --group: Send signals to the child's process group.\n");
	fprintf(file, "  --pre OTHER_PROGRAM: OTHER_PROGRAM to execute prior to launching PROGRAM.\n");
	fprintf(file, "  --post OTHER_PROGRAM: OTHER_PROGRAM to execute after PROGRAM terminates.\n");
	fprintf(file, "\n");
}


int wrap_arg(char* (**argv_ptr_ptr)[], const char* const value_ptr) {
	/* Unfortunately, execvp expects an array of constant pointers
	 * to mutable strings (although it doesn't change them), which
	 * means we need to not only allocate an array here, but also
	 * copy the optarg over.
	 */

	/* Allocate an array for the argv. We *could* use constant size here
	 * since it's always 2, but we might as well be consistent with what
	 * we do for the child
	 */
	*argv_ptr_ptr = calloc(2, sizeof(char*));
	if (*argv_ptr_ptr == NULL) {
		PRINT_FATAL("Failed to allocate memory for pre argv: '%s'", strerror(errno));
		return 1;
	}

	/* Copy over the optarg into the array, and NULL-terminate the array
	 * for execvp.
	 */
	(**argv_ptr_ptr)[0] = strdup(value_ptr);
	(**argv_ptr_ptr)[1] = NULL;

	if ((**argv_ptr_ptr)[0] == NULL) {
		PRINT_FATAL("Failed to allocate memory for pre / post argv[0]: '%s'", strerror(errno));
		return 1;
	}

	return 0;
}


int parse_args(const int argc, char* const argv[], exec_configuration_t* const exec_configuration_ptr, int* const parse_exitcode_ptr) {
	char* name = argv[0];

	const char *ch;
	while ((ch = GETOPT(argc, argv)) != NULL) {
		GETOPT_SWITCH(ch) {
			GETOPT_OPT("-h"):
			GETOPT_OPT("--help"):
				print_usage(name, stdout);
				*parse_exitcode_ptr = 0;
				return 1;
#if HAS_SUBREAPER
			GETOPT_OPT("-s"):
			GETOPT_OPT("--subreaper"):
				subreaper++;
				break;
#endif
			GETOPT_OPT("-v"):
				verbosity++;
				break;
			GETOPT_OPT("-g"):
			GETOPT_OPT("--group"):
				kill_process_group++;
				break;
			GETOPT_OPTARG("--pre"):
				if (wrap_arg(&exec_configuration_ptr->pre_argv_ptr, optarg)) {
					return 1;
				}
				break;
			GETOPT_OPTARG("--post"):
				if (wrap_arg(&exec_configuration_ptr->post_argv_ptr, optarg)) {
					return 1;
				}
				break;
			GETOPT_MISSING_ARG:
				fprintf(stderr, "Missing argument to -%c\n\n", optopt);
				/* FALLTHROUGH */
			GETOPT_DEFAULT:
				print_usage(name, stderr);
				return 1;
		}
	}

	/* Remaining arguments are for the child */
	exec_configuration_ptr->child_argv_ptr = calloc(argc-optind+1, sizeof(char*));
	if (exec_configuration_ptr->child_argv_ptr == NULL) {
		PRINT_FATAL("Failed to allocate memory for child argv: '%s'", strerror(errno));
		return 1;
	}

	int i;
	for (i = 0; i < argc - optind; i++) {
		(*exec_configuration_ptr->child_argv_ptr)[i] = argv[optind+i];
	}
	(*exec_configuration_ptr->child_argv_ptr)[i] = NULL;

	if (i == 0) {
		/* User forgot to provide args! */
		fprintf(stderr, "No program to execute was provided\n\n");
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


int configure_signals(sigset_t* const parent_sigset_ptr, const signal_configuration_t* const signal_conf_ptr) {
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

	if (sigprocmask(SIG_SETMASK, parent_sigset_ptr, signal_conf_ptr->sigmask_ptr)) {
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

	if (sigaction(SIGTTIN, &ign_action, signal_conf_ptr->sigttin_action_ptr)) {
		PRINT_FATAL("Failed to ignore SIGTTIN");
		return 1;
	}

	if (sigaction(SIGTTOU, &ign_action, signal_conf_ptr->sigttou_action_ptr)) {
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
					*child_exitcode_ptr = compute_exitcode(current_status);
					if (*child_exitcode_ptr == -1) {
						// Unknown exit reason.
						return 1;
					}
				}
				// Check if other childs have to be reaped before we exit.
				continue;
		}

		/* If we make it here, that's because we did not continue in the switch case, or had no children to wait. */
		break;
	}

	return 0;
}


int main(int argc, char *argv[]) {
	int retval = 10;  // 10 to indicate internal error.

	/* Parse command line arguments */
	int parse_exitcode = 1;   // By default, we exit with 1 if parsing fails.
	exec_configuration_t exec_conf = {
		.child_argv_ptr = NULL,
		.pre_argv_ptr = NULL,
		.post_argv_ptr = NULL,
	};


	if (parse_args(argc, argv, &exec_conf, &parse_exitcode)) {
		retval = parse_exitcode;
		goto exit;
	}

	/* Parse environment */
	if ((retval = parse_env())) {
		goto exit;
	}

	/* Configure signals */
	sigset_t parent_sigset, child_sigset;
	struct sigaction sigttin_action, sigttou_action;
	memset(&sigttin_action, 0, sizeof sigttin_action);
	memset(&sigttou_action, 0, sizeof sigttou_action);

	signal_configuration_t signal_conf = {
		.sigmask_ptr = &child_sigset,
		.sigttin_action_ptr = &sigttin_action,
		.sigttou_action_ptr = &sigttou_action,
	};

	if ((retval = configure_signals(&parent_sigset, &signal_conf))) {
		goto exit;
	}

#if HAS_SUBREAPER
	/* If available and requested, register as a subreaper */
	if ((retval = register_subreaper())) {
		goto exit;
	};
#endif

	/* Are we going to reap zombies properly? If not, warn. */
	reaper_check();

	/* Pre command */
	if (exec_conf.pre_argv_ptr != NULL) {
		PRINT_INFO("Spawning pre: %s", **exec_conf.pre_argv_ptr);
		if ((retval = spawn_and_wait(&signal_conf, *exec_conf.pre_argv_ptr))) {
			goto post_then_exit;
		}
	}

	/* Spawn the child */
	PRINT_INFO("Spawning main: %s", **exec_conf.child_argv_ptr);
	pid_t child_pid;
	if ((retval = spawn(&signal_conf, *exec_conf.child_argv_ptr, &child_pid))) {
		goto post_then_exit;
	}


	/* Main loop, forward signals and wait for the child to exit, all the while reaping zombies */
	int child_exitcode = -1;  // This isn't a valid exitcode, and lets us tell whether the child has exited.
	while (1) {
		/* Wait for one signal, and forward it */
		if ((retval = wait_and_forward_signal(&parent_sigset, child_pid))) {
			goto post_then_exit;
		}

		/* Now, reap zombies */
		if ((retval = reap_zombies(child_pid, &child_exitcode))) {
			goto post_then_exit;
		}

		if (child_exitcode != -1) {
			PRINT_TRACE("Exiting: child has exited");
			retval = child_exitcode;
			goto post_then_exit;
		}
	}

post_then_exit:
	/* Post command */
	if (exec_conf.post_argv_ptr != NULL) {
		PRINT_INFO("Spawning post: %s", **exec_conf.post_argv_ptr);
		if (spawn_and_wait(&signal_conf, *exec_conf.post_argv_ptr)) {  // Do *not* set the reval here.
			PRINT_WARNING("Post command exited with status != 0");
		}
	}


exit:
	free(exec_conf.child_argv_ptr);

	if (exec_conf.pre_argv_ptr != NULL) {
		free((*exec_conf.pre_argv_ptr)[0]);
		free(exec_conf.pre_argv_ptr);
	}

	if (exec_conf.post_argv_ptr != NULL) {
		free((*exec_conf.post_argv_ptr)[0]);
		free(exec_conf.post_argv_ptr);
	}

	return retval;
}
