#!/bin/bash

# Regression tests for stonewalling with MPI-IO file views and strided
# datatypes. Coalescing is valid only without interruption limits.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IOR_BIN_DIR="${IOR_BIN_DIR:-${ROOT}/../src}"
IOR_MPIRUN="${IOR_MPIRUN:-mpiexec -np}"
IOR_TEST_TIMEOUT="${IOR_TEST_TIMEOUT:-60}"
TEST_ROOT="${IOR_STRIDED_TEST_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/ior-mpiio-strided.XXXXXX")}"
CLEAN_TEST_ROOT=1
if [[ -n "${IOR_STRIDED_TEST_DIR:-}" ]]; then
  CLEAN_TEST_ROOT=0
fi

mkdir -p "${TEST_ROOT}"
if (( CLEAN_TEST_ROOT )); then
  trap 'rm -rf "${TEST_ROOT}"' EXIT
fi
read -r -a MPI_PREFIX <<< "${IOR_MPIRUN}"
ERRORS=0

fail() {
  echo "ERR  $*"
  ERRORS=$((ERRORS + 1))
}

run_ior() {
  local name="$1" ranks="$2" data_file="$3" summary_file="$4" log_file="$5"
  shift 5
  local command=(timeout "${IOR_TEST_TIMEOUT}" "${MPI_PREFIX[@]}" "${ranks}"
                 "${IOR_BIN_DIR}/ior" "$@" -o "${data_file}"
                 -O "summaryFile=${summary_file}" -O summaryFormat=JSON)
  echo "RUN  ${name}: ${command[*]}"
  if ! "${command[@]}" >"${log_file}" 2>&1; then
    fail "${name} failed; see ${log_file}"
    return 1
  fi
}

check_summary_bytes() {
  local name="$1" summary_file="$2" operation="$3" expected_bytes="$4"
  if ! python3 - "${summary_file}" "${operation}" "${expected_bytes}" <<'PY'
import json, sys
from decimal import Decimal, ROUND_HALF_EVEN
summary_file, operation, expected_bytes = sys.argv[1:4]
with open(summary_file) as stream:
    document = json.load(stream)
matches = []
def visit(value):
    if isinstance(value, dict):
        if value.get("operation") == operation and "xsizeMiB" in value:
            matches.append(value)
        for child in value.values():
            visit(child)
    elif isinstance(value, list):
        for child in value:
            visit(child)
visit(document)
if len(matches) != 1:
    raise SystemExit("expected one {0} summary, found {1}".format(operation, len(matches)))
precision = Decimal("0.0001")
actual_mib = Decimal(str(matches[0]["xsizeMiB"])).quantize(precision, rounding=ROUND_HALF_EVEN)
expected_mib = (Decimal(int(expected_bytes)) / Decimal(1024 * 1024)).quantize(
    precision, rounding=ROUND_HALF_EVEN
)
if actual_mib != expected_mib:
    raise SystemExit(
        "reported {0} MiB, expected {1} MiB from {2} bytes".format(
            actual_mib, expected_mib, expected_bytes
        )
    )
PY
  then
    fail "${name}: aggregate transfer size does not match expected bytes"
    return 1
  fi
}

check_file_bytes() {
  local name="$1" file="$2" expected_bytes="$3" actual_bytes
  actual_bytes="$(stat -c %s "${file}")"
  if [[ "${actual_bytes}" != "${expected_bytes}" ]]; then
    fail "${name}: file has ${actual_bytes} bytes, expected ${expected_bytes}"
    return 1
  fi
}

RANKS="${IOR_TEST_RANKS:-2}"
SEGMENTS=32
TRANSFER_SIZE=1024
FULL_BYTES=$((RANKS * SEGMENTS * TRANSFER_SIZE))
COMMON=(--mpiio.useFileView --mpiio.useStridedDatatype)

NO_DEADLINE_DATA="${TEST_ROOT}/no-deadline.data"
NO_DEADLINE_SUMMARY="${TEST_ROOT}/no-deadline.json"
if run_ior no-deadline "${RANKS}" "${NO_DEADLINE_DATA}" \
     "${NO_DEADLINE_SUMMARY}" "${TEST_ROOT}/no-deadline.log" \
     -a MPIIO -w -k -i 1 -s "${SEGMENTS}" -b "${TRANSFER_SIZE}" \
     -t "${TRANSFER_SIZE}" -G 1 "${COMMON[@]}"; then
  check_summary_bytes no-deadline "${NO_DEADLINE_SUMMARY}" write "${FULL_BYTES}"
  check_file_bytes no-deadline "${NO_DEADLINE_DATA}" "${FULL_BYTES}"
