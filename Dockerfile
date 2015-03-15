FROM ubuntu

RUN apt-get update \
 && apt-get install --no-install-recommends --yes build-essential git gdb valgrind cmake \
 && rm -rf /var/lib/apt/lists/*

ADD . /tini
WORKDIR /tini

RUN cmake . && make clean && make

ENTRYPOINT ["/tini/tini"]
