Tini - A tiny but valid `init` for containers
=============================================

Tini is the simplest `init` you could think of.

All Tini does is spawn a single child (Tini is meant to be run in a container),
and wait for it to exit all the while reaping zombies.

Using Tini
----------

Add Tini to your container, and make it executable.

It's a very small file (10KB range). All Tini depends on is libc.

Once you've added Tini, use it like so:

     tini -- your_program and its args

Note that you *can* skip the `--` above if your program only accepts
positional arguments, but it's best to get used to using it.