fi

DEADLINE_DATA="${TEST_ROOT}/deadline.data"
DEADLINE_SUMMARY="${TEST_ROOT}/deadline.json"
DEADLINE_STATUS="${TEST_ROOT}/deadline.status"
if run_ior deadline "${RANKS}" "${DEADLINE_DATA}" \
     "${DEADLINE_SUMMARY}" "${TEST_ROOT}/deadline.log" \
     -a MPIIO -w -k -i 1 -s "${SEGMENTS}" -b "${TRANSFER_SIZE}" \
     -t "${TRANSFER_SIZE}" -D 1 -O interIODelay=200000 \
     -O stoneWallingWearOut=1 -O "stoneWallingStatusFile=${DEADLINE_STATUS}" \
     "${COMMON[@]}"; then
  if [[ ! -s "${DEADLINE_STATUS}" ]]; then
    fail "deadline: stonewalling status file is missing or empty"
  else
    DEADLINE_PAIRS="$(cat "${DEADLINE_STATUS}")"
    DEADLINE_BYTES=$((RANKS * DEADLINE_PAIRS * TRANSFER_SIZE))
    if (( DEADLINE_PAIRS < 1 || DEADLINE_PAIRS >= SEGMENTS )); then
      fail "deadline: pair count ${DEADLINE_PAIRS} did not stop early"
    fi
    check_summary_bytes deadline "${DEADLINE_SUMMARY}" write "${DEADLINE_BYTES}"
    check_file_bytes deadline "${DEADLINE_DATA}" "${DEADLINE_BYTES}"
  fi
fi

COLLECTIVE_DATA="${TEST_ROOT}/collective.data"
COLLECTIVE_SUMMARY="${TEST_ROOT}/collective.json"
COLLECTIVE_STATUS="${TEST_ROOT}/collective.status"
if run_ior collective "${RANKS}" "${COLLECTIVE_DATA}" \
     "${COLLECTIVE_SUMMARY}" "${TEST_ROOT}/collective.log" \
     -a MPIIO -w -c -k -i 1 -s "${SEGMENTS}" -b "${TRANSFER_SIZE}" \
     -t "${TRANSFER_SIZE}" -D 1 -O interIODelay=200000 \
     -O stoneWallingWearOut=1 -O "stoneWallingStatusFile=${COLLECTIVE_STATUS}" \
     "${COMMON[@]}"; then
  if [[ ! -s "${COLLECTIVE_STATUS}" ]]; then
    fail "collective: stonewalling status file is missing or empty"
  else
    COLLECTIVE_PAIRS="$(cat "${COLLECTIVE_STATUS}")"
    COLLECTIVE_BYTES=$((RANKS * COLLECTIVE_PAIRS * TRANSFER_SIZE))
    if (( COLLECTIVE_PAIRS < 1 || COLLECTIVE_PAIRS >= SEGMENTS )); then
      fail "collective: pair count ${COLLECTIVE_PAIRS} did not stop early"
    fi
    check_summary_bytes collective "${COLLECTIVE_SUMMARY}" write "${COLLECTIVE_BYTES}"
    check_file_bytes collective "${COLLECTIVE_DATA}" "${COLLECTIVE_BYTES}"
  fi
fi

READ_DATA="${TEST_ROOT}/read.data"
READ_SUMMARY="${TEST_ROOT}/read.json"
READ_STATUS="${TEST_ROOT}/read.status"
READ_PAIRS=$((SEGMENTS / 2))
READ_BYTES=$((RANKS * READ_PAIRS * TRANSFER_SIZE))
printf "%s\n" "${READ_PAIRS}" > "${READ_STATUS}"
# Seed the replay fixture through POSIX so it tests the bounded read itself.
if run_ior status-prepare "${RANKS}" "${READ_DATA}" \
     "${TEST_ROOT}/read-prepare.json" "${TEST_ROOT}/read-prepare.log" \
     -a POSIX -w -k -i 1 -s "${SEGMENTS}" -b "${TRANSFER_SIZE}" \
     -t "${TRANSFER_SIZE}" -G 1; then
  if ! python3 - "${READ_DATA}" "${READ_BYTES}" <<'PY'
import sys
data_file, valid_bytes = sys.argv[1], int(sys.argv[2])
with open(data_file, "r+b") as stream:
    stream.seek(valid_bytes)
    corrupted = stream.read()
    stream.seek(valid_bytes)
    stream.write(bytes(value ^ 0xFF for value in corrupted))
