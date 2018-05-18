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
import itertools
import tempfile

DEVNULL = open(os.devnull, 'wb')

SIGNUM_TO_SIGNAME = dict((v, k) for k,v in signal.__dict__.items() if re.match("^SIG[A-Z]+$", k))


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

    args_disabled = os.environ.get("MINIMAL")

    proxy = os.path.join(src, "test", "subreaper-proxy.py")
    tini = os.path.join(build, "tini")

    subreaper_support = bool(int(os.environ["FORCE_SUBREAPER"]))

    # Run the exit code test. We use POSIXLY_CORRECT here to not need --
    # until that's the default in Tini anyways.
    if not args_disabled:
        print "Running exit code test for {0}".format(tini)
        for code in range(0, 256):
            p = subprocess.Popen(
                [tini, '-e', str(code), '--', 'sh', '-c', "exit {0}".format(code)],
                stdout=DEVNULL, stderr=DEVNULL
            )
            ret = p.wait()
            assert ret == 0, "Inclusive exit code test failed for %s, exit: %s" % (code, ret)

            other_codes = [x for x in range(0, 256) if x != code]
            args = list(itertools.chain(*[['-e', str(x)] for x in other_codes]))

            p = subprocess.Popen(
                [tini] + args + ['sh', '-c', "exit {0}".format(code)],
                env=dict(os.environ, POSIXLY_CORRECT="1"),
                stdout=DEVNULL, stderr=DEVNULL
            )
            ret = p.wait()
            assert ret == code, "Exclusive exit code test failed for %s, exit: %s" % (code, ret)

    tests = [([proxy, tini], {}),]

    if subreaper_support:
        if not args_disabled:
            tests.append(([tini, "-s"], {}))
        tests.append(([tini], {"TINI_SUBREAPER": ""}))

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
        assert "Reaped zombie process with pid=" not in err, "Warning message was output!"
        assert ret == 0, "Reaping test failed!\nOUT: %s\nERR: %s" % (out, err)


        if not args_disabled:
            print "Running reaping display test ({0} with env {1})".format(" ".join(target), env)
            p = subprocess.Popen(target + ["-w", os.path.join(src, "test", "reaping", "stage_1.py")],
                                env=dict(os.environ, **env),
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)

            out, err = p.communicate()
            ret = p.wait()
            assert "Reaped zombie process with pid=" in err, "Warning message was output!"
            assert ret == 0, "Reaping display test failed!\nOUT: %s\nERR: %s" % (out, err)


        # Run the signals test
        for signum in [signal.SIGTERM, signal.SIGUSR1, signal.SIGUSR2]:
            print "running signal test for: {0} ({1} with env {2})".format(signum, " ".join(target), env)
            p = subprocess.Popen(target + [os.path.join(src, "test", "signals", "test.py")], env=dict(os.environ, **env))
            busy_wait(lambda: len(psutil.Process(p.pid).children(recursive=True)) > 1, 10)
            p.send_signal(signum)
            ret = p.wait()
            assert ret == 128 + signum, "Signals test failed (ret was {0}, expected {1})".format(ret, 128 + signum)


    # Run the process group test
    # This test has Tini spawn a process that ignores SIGUSR1 and spawns a child that doesn't (and waits on the child)
    # We send SIGUSR1 to Tini, and expect the grand-child to terminate, then the child, and then Tini.
    if not args_disabled:
        print "Running process group test (arguments)"
        p = subprocess.Popen([tini, '-g', os.path.join(src, "test", "pgroup", "stage_1.py")], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        busy_wait(lambda: len(psutil.Process(p.pid).children(recursive=True)) == 2, 10)
        p.send_signal(signal.SIGUSR1)
        busy_wait(lambda: p.poll() is not None, 10)

    print "Running process group test (environment variable)"
    p = subprocess.Popen(
        [tini, os.path.join(src, "test", "pgroup", "stage_1.py")],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        env=dict(os.environ, TINI_KILL_PROCESS_GROUP="1")
    )

    busy_wait(lambda: len(psutil.Process(p.pid).children(recursive=True)) == 2, 10)
    p.send_signal(signal.SIGUSR1)
    busy_wait(lambda: p.poll() is not None, 10)

    # Run failing test. Force verbosity to 1 so we see the subreaper warning
    # regardless of whether MINIMAL is set.
    print "Running zombie reaping failure test (Tini should warn)"
    p = subprocess.Popen(
        [tini, os.path.join(src, "test", "reaping", "stage_1.py")],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        env={'TINI_VERBOSITY': '1'}
    )
    out, err = p.communicate()
    assert "zombie reaping won't work" in err, "No warning message was output!"
    ret = p.wait()
    assert ret == 1, "Reaping test succeeded (it should have failed)!"

    # Test that the signals are properly in place here.
    print "Running signal configuration test"

    p = subprocess.Popen([os.path.join(build, "sigconf-test"), tini, "cat", "/proc/self/status"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
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

    # Test parent death signal handling.
    if not args_disabled:
        print "Running parent death signal test"
        f = tempfile.NamedTemporaryFile()
        try:
            p = subprocess.Popen(
                [os.path.join(src, "test", "pdeathsignal", "stage_1.py"), tini, f.name],
                stdout=DEVNULL, stderr=DEVNULL
            )
            p.wait()

            busy_wait(lambda: open(f.name).read() == "ok", 10)
        finally:
            f.close()

    print "---------------------------"
    print "All done, tests as expected"
    print "---------------------------"


if __name__ == "__main__":
    main()
