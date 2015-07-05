#!/usr/bin/env python
from __future__ import print_function
import os
import subprocess
import time

import psutil


def main():
    p = subprocess.Popen([os.path.join(os.path.dirname(__file__), "stage_2.py")])
    p.wait()

    # In tests, we assume this process is the direct child of init
    this_process = psutil.Process(os.getpid())
    init_process = this_process.parent()

    print("Reaping test: stage_1 is pid{0}, init is pid{1}".format(this_process.pid, init_process.pid))

    # The only child PID that should persist is this one.
    expected_pids = [this_process.pid]

    print("Expecting pids to remain: {0}".format(", ".join(str(pid) for pid in expected_pids)))

    while 1:
        pids = [p.pid for p in init_process.children(recursive=True)]
        print("Has pids: {0}".format(", ".join(str(pid) for pid in pids)))
        if set(pids) == set(expected_pids):
            break
        time.sleep(1)


if __name__ == "__main__":
    main()
