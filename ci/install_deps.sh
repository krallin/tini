#!/bin/bash
set -o errexit
set -o nounset

apt-get update

apt-get install --no-install-recommends --yes \
  build-essential git gdb valgrind cmake rpm \
  python-dev libcap-dev python-pip python-virtualenv \
  hardening-includes gnupg \
  gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu libc6-dev-arm64-cross \
  gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabi libc6-dev-armhf-cross

rm -rf /var/lib/apt/lists/*
