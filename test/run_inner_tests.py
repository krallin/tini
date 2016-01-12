#!/usr/bin/env python
#coding:utf-8
import os
import sys
import signal
import subprocess
import time
import psutil
import bitmap
import re


SIGNUM_TO_SIGNAME = dict((v, k) for k,v in signal.__dict__.items() if re.match("^SIG[A-Z]+$", k))


def print_section(name):
    print "\n"
    print "*" * len(name)
    print name
    print "*" * len(name)
    print


def busy_wait(condition_callable, timeout):
    checks = 100
    increment = float(timeout) / checks

    for _ in xrange(checks):
        if condition_callable():
            return
        time.sleep(increment)

    assert False, "Condition was never met"


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

    print_section("Running reaping and signal tests")

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
        for signum in [signal.SIGINT, signal.SIGTERM]:
            print "running signal test for: {0} ({1} with env {2})".format(SIGNUM_TO_SIGNAME[signum], " ".join(target), env)
            p = subprocess.Popen(target + [os.path.join(src, "test", "signals", "test.py")], env=dict(os.environ, **env))
            p.send_signal(signum)
            ret = p.wait()
            assert ret == -signum, "Signals test failed (ret was {0}, expected {1})".format(ret, -signum)


    # Run the process group test
    # This test has Tini spawn a process that ignores SIGUSR1 and spawns a child that doesn't (and waits on the child)
    # We send SIGUSR1 to Tini, and expect the grand-child to terminate, then the child, and then Tini.
    print_section("Running process group test")
    p = subprocess.Popen([tini, '-g', '--', os.path.join(src, "test", "pgroup", "stage_1.py")], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    busy_wait(lambda: len(psutil.Process(p.pid).children(recursive=True)) == 2, 10)
    p.send_signal(signal.SIGUSR1)
    busy_wait(lambda: p.poll() is not None, 10)


    # Run failing test
    print_section("Running zombie reaping failure test (Tini should warn)")
    p = subprocess.Popen([tini, "--", os.path.join(src, "test", "reaping", "stage_1.py")], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = p.communicate()
    assert "zombie reaping won't work" in err, "No warning message was output!"
    ret = p.wait()
    assert ret == 1, "Reaping test succeeded (it should have failed)!"


    # Test that the signals are properly in place here.
    print_section("Running signal configuration test")

    p = subprocess.Popen([os.path.join(build, "sigconf-test"), tini, '-g', '--', "cat", "/proc/self/status"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = p.communicate()

    # Extract the signal properties, and add a zero at the end.
    props = [line.split(":") for line in out.splitlines()]
    props = [(k.strip(), v.strip()) for (k, v) in props]
    props = [(k, bitmap.BitMap.fromstring(bin(int(v, 16))[2:].zfill(32))) for (k, v) in props if k in ["SigBlk", "SigIgn", "SigCgt"]]
    props = dict(props)

    # Print actual handling configuration
    for k, bmp in props.items():
        print "{0}: {1}".format(k, ", ".join(["{0} ({1})".format(SIGNUM_TO_SIGNAME[n+1], n+1) for n in bmp.nonzero()]))

    for signal_set_name, signals_to_test_for in [
        ("SigIgn", [signal.SIGTTOU, signal.SIGSEGV, signal.SIGINT,]),
        ("SigBlk", [signal.SIGTTIN, signal.SIGILL,  signal.SIGTERM,]),
    ]:
        for signum in signals_to_test_for:
            # Use signum - 1 because the bitmap is 0-indexed but represents signals strting at 1
            assert (signum - 1) in props[signal_set_name].nonzero(), "{0} ({1}) is missing in {2}!".format(SIGNUM_TO_SIGNAME[signum], signum, signal_set_name)

    print_section("Running pre / post tests")
    cmd = [
        tini,
        "--pre", os.path.join(src, "test", "pre-post", "pre.sh"),
        "--post", os.path.join(src, "test", "pre-post", "post.sh"),
        "--", os.path.join(src, "test", "pre-post", "main.sh")
    ]

    for (pre_exit, pre_runs, post_exit, post_runs, main_exit, main_runs) in [
            (os.EX_IOERR, True, os.EX_OK, True,    os.EX_OK, False),
            (os.EX_OK, True,    os.EX_OK, True,    os.EX_IOERR, True),
            (os.EX_OK, True,    os.EX_IOERR, True, os.EX_OK, True),
    ]:
        env = {
                "PRE_EXITCODE": str(pre_exit),
                "POST_EXITCODE": str(post_exit),
                "MAIN_EXITCODE": str(main_exit),
        }
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=dict(os.environ, **env))
        out, err = p.communicate()
        ret = p.wait()

        for should_it_run, expected_string in [(pre_runs, "PRE"), (post_runs, "POST"), (main_runs, "MAIN")]:
            did_it_run = ("{0} is running".format(expected_string) in out)
            assert should_it_run == did_it_run, "{0} unexpectedly {1} with env {2}".format(expected_string, "ran" if did_it_run else "did not run", env)

        expected_exit = main_exit if main_runs else pre_exit
        assert ret == expected_exit, "Unexpected exit code for {0}: {1} (expected {2})".format(" ".join(cmd), ret, expected)

    for cmd, expected in [("pre", 1), ("post", 0)]:
        p = subprocess.Popen([tini, "--{0}".format(cmd), "/does/not/exist", "--", "ls"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        ret = p.wait()
        assert ret == expected, "Unexpected exit code with invalid --{0}: {1} (expected {2})".format(cmd, ret, expected)

    print_section("All done, tests as expected")


if __name__ == "__main__":
    main()
