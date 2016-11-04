#coding:utf-8
import os
import sys
import time
import pipes
import subprocess
import threading
import pexpect
import signal


class ReturnContainer():
    def __init__(self):
        self.value = None


class Command(object):
    def __init__(self, cmd, fail_cmd, post_cmd=None, post_delay=0):
        self.cmd = cmd
        self.fail_cmd = fail_cmd
        self.post_cmd = post_cmd
        self.post_delay = post_delay
        self.proc = None

    def run(self, timeout=None, retcode=0):
        print "Testing '{0}'...".format(" ".join(pipes.quote(s) for s in self.cmd)),
        sys.stdout.flush()

        err = None
        pipe_kwargs = {"stdout": subprocess.PIPE, "stderr": subprocess.PIPE, "stdin": subprocess.PIPE}

        def target():
            self.proc = subprocess.Popen(self.cmd, **pipe_kwargs)
            self.stdout, self.stderr = self.proc.communicate()

        thread = threading.Thread(target=target)
        thread.daemon = True

        thread.start()

        if self.post_cmd is not None:
            time.sleep(self.post_delay)
            subprocess.check_call(self.post_cmd, **pipe_kwargs)

        thread.join(timeout - self.post_delay if timeout is not None else timeout)

        # Checks
        if thread.is_alive():
            subprocess.check_call(self.fail_cmd, **pipe_kwargs)
            err =  Exception("Test failed with timeout!")

        elif self.proc.returncode != retcode:
            err =  Exception("Test failed with unexpected returncode (expected {0}, got {1})".format(retcode, self.proc.returncode))

        if err is not None:
            print "FAIL"
            print "--- STDOUT ---"
            print self.stdout
            print "--- STDERR ---"
            print self.stderr
            print "--- ... ---"
            raise err
        else:
            print "OK"


def attach_and_type_exit_0(name):
    print "Attaching to {0} to exit 0".format(name)
    p = pexpect.spawn("docker attach {0}".format(name))
    p.sendline('')
    p.sendline('exit 0')
    p.close()


def attach_and_issue_ctrl_c(name):
    print "Attaching to {0} to CTRL+C".format(name)
    p = pexpect.spawn("docker attach {0}".format(name))
    p.expect_exact('#')
    p.sendintr()
    p.close()


def test_tty_handling(img, name, base_cmd, fail_cmd, container_command, exit_function, expect_exit_code):
    print "Testing TTY handling (using container command '{0}' and exit function '{1}')".format(container_command, exit_function.__name__)
    rc = ReturnContainer()

    shell_ready_event = threading.Event()

    def spawn():
        cmd = base_cmd + ["--tty", "--interactive", img, "/tini/dist/tini"]
        if os.environ.get("MINIMAL") is None:
            cmd.append("--")
        cmd.append(container_command)
        p = pexpect.spawn(" ".join(cmd))
        p.expect_exact("#")
        shell_ready_event.set()
        rc.value = p.wait()

    thread = threading.Thread(target=spawn)
    thread.daemon = True

    thread.start()

    if not shell_ready_event.wait(2):
        raise Exception("Timeout waiting for shell to spawn")

    exit_function(name)

    thread.join(timeout=2)

    if thread.is_alive():
        subprocess.check_call(fail_cmd)
        raise Exception("Timeout waiting for container to exit!")

    if rc.value != expect_exit_code:
        raise Exception("Return code is: {0} (expected {1})".format(rc.value, expect_exit_code))



def main():
    img = sys.argv[1]
    name = "{0}-test".format(img)
    args_disabled = os.environ.get("MINIMAL")

    root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

    base_cmd = [
        "docker",
        "run",
        "--rm",
        "--volume={0}:/tini".format(root),
        "--name={0}".format(name),
    ]

    fail_cmd = ["docker", "kill", "-s", "KILL", name]

    # Funtional tests
    for entrypoint in ["/tini/dist/tini", "/tini/dist/tini-static"]:
        functional_base_cmd = base_cmd + [
            "--entrypoint={0}".format(entrypoint),
            "-e", "TINI_VERBOSITY=3",
            img,
        ]

        # Reaping test
        Command(functional_base_cmd + ["/tini/test/reaping/stage_1.py"], fail_cmd).run(timeout=10)

        # Signals test
        for sig, retcode in [("INT", 1), ("TERM", 143)]:
            Command(
                functional_base_cmd + ["/tini/test/signals/test.py"],
                fail_cmd,
                ["docker", "kill", "-s", sig, name],
                2
            ).run(timeout=10, retcode=retcode)

        # Exit code test
        Command(functional_base_cmd + ["-z"], fail_cmd).run(retcode=127 if args_disabled else 1)
        Command(functional_base_cmd + ["-h"], fail_cmd).run(retcode=127 if args_disabled else 0)
        Command(functional_base_cmd + ["zzzz"], fail_cmd).run(retcode=127)

    # Valgrind test (we only run this on the dynamic version, because otherwise Valgrind may bring up plenty of errors that are
    # actually from libc)
    Command(base_cmd + [img, "valgrind", "--leak-check=full", "--error-exitcode=1", "/tini/dist/tini", "ls"], fail_cmd).run()

    # Test tty handling
    test_tty_handling(img, name, base_cmd, fail_cmd, "dash", attach_and_type_exit_0, 0)
    test_tty_handling(img, name, base_cmd, fail_cmd, "dash -c 'while true; do echo \#; sleep 0.1; done'", attach_and_issue_ctrl_c, 128 + signal.SIGINT)

    # Installation tests (sh -c is used for globbing and &&)
    for image, pkg_manager, extension in [
            ["ubuntu:precise", "dpkg", "deb"],
            ["ubuntu:trusty", "dpkg", "deb"],
            ["centos:6", "rpm", "rpm"],
            ["centos:7", "rpm", "rpm"],
    ]:
        Command(base_cmd + [image, "sh", "-c", "{0} -i /tini/dist/*.{1} && /usr/bin/tini true".format(pkg_manager, extension)], fail_cmd).run()


if __name__ == "__main__":
    main()
