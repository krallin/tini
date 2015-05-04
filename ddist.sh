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

# Run the build
docker run --rm --volume="${HERE}:${SRC}" -e BUILD_DIR=/tmp/tini-build -e SOURCE_DIR="${SRC}" "${IMG}" "${SRC}/ci/run_build.sh"
