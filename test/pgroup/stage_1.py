#!/usr/bin/env python
import os
import subprocess
import signal


def reset_sig_handler():
    signal.signal(signal.SIGUSR1, signal.SIG_DFL)


if __name__ == "__main__":
    signal.signal(signal.SIGUSR1, signal.SIG_IGN)
    p = subprocess.Popen(
            ["sleep", "1000"],
            preexec_fn=reset_sig_handler
        )
    p.wait()

