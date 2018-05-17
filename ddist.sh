#!/bin/bash
set -o errexit
set -o nounset

REL_HERE=$(dirname "${BASH_SOURCE}")
HERE=$(cd "${REL_HERE}"; pwd)

IMG="tini-build"

if [[ -n "${ARCH_SUFFIX-}" ]]; then
  IMG="${IMG}_${ARCH_SUFFIX}"
fi

if [[ -n "${ARCH_NATIVE-}" ]]; then
  IMG="${IMG}_native"
fi

if [[ -n "${CC-}" ]]; then
  IMG="${IMG}_${CC}"
fi

# Cleanup the build dir
rm -f "${HERE}/dist"/*

# Create the build image
echo "build: ${IMG}"

docker build \
  --build-arg "ARCH_SUFFIX=${ARCH_SUFFIX-}" \
  --build-arg "ARCH_NATIVE=${ARCH_NATIVE-}" \
  --build-arg "CC=${CC-gcc}" \
  -t "${IMG}" \
  .

# Build new Tini
SRC="/tini"

docker run -it --rm \
  --volume="${HERE}:${SRC}" \
  -e BUILD_DIR=/tmp/tini-build \
  -e SOURCE_DIR="${SRC}" \
  -e FORCE_SUBREAPER="${FORCE_SUBREAPER-1}" \
  -e GPG_PASSPHRASE="${GPG_PASSPHRASE-}" \
  -e CFLAGS="${CFLAGS-}" \
  -e MINIMAL="${MINIMAL-}" \
  -u "$(id -u):$(id -g)" \
  "${IMG}" "${SRC}/ci/run_build.sh"
