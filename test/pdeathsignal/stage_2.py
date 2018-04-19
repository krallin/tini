#!/usr/bin/env python
from __future__ import print_function

import os
import sys
import signal
import time


def main():
    ret = sys.argv[2]

    def handler(*args):
        with open(ret, "w") as f:
            f.write("ok")
        sys.exit(0)

    signal.signal(signal.SIGUSR1, handler)
    pid = int(sys.argv[1])

    os.kill(pid, signal.SIGKILL)
    time.sleep(5)

if __name__ == "__main__":
    main()
