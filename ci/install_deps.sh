#!/bin/bash
set -o errexit
set -o nounset
set -o xtrace

DEPS=(
  build-essential git gdb valgrind cmake rpm file
  libcap-dev python-dev python-pip python-setuptools
  hardening-includes gnupg
)

if [[ -z "${ARCH_SUFFIX-}" ]] || [[ "$ARCH_SUFFIX" = "amd64" ]]; then
  true
elif [[ "$ARCH_SUFFIX" = "armel" ]]; then
  DEPS+=(gcc-arm-linux-gnueabi binutils-arm-linux-gnueabi libc6-dev-armel-cross)
elif [[ "$ARCH_SUFFIX" = "armhf" ]]; then
  DEPS+=(gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf libc6-dev-armhf-cross)
elif [[ "$ARCH_SUFFIX" = "arm64" ]]; then
  DEPS+=(gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu libc6-dev-arm64-cross)
elif [[ "$ARCH_SUFFIX" = "ppc64el" ]]; then
  DEPS+=(gcc-powerpc64le-linux-gnu binutils-powerpc64le-linux-gnu libc6-dev-ppc64el-cross)
elif [[ "$ARCH_SUFFIX" = "s390x" ]]; then
  DEPS+=(gcc-s390x-linux-gnu binutils-s390x-linux-gnu libc6-dev-s390x-cross)
elif [[ "$ARCH_SUFFIX" = "i386" ]]; then
  DEPS+=(libc6-dev-i386  gcc-multilib)
elif [[ "$ARCH_SUFFIX" = "muslc-amd64" ]]; then
  DEPS+=(musl-tools)
else
  echo "Unknown ARCH_SUFFIX=${ARCH_SUFFIX}"
  exit 1
fi

apt-get update
apt-get install --no-install-recommends --yes "${DEPS[@]}"
rm -rf /var/lib/apt/lists/*

pip install virtualenv
