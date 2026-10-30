#!/bin/sh
# Run from the build directory with the native HDF5 VOL connector.
# IOR_MPIRUN may contain a launcher and rank count, e.g. "mpiexec -n 2".
set -eu

IOR_BIN=${IOR_BIN:-./src/ior}
IOR_MPIRUN=${IOR_MPIRUN:-}
work=$(mktemp -d "${TMPDIR:-/tmp}/ior-hdf5-delete.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

run_ior() {
    if ! $IOR_MPIRUN "$IOR_BIN" -a HDF5 -b 32k -t 8k -s 2 -G 123 \
        -o "$work/data" "$@" > "$work/run.log" 2>&1; then
        cat "$work/run.log"
        return 1
    fi
}

check_removed() {
    for file in "$work"/data*; do
        if [ -e "$file" ]; then
            echo "HDF5 test file was not removed: $file" >&2
            return 1
        fi
    done
}

for layout in shared file-per-process; do
    set --
    if [ "$layout" = file-per-process ]; then set -- -F; fi

    run_ior "$@" -i 2 -w -r -W -R
    check_removed
    echo "PASS $layout repeated write/read and removal"

    run_ior "$@" -i 1 -w -r -W -R -k
    found=0
    for file in "$work"/data*; do
        if [ -f "$file" ]; then found=1; fi
    done
    if [ "$found" != 1 ]; then
        echo "HDF5 keep-file control did not retain any data" >&2
        exit 1
    fi
    echo "PASS $layout keep-file control"

    run_ior "$@" -i 1 -r -R
    check_removed
    echo "PASS $layout retained data verification and removal"
done
