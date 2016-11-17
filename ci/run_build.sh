#!/bin/bash
# Should be run from the root dir, or SOURCE_DIR should be set.
set -o errexit
set -o nounset
set -o pipefail

# Default compiler
: ${CC:="gcc"}

echo "CC=${CC}"

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
CMAKE_ARGS=(-B"${BUILD_DIR}" -H"${SOURCE_DIR}")
if [[ -n "${MINIMAL:-}" ]]; then
  CMAKE_ARGS+=(-DMINIMAL=ON)
fi
cmake "${CMAKE_ARGS[@]}"

pushd "${BUILD_DIR}"
make clean
make
if [[ -n "${ARCH_NATIVE:=}" ]]; then
  make package
fi
popd

pkg_version="$(cat "${BUILD_DIR}/VERSION")"


if [[ -n "${ARCH_NATIVE:=}" ]]; then
  echo "Built native package (ARCH_NATIVE=${ARCH_NATIVE})"
  echo "Running smoke and internal tests"

  BIN_TEST_DIR="${BUILD_DIR}/bin-test"
  mkdir -p "$BIN_TEST_DIR"
  export PATH="${BIN_TEST_DIR}:${PATH}"

  # Smoke tests (actual tests need Docker to run; they don't run within the CI environment)
  for tini in "${BUILD_DIR}/tini" "${BUILD_DIR}/tini-static"; do
    echo "Smoke test for ${tini}"
    "$tini" --version

    echo "Testing ${tini} --version"
    "$tini" --version | grep -q "tini version"

    echo "Testing ${tini} without arguments exits with 1"
    ! "$tini" 2>/dev/null

    echo "Testing ${tini} shows help message"
    {
      ! "$tini" 2>&1
    } | grep -q "supervision of a valid init process"

    if [[ -n "${MINIMAL:-}" ]]; then
      echo "Testing $tini with: true"
      "${tini}" true

      echo "Testing $tini with: false"
      if "${tini}" false; then
        exit 1
      fi

      echo "Testing ${tini} does not reference options that don't exist"
      ! {
        ! "$tini" 2>&1
      } | grep -q "more verbose"

      # We try running binaries named after flags (both valid and invalid
      # flags) and test that they run.
      for flag in h s x; do
        bin="-${flag}"
        echo "Testing $tini can run binary: ${bin}"
        cp "$(which true)" "${BIN_TEST_DIR}/${bin}"
        "$tini" "$bin"
      done

      echo "Testing $tini can run binary --version if args are given"
      cp "$(which true)" "${BIN_TEST_DIR}/--version"
      if "$tini" "--version" --foo | grep -q "tini version"; then
        exit 1
      fi
    else
      echo "Testing ${tini} -h"
      "${tini}" -h

      echo "Testing $tini for license"
      "$tini" -l | diff - "${SOURCE_DIR}/LICENSE"

      echo "Testing $tini with: true"
      "${tini}" -vvv true

      echo "Testing $tini with: false"
      if "${tini}" -vvv false; then
        exit 1
      fi

      echo "Testing ${tini} references options that exist"
      {
        ! "$tini" 2>&1
      } | grep -q "more verbose"
    fi

    echo "Testing ${tini} supports TINI_VERBOSITY"
    TINI_VERBOSITY=3 "$tini" true 2>&1 | grep -q 'Received SIGCHLD'

    echo "Testing ${tini} exits with 127 if the command does not exist"
    "$tini" foobar123 && rc="$?" || rc="$?"
    if [[ "$rc" != 127 ]]; then
      echo "Exit code was: ${rc}"
      exit 1
    fi

    echo "Testing ${tini} exits with 126 if the command is not executable"
    "$tini" /etc && rc="$?" || rc="$?"
    if [[ "$rc" != 126 ]]; then
      echo "Exit code was: ${rc}"
      exit 1
    fi

    # Test stdin / stdout are handed over to child
    echo "Testing ${tini} does not break pipes"
    echo "exit 0" | "${tini}" sh
    if [[ ! "$?" -eq "0" ]]; then
      echo "Pipe test failed"
      exit 1
    fi

    echo "Checking hardening on $tini"
    hardening-check --nopie --nostackprotector --nobindnow "${tini}"
  done

  # Quick package audit
  if which rpm >/dev/null; then
    echo "Contents for RPM:"
    rpm -qlp "${BUILD_DIR}/tini_${pkg_version}.rpm"
    echo "--"
  fi

  if which dpkg >/dev/null; then
    echo "Contents for DEB:"
    dpkg --contents "${BUILD_DIR}/tini_${pkg_version}.deb"
    echo "--"
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
else
  if [[ ! -n "${ARCH_SUFFIX:=}" ]]; then
    echo "Built cross package, but $ARCH_SUFFIX is empty!"
    exit 1
  fi
  echo "Built cross package (ARCH_SUFFIX=${ARCH_SUFFIX})"
  echo "Skipping smoke and internal tests"
