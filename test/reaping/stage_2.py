#!/usr/bin/env python
from __future__ import print_function
import subprocess
import os
import random


if __name__ == "__main__":
    # Spawn lots of process
    for i in range(0, 100):
        cmd = ["sleep", str(1 + i % 2 + random.random())]
        proc = subprocess.Popen(cmd)
