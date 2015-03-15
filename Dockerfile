FROM ubuntu

RUN apt-get update \
 && apt-get install --no-install-recommends --yes build-essential git gdb valgrind cmake clang \
 && rm -rf /var/lib/apt/lists/*

ADD . /tini
WORKDIR /tini

RUN ./ci/run_build.sh

ENTRYPOINT ["/tini/tini"]
