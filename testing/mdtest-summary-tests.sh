#!/bin/sh
set -eu

ROOT=$(CDPATH= cd "$(dirname "$0")" && pwd)
MDTEST_BIN=${MDTEST_BIN:-"$ROOT/../src/mdtest"}
MPIEXEC=${MPIEXEC:-mpiexec}
PYTHON=${PYTHON:-python3}
TMPDIR=${TMPDIR:-/tmp}
WORK=$(mktemp -d "$TMPDIR/mdtest-summary.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

if [ ! -x "$MDTEST_BIN" ]; then
    echo "mdtest binary not found: $MDTEST_BIN" >&2
    exit 1
fi

run_default() {
    ranks=$1
    iterations=$2
    output="$WORK/default-$ranks-$iterations.out"
    $MPIEXEC -n "$ranks" "$MDTEST_BIN" -a DUMMY -F -n 1 -i "$iterations" \
        >"$output" 2>"$output.stderr"
    grep -q "SUMMARY" "$output"
}

run_text_file() {
    ranks=$1
    iterations=$2
    summary="$WORK/text-$ranks-$iterations.summary"
    output="$WORK/text-$ranks-$iterations.out"
    $MPIEXEC -n "$ranks" "$MDTEST_BIN" -a DUMMY -F -n 1 -i "$iterations" \
        --summaryFile="$summary" >"$output" 2>"$output.stderr"
    grep -q "SUMMARY" "$summary"
    ! grep -q "SUMMARY" "$output"
}

run_json_file() {
    ranks=$1
    iterations=$2
    summary="$WORK/json-$ranks-$iterations.json"
    output="$WORK/json-$ranks-$iterations.out"
    $MPIEXEC -n "$ranks" "$MDTEST_BIN" -a DUMMY -F -n 1 -i "$iterations" \
        --summaryFormat=JSON --summaryFile="$summary" >"$output" 2>"$output.stderr"
    "$PYTHON" "$ROOT/test-mdtest-summary.py" "$summary" "$iterations" "$ranks"
}

run_json_stdout() {
    ranks=$1
    iterations=$2
    output="$WORK/json-stdout-$ranks-$iterations.json"
    log="$WORK/json-stdout-$ranks-$iterations.log"
    $MPIEXEC -n "$ranks" "$MDTEST_BIN" -a DUMMY -F -n 1 -i "$iterations" \
        --summaryFormat=JSON >"$output" 2>"$log"
    "$PYTHON" "$ROOT/test-mdtest-summary.py" "$output" "$iterations" "$ranks"
}

run_write_failure() {
    ranks=$1
    mode=$2
    output="$WORK/full-$mode-$ranks.out"
    log="$WORK/full-$mode-$ranks.err"
    case "$mode" in
        text-file)
            set -- "$MDTEST_BIN" -a DUMMY -F -n 1 -i 1 --summaryFile=/dev/full
            ;;
        json-file)
            set -- "$MDTEST_BIN" -a DUMMY -F -n 1 -i 1 --summaryFormat=JSON --summaryFile=/dev/full
            ;;
        json-stdout)
            set -- sh -c 'exec "$@" > /dev/full' sh "$MDTEST_BIN" -a DUMMY -F -n 1 -i 1 --summaryFormat=JSON
            ;;
    esac
    if $MPIEXEC -n "$ranks" "$@" >"$output" 2>"$log"; then
        echo "mdtest accepted a failed summary write: $mode, $ranks ranks" >&2
        return 1
    fi
    grep -q "Cannot write summary output to" "$output" "$log"
    echo "mdtest summary write-failure case passed: $mode, $ranks ranks"
}

run_default 1 1
run_default 2 2
run_text_file 1 1
run_text_file 2 2
run_json_file 1 1
run_json_file 2 2
run_json_stdout 1 1
run_json_stdout 2 2

if [ -c /dev/full ] && [ -w /dev/full ]; then
    for ranks in 1 2; do
        run_write_failure "$ranks" text-file
        run_write_failure "$ranks" json-file
        run_write_failure "$ranks" json-stdout
    done
fi

echo "mdtest summary shell regression cases passed"
