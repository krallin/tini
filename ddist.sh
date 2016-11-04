#!/bin/bash
set -o errexit
set -o nounset

REL_HERE=$(dirname "${BASH_SOURCE}")
HERE=$(cd "${REL_HERE}"; pwd)

IMG="tini"
SRC="/tini"

# Cleanup the build dir
rm -f "${HERE}/dist"/*

# Create the build image
docker build -t "${IMG}" .

# Run test without subreaper support, don't copy build files here
docker run -it --rm \
  --volume="${HERE}:${SRC}" \
  -e BUILD_DIR=/tmp/tini-build \
  -e SOURCE_DIR="${SRC}" \
  -e FORCE_SUBREAPER="${FORCE_SUBREAPER:="1"}" \
  -e GPG_PASSPHRASE="${GPG_PASSPHRASE:=}" \
  -e CC="${CC:=gcc}" \
  -e ARCH_NATIVE="${ARCH_NATIVE-1}" \
  -e ARCH_SUFFIX="${ARCH_SUFFIX-}" \
  -e MINIMAL="${MINIMAL-}" \
  "${IMG}" "${SRC}/ci/run_build.sh"
