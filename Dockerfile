FROM ubuntu:precise

RUN apt-get update \
 && apt-get install --no-install-recommends --yes build-essential git gdb valgrind cmake rpm python3 \
 && rm -rf /var/lib/apt/lists/*
