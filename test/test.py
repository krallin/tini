#coding:utf-8
import os
import time
import subprocess
import threading


class Command(object):
    def __init__(self, cmd, post_cmd=None, post_delay=None):
        self.cmd = cmd
        self.post_cmd = post_cmd
        self.post_delay = post_delay
        self._process = None

    def run(self, timeout):
        def target():
            self._process = subprocess.Popen(self.cmd)
            self._process.communicate()

        thread = threading.Thread(target=target)
        thread.start()

        if self.post_cmd is not None:
            if self.post_delay is not None:
                time.sleep(self.post_delay)
            subprocess.check_call(self.post_cmd)

        thread.join(timeout)
        if thread.is_alive():
            raise Exception("Test failed!")


if __name__ == "__main__":
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

    # Tests rely on exiting fast enough (exiting at all, in fact).
    base_cmd = [
        "docker",
        "run",
        "-it",
        "--rm",
        "--name=tini-test",
        "-v",
        "{0}:{0}".format(root),
        "ubuntu",
        "{0}/tini".format(root),
        "-vvv",
        "--",
    ]

    # Reaping test
    Command(base_cmd + ["/Users/thomas/dev/tini/test/reaping/stage_1.py"]).run(timeout=10)

    # Signals test
    for sig in ["INT", "TERM"]:
        Command(
            base_cmd + ["/Users/thomas/dev/tini/test/signals/test.py"],
            ["docker", "kill", "-s", sig, "tini-test"],
            2
        ).run(timeout=10)
