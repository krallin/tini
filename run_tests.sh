#!/bin/bash
set -o errexit
set -o nounset

REL_HERE=$(dirname "${BASH_SOURCE}")
HERE=$(cd "${REL_HERE}"; pwd)

for i in $(seq 0 1); do
  export FORCE_SUBREAPER="${i}"
  echo "Testing with FORCE_SUBREAPER=${FORCE_SUBREAPER}"
  "${HERE}/ddist.sh"
  "${HERE}/dtest.sh"
done