PY
  then
    fail "status-read: could not corrupt the suffix fixture"
  fi
  if run_ior status-read "${RANKS}" "${READ_DATA}" \
       "${READ_SUMMARY}" "${TEST_ROOT}/read.log" \
       -a MPIIO -R -k -i 1 -s "${SEGMENTS}" -b "${TRANSFER_SIZE}" \
       -t "${TRANSFER_SIZE}" -G 1 -O "stoneWallingStatusFile=${READ_STATUS}" \
       "${COMMON[@]}"; then
    check_summary_bytes status-read "${READ_SUMMARY}" read "${READ_BYTES}"
    check_file_bytes status-read "${READ_DATA}" "${FULL_BYTES}"
  fi
fi

VERIFY_SEGMENTS=4
VERIFY_BLOCK=4096
VERIFY_BYTES=$((RANKS * VERIFY_SEGMENTS * VERIFY_BLOCK))
for packet in t o i r; do
  VERIFY_DATA="${TEST_ROOT}/verify-${packet}.data"
  VERIFY_SUMMARY="${TEST_ROOT}/verify-${packet}.json"
  VERIFY_FLAGS=()
  # Exercise timestamp coalescing collectively; see the independent-I/O #566.
  if [[ "${packet}" == t ]]; then VERIFY_FLAGS=(-c); fi
  if run_ior "verify-${packet}" "${RANKS}" "${VERIFY_DATA}" \
       "${VERIFY_SUMMARY}" "${TEST_ROOT}/verify-${packet}.log" \
       -a MPIIO -w -W -R -k -i 1 -s "${VERIFY_SEGMENTS}" \
       -b "${VERIFY_BLOCK}" -t "${TRANSFER_SIZE}" -G 1 \
       -O "dataPacketType=${packet}" "${VERIFY_FLAGS[@]}" "${COMMON[@]}"; then
    check_summary_bytes "verify-${packet}" "${VERIFY_SUMMARY}" write "${VERIFY_BYTES}"
    check_summary_bytes "verify-${packet}" "${VERIFY_SUMMARY}" read "${VERIFY_BYTES}"
    check_file_bytes "verify-${packet}" "${VERIFY_DATA}" "${VERIFY_BYTES}"
    run_ior "verify-posix-${packet}" "${RANKS}" "${VERIFY_DATA}" \
      "${TEST_ROOT}/verify-posix-${packet}.json" "${TEST_ROOT}/verify-posix-${packet}.log" \
      -a POSIX -R -k -i 1 -s "${VERIFY_SEGMENTS}" \
      -b "${VERIFY_BLOCK}" -t "${TRANSFER_SIZE}" -G 1 -O "dataPacketType=${packet}" || true
  fi
done

PERPROC_DATA="${TEST_ROOT}/verify-perproc.data"
if run_ior verify-perproc "${RANKS}" "${PERPROC_DATA}" \
     "${TEST_ROOT}/verify-perproc.json" "${TEST_ROOT}/verify-perproc.log" \
     -a MPIIO -w -F -k -i 1 -s "${VERIFY_SEGMENTS}" \
     -b "${VERIFY_BLOCK}" -t "${TRANSFER_SIZE}" -G 1 "${COMMON[@]}"; then
  for ((process = 0; process < RANKS; process++)); do
    printf -v process_file '%s.%08d' "${PERPROC_DATA}" "${process}"
    check_file_bytes "verify-perproc-${process}" "${process_file}" \
      "$((VERIFY_SEGMENTS * VERIFY_BLOCK))"
  done
  run_ior verify-perproc-posix "${RANKS}" "${PERPROC_DATA}" \
    "${TEST_ROOT}/verify-perproc-posix.json" "${TEST_ROOT}/verify-perproc-posix.log" \
    -a POSIX -R -F -k -i 1 -s "${VERIFY_SEGMENTS}" \
    -b "${VERIFY_BLOCK}" -t "${TRANSFER_SIZE}" -G 1 || true
fi

run_ior verify-shuffled "${RANKS}" "${TEST_ROOT}/verify-shuffled.data" \
  "${TEST_ROOT}/verify-shuffled.json" "${TEST_ROOT}/verify-shuffled.log" \
  -a MPIIO -w -r -Z -Z -X 1 -k -i 1 -s "${VERIFY_SEGMENTS}" \
  -b "${VERIFY_BLOCK}" -t "${TRANSFER_SIZE}" -G 1 "${COMMON[@]}" || true

if (( ERRORS != 0 )); then
  echo "FAILED: ${ERRORS} error(s)"
  exit 1
fi

echo "PASSED"
