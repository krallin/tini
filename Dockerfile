FROM ubuntu:precise

RUN apt-get update \
 && apt-get install --no-install-recommends --yes build-essential git gdb valgrind cmake rpm python-dev libcap-dev python-pip python-virtualenv \
 && rm -rf /var/lib/apt/lists/*

# Pre-install those here for faster local builds.
RUN CFLAGS="-DPR_SET_CHILD_SUBREAPER=36 -DPR_GET_CHILD_SUBREAPER=37" pip install psutil python-prctl bitmap
