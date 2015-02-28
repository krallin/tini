FROM ubuntu

RUN apt-get update && apt-get install -y build-essential && rm -rf /var/lib/apt/lists/*

ADD . /tini
RUN cd /tini && make
