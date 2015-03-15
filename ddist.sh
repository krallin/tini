#!/bin/bash
set -o errexit
set -o nounset

REL_HERE=$(dirname "${BASH_SOURCE}")
HERE=$(cd "${REL_HERE}"; pwd)

DIST_DIR="${HERE}/dist"
rm -rf "${DIST_DIR}"

IMG="tini"
NAME="${IMG}-dist"
BIN="tini"

docker build -t "${IMG}" .

# Smoke test
docker run -it --rm --entrypoint="/tini/${BIN}" "${IMG}" "-h" "--" "true"

# Copy the binary
docker run -it --entrypoint="/bin/true" --name="${NAME}" "${IMG}"
docker cp "${NAME}:/tini/${BIN}" "${DIST_DIR}"
docker rm "${NAME}"
