FROM ubuntu:precise

RUN apt-get update \
 && apt-get install --no-install-recommends --yes build-essential git gdb valgrind cmake rpm python-dev libcap-dev python-pip python-virtualenv \
 && rm -rf /var/lib/apt/lists/*

RUN pip install psutil
