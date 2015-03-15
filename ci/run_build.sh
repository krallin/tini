#!/bin/bash
# Should be run from the root dir (!)
set -o errexit
set -o nounset

# Build
cmake .
make clean
make

# Smoke tests (actual tests need Docker to run; they don't run within the CI environment)

# Success
for tini in ./tini; do
  echo "Testing $tini with: true"
  $tini -vvvv true

  echo "Testing $tini with: false"
  if $tini -vvvv false; then
    exit 1
  fi
done
