#!/bin/bash
# Should be run from the root dir, or SOURCE_DIR should be set.
set -o errexit
set -o nounset

# Default compiler
: ${CC:="gcc"}

# Paths
: ${SOURCE_DIR:="."}
: ${DIST_DIR:="${SOURCE_DIR}/dist"}
: ${BUILD_DIR:="/tmp/build"}

# GPG Configuration
: ${GPG_PASSPHRASE:=""}


# Make those paths absolute, and export them for the Python tests to consume.
export SOURCE_DIR="$(readlink -f "${SOURCE_DIR}")"
export DIST_DIR="$(readlink -f "${DIST_DIR}")"
export BUILD_DIR="$(readlink -f "${BUILD_DIR}")"

# Configuration
: ${FORCE_SUBREAPER:="1"}
export FORCE_SUBREAPER


# Our build platform doesn't have those newer Linux flags, but we want Tini to have subreaper support
# We also use those in our tests
CFLAGS="-DPR_SET_CHILD_SUBREAPER=36 -DPR_GET_CHILD_SUBREAPER=37"
if [[ "${FORCE_SUBREAPER}" -eq 1 ]]; then
  # If FORCE_SUBREAPER is requested, then we set those CFLAGS for the Tini build
  export CFLAGS
fi

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
  echo "Smoke test for $tini"
  "${tini}" -h

  echo "Testing $tini with: true"
  "${tini}" -vvv true

  echo "Testing $tini with: false"
  if "${tini}" -vvv false; then
    exit 1
  fi

  # Test stdin / stdout are handed over to child
  echo "Testing pipe"
  echo "exit 0" | "${tini}" -vvv sh
  if [[ ! "$?" -eq "0" ]]; then
    echo "Pipe test failed"
    exit 1
  fi

  echo "Checking hardening on $tini"
  hardening-check --nopie --nostackprotector --nobindnow "${tini}"
done

# Move files to the dist dir for testing
mkdir -p "${DIST_DIR}"
cp "${BUILD_DIR}"/tini{,-static,*.rpm,*deb} "${DIST_DIR}"

# Quick package audit
if which rpm; then
  echo "Contents for RPM:"
  rpm -qlp "${DIST_DIR}/tini"*.rpm
fi

if which dpkg; then
  echo "Contents for DEB:"
  dpkg --contents "${DIST_DIR}/tini"*deb
fi

# Compile test code
"${CC}" -o "${BUILD_DIR}/sigconf-test" "${SOURCE_DIR}/test/sigconf/sigconf-test.c"

# Create virtual environment to run tests.
# Accept system site packages for faster local builds.
VENV="${BUILD_DIR}/venv"
virtualenv --system-site-packages "${VENV}"

# Don't use activate because it does not play nice with nounset
export PATH="${VENV}/bin:${PATH}"
export CFLAGS  # We need them to build our test suite, regardless of FORCE_SUBREAPER

# Install test dependencies
pip install psutil python-prctl bitmap

# Run tests
python "${SOURCE_DIR}/test/run_inner_tests.py"

# If a signing key is made available, then use it to sign the binaries
if [[ -f "${SOURCE_DIR}/sign.key" ]]; then
  echo "Signing binaries"
  GPG_SIGN_HOMEDIR="${BUILD_DIR}/gpg-sign"
  GPG_VERIFY_HOMEDIR="${BUILD_DIR}/gpg-verify"
  mkdir "${GPG_SIGN_HOMEDIR}" "${GPG_VERIFY_HOMEDIR}"
  chmod 700 "${GPG_SIGN_HOMEDIR}" "${GPG_VERIFY_HOMEDIR}"

  gpg --homedir "${GPG_SIGN_HOMEDIR}" --import "${SOURCE_DIR}/sign.key"
  gpg --homedir "${GPG_VERIFY_HOMEDIR}" --keyserver ha.pool.sks-keyservers.net --recv-keys 0527A9B7

  for tini in "${DIST_DIR}/tini" "${DIST_DIR}/tini-static"; do
    echo "${GPG_PASSPHRASE}" | gpg --homedir "${GPG_SIGN_HOMEDIR}" --passphrase-fd 0 --armor --detach-sign "${tini}"
    gpg --homedir "${GPG_VERIFY_HOMEDIR}" --verify "${tini}.asc"
  done
fi
