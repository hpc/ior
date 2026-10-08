#!/bin/sh
set -eu
test_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
python3 "$test_dir/test-summary-output.py"
