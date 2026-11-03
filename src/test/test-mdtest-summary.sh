#!/bin/sh
set -eu

test_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
# Automake runs tests from the build tree's src/test directory.
MDTEST_BIN="${MDTEST_BIN:-$(CDPATH= cd .. && pwd)/mdtest}"
export MDTEST_BIN
export MPIEXEC="${MPIEXEC:-mpiexec}"
export PYTHON="${PYTHON:-python3}"
exec bash "$test_dir/../../testing/mdtest-summary-tests.sh"
