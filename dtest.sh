#!/bin/bash
set -o errexit
set -o nounset

IMG="tini"


docker build -t "${IMG}" .
exec python test/run_outer_tests.py  "${IMG}"
