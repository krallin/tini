#!/bin/bash
set -o errexit
set -o nounset

if [[ "$#" != 1 ]]; then
  echo "Usage: $0 ARCH_SUFFIX"
  exit 1
fi
suffix="$1"

REL_HERE=$(dirname "${BASH_SOURCE}")
HERE=$(cd "${REL_HERE}"; pwd)

IMG="tini-build-${suffix}"
SRC="/tini"

# Cleanup the build dir
rm -f "${HERE}/dist"/*

# Create the build image
docker build --build-arg "ARCH_SUFFIX=${suffix}" -t "${IMG}" .

# Run test without subreaper support, don't copy build files here
docker run -it --rm \
  --volume="${HERE}:${SRC}" \
  -e BUILD_DIR=/tmp/tini-build \
  -e SOURCE_DIR="${SRC}" \
  -e FORCE_SUBREAPER="${FORCE_SUBREAPER:="1"}" \
  -e GPG_PASSPHRASE="${GPG_PASSPHRASE:=}" \
  -e CC="${CC:=gcc}" \
  -e CFLAGS="${CFLAGS-}" \
  -e ARCH_NATIVE="${ARCH_NATIVE-1}" \
  -e ARCH_SUFFIX="${suffix}" \
  -e MINIMAL="${MINIMAL-}" \
  "${IMG}" "${SRC}/ci/run_build.sh"
