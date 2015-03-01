#!/bin/bash
set -o errexit
set -o nounset

IMG="tini"


docker build -t "${IMG}" .
python test/test.py  "${IMG}"
