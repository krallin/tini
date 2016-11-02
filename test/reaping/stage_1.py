#!/usr/bin/env python
from __future__ import print_function
import os
import sys
import subprocess
import time

import psutil


def in_group_or_reaped(pid):
    try:
        return os.getpgid(pid) == os.getpgid(0)
    except OSError:
        return True


def main():
    stage_2 = os.path.join(os.path.dirname(__file__), "stage_2.py")
    subprocess.Popen([stage_2]).wait()

    # In tests, we assume this process is the direct child of init
    this_process = psutil.Process(os.getpid())
    init_process = this_process.parent()

    print("Reaping test: stage_1 is pid{0}, init is pid{1}".format(
        this_process.pid, init_process.pid))

    # The only child PID that should persist is this one.
    expected_pids = [this_process.pid]

    print("Expecting pids to remain: {0}".format(
        ", ".join(str(pid) for pid in expected_pids)))

    while 1:
        pids = [p.pid for p in init_process.children(recursive=True)]
        print("Has pids: {0}".format(", ".join(str(pid) for pid in pids)))
        for pid in pids:
            assert in_group_or_reaped(pid), "Child had unexpected pgid"
        if set(pids) == set(expected_pids):
            break
        time.sleep(1)

    # Now, check if there are any zombies. For each of the potential zombies,
    # we check that the pgid is ours.  NOTE: We explicitly test that this test
    # fails if subreaping is disabled, so we can be confident this doesn't turn
    # a failure into a success.
    for process in psutil.process_iter():
        if process.pid == this_process.pid:
            continue
        if not in_group_or_reaped(process.pid):
            continue
        print("Not reaped: pid {0}: {1}".format(process.pid, process.name()))
        sys.exit(1)

    sys.exit(0)


if __name__ == "__main__":
    main()
