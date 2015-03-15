#!/bin/bash
set -o errexit
set -o nounset

REL_HERE=$(dirname "${BASH_SOURCE}")
HERE=$(cd "${REL_HERE}"; pwd)

IMG="tini"
NAME="${IMG}-dist"

docker build -t "${IMG}" .

# Copy the generated README
docker run -it --entrypoint="/bin/true" --name="${NAME}" "${IMG}"
docker cp "${NAME}:/tini/README.md" "${HERE}"
docker rm "${NAME}"
