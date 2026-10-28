#!/bin/sh
set -eu

test_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
MDTEST_BIN="$(CDPATH= cd "$test_dir/.." && pwd)/mdtest"
export MDTEST_BIN
export MPIEXEC="${MPIEXEC:-mpiexec}"
export PYTHON="${PYTHON:-python3}"
exec bash "$test_dir/../../testing/mdtest-summary-tests.sh"
