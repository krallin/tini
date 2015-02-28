Tini - A tiny but valid `init` for containers
=============================================

Tini is the simplest `init` you could think of.

All Tini does is spawn a single child (Tini is meant to be run in a container),
and wait for it to exit all the while reaping zombies and performing
signal forwarding.


Using Tini
----------

Add Tini to your container, and make it executable.

Tini is a very small file (in the 10KB range), and all it depends on is `libc`.

Once you've added Tini, use it like so:

     tini -- your_program and its args

Note that you *can* skip the `--` above if your program only accepts
positional arguments, but it's best to get used to using it.

If you try to use positional arguments with Tini without using `--`, you'll
get an error similar to:

    ./tini: invalid option -- 'c'


Understanding Tini
------------------

After spawning your process, Tini will wait for signals and forward those
to the child process (except for `SIGCHLD` and `SIGKILL`, of course).

Besides, Tini will reap potential zombie processes every second.


Debugging
---------

If something isn't working just like you expect, consider increasing the
verbosity level (up to 4):

    tini -v    -- bash -c 'exit 1'
    tini -vv   -- true
    tini -vvv  -- pwd
    tini -vvvv -- ls
