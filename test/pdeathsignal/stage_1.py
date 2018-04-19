#!/usr/bin/env python
from __future__ import print_function

import os
import sys
import subprocess


def main():
    pid = os.getpid()

    tini = sys.argv[1]
    ret = sys.argv[2]
    stage_2 = os.path.join(os.path.dirname(__file__), "stage_2.py")

    cmd = [
        tini,
        "-vvv",
        "-p",
        "SIGUSR1",
        "--",
        stage_2,
        str(pid),
        ret
    ]

    subprocess.Popen(cmd).wait()

if __name__ == "__main__":
    main()
