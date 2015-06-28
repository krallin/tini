#!/bin/bash
set -o errexit
set -o nounset

IMG="tini"


docker build -t "${IMG}" .
python test/run_outer_tests.py  "${IMG}"
