#!/usr/bin/env python
#coding:utf-8
import os
import sys
import signal
import subprocess


def main():
    src = os.environ["SOURCE_DIR"]
    build = os.environ["BUILD_DIR"]

    proxy = os.path.join(src, "test", "subreaper-proxy.py")
    tini = os.path.join(build, "tini")

    # Run the reaping test
    print "Running reaping test"
    p = subprocess.Popen([proxy, tini, "--", os.path.join(src, "test", "reaping", "stage_1.py")])
    ret = p.wait()
    assert ret == 0, "Reaping test failed!"

    # Run the signals test
    for signame in "SIGINT", "SIGTERM":
        print "running signal test for: {0}".format(signame)
        p = subprocess.Popen([proxy, tini, "--", os.path.join(src, "test", "signals", "test.py")])
        sig = getattr(signal, signame)
        p.send_signal(sig)
        ret = p.wait()
        assert ret == - sig, "Signals test failed!"


if __name__ == "__main__":
    main()
