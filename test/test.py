#coding:utf-8
import os
import sys
import time
import subprocess
import threading


class Command(object):
    def __init__(self, cmd, fail_cmd, post_cmd=None, post_delay=0):
        self.cmd = cmd
        self.fail_cmd = fail_cmd
        self.post_cmd = post_cmd
        self.post_delay = post_delay
        self.proc = None

    def run(self, timeout=None, retcode=None):
        print "Testing...", self.cmd,
        sys.stdout.flush()

        err = None
        pipe_kwargs = {"stdout": subprocess.PIPE, "stderr": subprocess.PIPE, "stdin": subprocess.PIPE}

        def target():
            self.proc = subprocess.Popen(self.cmd, **pipe_kwargs)
            self.proc.communicate()

        thread = threading.Thread(target=target)
        thread.start()

        if self.post_cmd is not None:
            time.sleep(self.post_delay)
            subprocess.check_call(self.post_cmd, **pipe_kwargs)

        thread.join(timeout - self.post_delay if timeout is not None else timeout)

        # Checks
        if thread.is_alive():
            subprocess.check_call(self.fail_cmd, **pipe_kwargs)
            err =  Exception("Test failed with timeout!")

        elif retcode is not None and self.proc.returncode != retcode:
            err =  Exception("Test failed with unexpected returncode (expected {0}, got {1})".format(retcode, self.proc.returncode))

        if err is not None:
            print "FAIL"
            print "--- STDOUT ---"
            print out
            print "--- STDERR ---"
            print err
            print "--- ... ---"
            raise err
        else:
            print "OK"


if __name__ == "__main__":
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

    # Tests rely on exiting fast enough (exiting at all, in fact).
    base_cmd = [
        "docker",
        "run",
        "--rm",
        "--name=tini-test",
        "tini",
        "-vvvv",
    ]

    fail_cmd = ["docker", "kill", "tini-test"]

    # Reaping test
    Command(base_cmd + ["/tini/test/reaping/stage_1.py"], fail_cmd).run(timeout=10)

    # Signals test
    for sig, retcode in [("INT", 1), ("TERM", 143)]:
        Command(
            base_cmd + ["--", "/tini/test/signals/test.py"],
            fail_cmd,
            ["docker", "kill", "-s", sig, "tini-test"],
            2
        ).run(timeout=10, retcode=retcode)

    # Exit code test
    c = Command(base_cmd + ["-z"], fail_cmd).run(retcode=1)
    c = Command(base_cmd + ["zzzz"], fail_cmd).run(retcode=1)
    c = Command(base_cmd + ["-h"], fail_cmd).run(retcode=0)
