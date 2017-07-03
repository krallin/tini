#!/usr/bin/env python
import signal
import os


def main():
    signal.signal(signal.SIGTERM, signal.SIG_DFL)
    signal.signal(signal.SIGUSR1, signal.SIG_DFL)
    signal.signal(signal.SIGUSR2, signal.SIG_DFL)
    os.system("sleep 100")

if __name__ == "__main__":
    main()
