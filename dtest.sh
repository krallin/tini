#!/bin/bash
set -o errexit
set -o nounset

IMG="tini"

if [[ "$#" != 1 ]]; then
  echo "Usage: $0 ARCH_SUFFIX"
  exit 1
fi
suffix="$1"

IMG="tini-build-${suffix}"
python test/run_outer_tests.py "${IMG}"
