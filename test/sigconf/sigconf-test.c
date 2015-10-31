/*
Test program to:
+ Ignore a few signals
+ Block a few signals
+ Exec whatever the test runner asked for
*/

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>


int main(int argc, char *argv[]) {
	// Signals to ignore
	signal(SIGTTOU, SIG_IGN);  // This one should still be in SigIgn (Tini touches it to ignore it, and should restore it)
	signal(SIGSEGV, SIG_IGN);  // This one should still be in SigIgn (Tini shouldn't touch it)
	signal(SIGINT,  SIG_IGN);  // This one should still be in SigIgn (Tini should block it to forward it, and restore it)

	// Signals to block
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGTTIN);  // This one should still be in SigIgn (Tini touches it to ignore it, and should restore it)
	sigaddset(&set, SIGILL);   // This one should still be in SigIgn (Tini shouldn't touch it)
	sigaddset(&set, SIGTERM);  // This one should still be in SigIgn (Tini should block it to forward it, and restore it)
	sigprocmask(SIG_BLOCK, &set, NULL);

	// Run whatever we were asked to run
	execvp(argv[1], argv+1);
}
