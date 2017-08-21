#!/bin/bash
set -o errexit
set -o nounset
set -o xtrace

DEPS=(
  build-essential git gdb valgrind cmake rpm file
  libcap-dev python-dev python-pip python-setuptools
  hardening-includes gnupg
)

case "${ARCH_SUFFIX-}" in
  amd64|'') ;;
  arm64) DEPS+=(gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu libc6-dev-arm64-cross) ;;
  armel) DEPS+=(gcc-arm-linux-gnueabi binutils-arm-linux-gnueabi libc6-dev-armel-cross) ;;
  armhf) DEPS+=(gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf libc6-dev-armhf-cross) ;;
  i386) DEPS+=(libc6-dev-i386  gcc-multilib) ;;
  muslc-amd64) DEPS+=(musl-tools) ;;
  ppc64el) DEPS+=(gcc-powerpc64le-linux-gnu binutils-powerpc64le-linux-gnu libc6-dev-ppc64el-cross) ;;
  s390x) DEPS+=(gcc-s390x-linux-gnu binutils-s390x-linux-gnu libc6-dev-s390x-cross) ;;
  *) echo "Unknown ARCH_SUFFIX=${ARCH_SUFFIX-}"; exit 1 ;;
esac

apt-get update
apt-get install --no-install-recommends --yes "${DEPS[@]}"
rm -rf /var/lib/apt/lists/*

pip install virtualenv
