#!/usr/bin/env python3
from __future__ import print_function
import subprocess
import os


if __name__ == "__main__":
    # Spawn lots of process
    for i in range(0, 10):
        cmd = ["sleep", str(1 + i % 2)]
        proc = subprocess.Popen(cmd)
