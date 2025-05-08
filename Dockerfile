FROM ubuntu:xenial

ARG ARCH_SUFFIX

COPY ci/install_deps.sh /install_deps.sh
RUN /install_deps.sh

# Pre-install those here for faster local builds.
RUN CFLAGS="-DPR_SET_CHILD_SUBREAPER=36 -DPR_GET_CHILD_SUBREAPER=37" python3 -m pip install psutil python-prctl bitmap

ARG ARCH_NATIVE
ARG CC

# Persist ARGs into the image

RUN ln -s /usr/bin/python3 /usr/bin/python

ENV ARCH_SUFFIX="$ARCH_SUFFIX" \
    ARCH_NATIVE="$ARCH_NATIVE" \
    CC="$CC"
