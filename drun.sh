#!/bin/bash
set -o errexit
set -o nounset

if [[ $# -eq 0 ]]; then
  # Default to bash if no arguments are set
  set "bash"
fi

REL_HERE=$(dirname "${BASH_SOURCE}")
HERE=$(cd "${REL_HERE}"; pwd)

IMG="tini"

docker run -it --rm \
  --volume="${HERE}/dist:/tini" \
  "${IMG}" "$@"

