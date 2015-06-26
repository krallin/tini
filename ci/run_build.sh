#!/bin/bash
# Should be run from the root dir, or SOURCE_DIR should be set.
set -o errexit
set -o nounset

: ${SOURCE_DIR:="."}
: ${DIST_DIR:="${SOURCE_DIR}/dist"}
: ${BUILD_DIR:="/tmp/build"}

# Set path to prioritize our utils
export REAL_PATH="${PATH}"
export PATH="$(readlink -f "${SOURCE_DIR}")/ci/util:${PATH}"

# Build
cmake -B"${BUILD_DIR}" -H"${SOURCE_DIR}"

pushd "${BUILD_DIR}"
make clean
make
make package

popd

# Smoke tests (actual tests need Docker to run; they don't run within the CI environment)
for tini in "${BUILD_DIR}/tini" "${BUILD_DIR}/tini-static"; do
  echo "Testing $tini with: true"
  $tini -vvvv true

  echo "Testing $tini with: false"
  if $tini -vvvv false; then
    exit 1
  fi

  # Place files
  mkdir -p "${DIST_DIR}"
  cp "${BUILD_DIR}"/tini{,*.rpm,*deb} "${DIST_DIR}"

  # Quick audit
  if which rpm; then
    echo "Contents for RPM:"
    rpm -qlp "${DIST_DIR}/tini"*.rpm
  fi

  if which dpkg; then
    echo "Contents for DEB:"
    dpkg --contents "${DIST_DIR}/tini"*deb
  fi
done
