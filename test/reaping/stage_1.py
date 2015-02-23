#!/usr/bin/env python3
import os
import subprocess
import time


if __name__ == "__main__":
    p = subprocess.Popen([os.path.join(os.path.dirname(__file__), "stage_2.py")])
    p.wait()

    # These are the only PIDs that should remain if the system is well-behaved:
    # - This process
    # - Init
    expected_pids = [1, os.getpid()]

    print("Expecting pids to remain: {0}".format(", ".join(str(pid) for pid in expected_pids)))

    while 1:
        pids = [pid for pid in os.listdir('/proc') if pid.isdigit()]
        print("Has pids: {0}".format(", ".join(pids)))
        if set(int(pid) for pid in pids) == set(expected_pids):
            break
        time.sleep(1)


