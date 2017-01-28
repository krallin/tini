#!/bin/bash
set -o errexit
set -o nounset
set -o xtrace

DEPS=(
  build-essential git gdb valgrind cmake rpm \
  python-dev libcap-dev python-pip python-virtualenv \
  hardening-includes gnupg
)

if [[ "$ARCH_SUFFIX" = "amd64" ]]; then
  true
elif [[ "$ARCH_SUFFIX" = "armhf" ]]; then
  DEPS+=(gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabi libc6-dev-armhf-cross)
elif [[ "$ARCH_SUFFIX" = "arm64" ]]; then
  DEPS+=(gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu libc6-dev-arm64-cross)
elif [[ "$ARCH_SUFFIX" = "i386" ]]; then
  DEPS+=(libc6-dev-i386  gcc-multilib)
else
  echo "Unknown ARCH_SUFFIX=${ARCH_SUFFIX}"
  exit 1
fi

apt-get update
apt-get install --no-install-recommends --yes "${DEPS[@]}"
rm -rf /var/lib/apt/lists/*
