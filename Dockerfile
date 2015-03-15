FROM ubuntu

RUN apt-get update \
 && apt-get install --no-install-recommends --yes build-essential git gdb valgrind cmake rpm \
 && rm -rf /var/lib/apt/lists/*