fi

# Now, copy over files to DIST_DIR, with appropriate names depending on the
# architecture.
# Handle the DEB / RPM
mkdir -p "${DIST_DIR}"

TINIS=()

for tini in tini tini-static; do
  if [[ -n "${ARCH_SUFFIX:=}" ]]; then
    to="${DIST_DIR}/${tini}-${ARCH_SUFFIX}"
    TINIS+=("$to")
    cp "${BUILD_DIR}/${tini}" "$to"
  fi

  if [[ -n "${ARCH_NATIVE:=}" ]]; then
    to="${DIST_DIR}/${tini}"
    TINIS+=("$to")
    cp "${BUILD_DIR}/${tini}" "$to"
  fi
done

if [[ -n "${ARCH_NATIVE:=}" ]]; then
  for pkg_format in deb rpm; do
    src="${BUILD_DIR}/tini_${pkg_version}.${pkg_format}"

    if [[ -n "${ARCH_SUFFIX:=}" ]]; then
      to="${DIST_DIR}/tini_${pkg_version}-${ARCH_SUFFIX}.${pkg_format}"
      TINIS+=("$to")
      cp "$src" "$to"
    fi

    if [[ -n "${ARCH_NATIVE:=}" ]]; then
      to="${DIST_DIR}/tini_${pkg_version}.${pkg_format}"
      TINIS+=("$to")
      cp "$src" "$to"
    fi
  done
fi

echo "Tinis: ${TINIS[*]}"

for tini in "${TINIS[@]}"; do
  echo "${tini}:"
  sha1sum "$tini"
  file "$tini"
  echo "--"
done

# If a signing key and passphrase are made available, then use it to sign the
# binaries
if [[ -n "$GPG_PASSPHRASE" ]] && [[ -f "${SOURCE_DIR}/sign.key" ]]; then
  echo "Signing tinis"
  GPG_SIGN_HOMEDIR="${BUILD_DIR}/gpg-sign"
  GPG_VERIFY_HOMEDIR="${BUILD_DIR}/gpg-verify"
  PGP_KEY_FINGERPRINT="595E85A6B1B4779EA4DAAEC70B588DFF0527A9B7"
  PGP_KEYSERVER="ha.pool.sks-keyservers.net"

  mkdir "${GPG_SIGN_HOMEDIR}" "${GPG_VERIFY_HOMEDIR}"
  chmod 700 "${GPG_SIGN_HOMEDIR}" "${GPG_VERIFY_HOMEDIR}"

  gpg --homedir "${GPG_SIGN_HOMEDIR}" --import "${SOURCE_DIR}/sign.key"
  gpg --homedir "${GPG_VERIFY_HOMEDIR}" --keyserver "$PGP_KEYSERVER" --recv-keys "$PGP_KEY_FINGERPRINT"

  for tini in "${TINIS[@]}"; do
    echo "${GPG_PASSPHRASE}" | gpg --homedir "${GPG_SIGN_HOMEDIR}" --passphrase-fd 0 --armor --detach-sign "${tini}"
    gpg --homedir "${GPG_VERIFY_HOMEDIR}" --verify "${tini}.asc"
  done
fi
