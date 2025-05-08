#!/usr/bin/env python
import signal
import os
import sys
import time


def sigterm_handler(sig, frame):
    print("SIGTERM received - exiting gracefully")
    sys.exit(0)


def sigusr1_handler(sig, frame):
    print("SIGUSR1 received - exiting")
    sys.exit(1)


def main():
    signal.signal(signal.SIGTERM, sigterm_handler)
    signal.signal(signal.SIGUSR1, sigusr1_handler)
    print("#")
    print("Starting")
    while True:
        time.sleep(1)


if __name__ == "__main__":
    main()
