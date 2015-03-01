FROM ubuntu

RUN apt-get update && apt-get install --no-install-recommends -y build-essential gdb && rm -rf /var/lib/apt/lists/*
RUN apt-get update
RUN apt-get install --no-install-recommends -y git

ADD . /tini
RUN cd /tini && make clean && make

ENTRYPOINT ["/tini/tini"]
