FROM ubuntu

RUN apt-get update \
 && apt-get install --no-install-recommends --yes build-essential git gdb valgrind \
 && rm -rf /var/lib/apt/lists/*

ADD . /tini
RUN cd /tini && make clean && make

ENTRYPOINT ["/tini/tini"]
