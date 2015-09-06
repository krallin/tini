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

    subreaper_support = bool(int(os.environ["FORCE_SUBREAPER"]))

    tests = [([proxy, tini, "--"], {}),]

    if subreaper_support:
        tests.extend([
            ([tini, "-s", "--"], {}),
            ([tini, "--"], {"TINI_SUBREAPER": ""}),
            ])

    for target, env in tests:
        # Run the reaping test
        print "Running reaping test ({0} with env {1})".format(" ".join(target), env)
        p = subprocess.Popen(target + [os.path.join(src, "test", "reaping", "stage_1.py")],
                             env=dict(os.environ, **env),
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        out, err = p.communicate()

        if subreaper_support:
            # If subreaper support sin't available, Tini won't looku p its subreaper bit
            # and will output the error message here.
            assert "zombie reaping won't work" not in err, "Warning message was output!"
        ret = p.wait()
        assert ret == 0, "Reaping test failed!"


        # Run the signals test
        for signame in "SIGINT", "SIGTERM":
            print "running signal test for: {0} ({1} with env {2})".format(signame, " ".join(target), env)
            p = subprocess.Popen(target + [os.path.join(src, "test", "signals", "test.py")], env=dict(os.environ, **env))
            sig = getattr(signal, signame)
            p.send_signal(sig)
            ret = p.wait()
            assert ret == - sig, "Signals test failed!"

    # Run failing test
    print "Running failing test"
    p = subprocess.Popen([tini, "--", os.path.join(src, "test", "reaping", "stage_1.py")], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = p.communicate()
    assert "zombie reaping won't work" in err, "No warning message was output!"
    ret = p.wait()
    assert ret == 1, "Reaping test succeeded (it should have failed)!"

    print "---------------------------"
    print "All done, tests as expected"
    print "---------------------------"


if __name__ == "__main__":
    main()
