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
docker run -it --name="${NAME}" --entrypoint="true" "${IMG}" true
docker cp "${NAME}:/tini/${BIN}" "${DIST_DIR}"
docker rm "${NAME}"
