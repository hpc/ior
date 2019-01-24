#!/usr/bin/env bash
#
# Test the IOR source package.  This is a complicated alternative to
# the `make distcheck` option.
#

# These options will be passed directly to the autoconf configure script
CONFIGURE_OPTS="${CONFIGURE_OPTS:-"CFLAGS=-std=c99 --disable-silent-rules"}"

BASE_DIR="$(cd "${0%/*}" && pwd)"
if [ -z "$BASE_DIR" -o ! -d "$BASE_DIR" ]; then
    echo "Cannot determine BASE_DIR (${BASE_DIR})" >&2
    exit 2
fi
PACKAGE="$(awk '/^Package/ {print $2}' $BASE_DIR/META)"
VERSION="$(awk '/^Version/ {print $2}' $BASE_DIR/META)"
DIST_TGZ="${BASE_DIR}/${PACKAGE}-${VERSION}.tar.gz"

TEST_DIR="${BASE_DIR}/test"
INSTALL_DIR="${TEST_DIR}/_inst"

if [ -z "$DIST_TGZ" -o ! -f "$DIST_TGZ" ]; then
    echo "Cannot find DIST_TGZ ($DIST_TGZ)" >&2
    exit 1
fi

test -d "$TEST_DIR" && rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

tar -C "$TEST_DIR" -zxf "${DIST_TGZ}"

# Configure, make, and install from the source distribution
set -e
cd "$TEST_DIR/${PACKAGE}-${VERSION}"
./configure $CONFIGURE_OPTS "--prefix=$INSTALL_DIR"
make install
set +e

# Run the MPI tests
export IOR_BIN_DIR="${INSTALL_DIR}/bin"
export IOR_OUT="${TEST_DIR}/test_logs"
export IOR_TMP="$(mktemp -d)"
source "${TEST_DIR}/${PACKAGE}-${VERSION}/testing/basic-tests.sh"

# Clean up residual temporary directories (if this isn't running as root)
if [ -d "$IOR_TMP" -a "$(id -u)" -ne 0 -a ! -z "$IOR_TMP" ]; then
    rm -rvf "$IOR_TMP"
fi
