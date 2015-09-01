#!/bin/bash
# Should be run from the root dir, or SOURCE_DIR should be set.
set -o errexit
set -o nounset

# Paths
: ${SOURCE_DIR:="."}
: ${DIST_DIR:="${SOURCE_DIR}/dist"}
: ${BUILD_DIR:="/tmp/build"}

# Make those paths absolute, and export them for the Python tests to consume.
export SOURCE_DIR="$(readlink -f "${SOURCE_DIR}")"
export DIST_DIR="$(readlink -f "${DIST_DIR}")"
export BUILD_DIR="$(readlink -f "${BUILD_DIR}")"


# Our build platform doesn't have those newer Linux flags, but we want Tini to have subreaper support
# We also use those in our tests
export CFLAGS="-DPR_SET_CHILD_SUBREAPER=36 -DPR_GET_CHILD_SUBREAPER=37"


# Ensure Python output is not buffered (to make tests output clearer)
export PYTHONUNBUFFERED=1

# Set path to prioritize our utils
export REAL_PATH="${PATH}"
export PATH="${SOURCE_DIR}/ci/util:${PATH}"

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
  $tini -vvv true

  echo "Testing $tini with: false"
  if $tini -vvv false; then
    exit 1
  fi

  # Move files to the dist dir for testing
  mkdir -p "${DIST_DIR}"
  cp "${BUILD_DIR}"/tini{,-static,*.rpm,*deb} "${DIST_DIR}"

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

# Create virtual environment to run tests
VENV="${BUILD_DIR}/venv"
virtualenv "${VENV}"

# Don't use activate because it does not play nice with nounset
export PATH="${VENV}/bin:${PATH}"

# Install test dependencies
pip install psutil python-prctl

# Run tests
python "${SOURCE_DIR}/test/run_inner_tests.py"
