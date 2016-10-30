FROM ubuntu:trusty

COPY ci/install_deps.sh /install_deps.sh
RUN /install_deps.sh

# Pre-install those here for faster local builds.
RUN CFLAGS="-DPR_SET_CHILD_SUBREAPER=36 -DPR_GET_CHILD_SUBREAPER=37" pip install psutil python-prctl bitmap
