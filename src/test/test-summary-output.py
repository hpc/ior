#!/usr/bin/env python3
import json
import math
import re
import subprocess


FIXTURES = {
    "full": (4, {"write": [1, 2, 3, 4], "read": [2, 4, 6, 8]}),
    "partial": (4, {"write": [1, 3, 4], "read": [2]}),
    "write-only": (4, {"write": [1, 3]}),
    "read-only": (4, {"read": [4, 8]}),
    "single": (1, {"write": [1], "read": [2]}),
    "zero": (4, {"write": [], "read": []}),
}


def reject_constant(value):
    raise ValueError(f"non-standard JSON constant: {value}")


def assert_finite_json(value):
    if isinstance(value, dict):
        for child in value.values():
            assert_finite_json(child)
    elif isinstance(value, list):
        for child in value:
            assert_finite_json(child)
    elif isinstance(value, float):
        assert math.isfinite(value), f"non-finite numeric value: {value}"
    elif isinstance(value, str):
        assert value.lower() not in {"nan", "-nan", "inf", "-inf", "infinity", "-infinity"}


def expected_max(samples):
    return max(samples) if samples else None


def expected_sd(samples):
    if not samples:
        return None
    mean = sum(samples) / len(samples)
    return math.sqrt(sum((sample - mean) ** 2 for sample in samples) / len(samples))


def check_json(fixture, repetitions, expected):
    output = subprocess.check_output(
        ["./testsummary", fixture, "json"], universal_newlines=True
    )
    data = json.loads(output, parse_constant=reject_constant)
    assert_finite_json(data)

    long_rows = {row["operation"]: row for row in data["long"]}
    assert set(long_rows) == set(expected)
    for operation, samples in expected.items():
        row = long_rows[operation]
        assert row["repetitions"] == repetitions
        assert row["completedRepetitions"] == len(samples)
        for metric in ("bwMaxMIB", "bwMinMIB", "bwMeanMIB", "bwStdMIB",
                       "OPsMax", "OPsMin", "OPsMean", "OPsSD", "MeanTime",
                       "xsizeMiB"):
            assert (row[metric] is None) == (not samples), (fixture, operation, metric, row[metric])
        if samples:
            mean = sum(samples) / len(samples)
            standard_deviation = expected_sd(samples)
            for metric in ("bwMaxMIB", "OPsMax"):
                assert math.isclose(row[metric], max(samples), rel_tol=1e-4, abs_tol=1e-4)
            for metric in ("bwMinMIB", "OPsMin"):
                assert math.isclose(row[metric], min(samples), rel_tol=1e-4, abs_tol=1e-4)
            for metric in ("bwMeanMIB", "OPsMean"):
                assert math.isclose(row[metric], mean, rel_tol=1e-4, abs_tol=1e-4)
            for metric in ("bwStdMIB", "OPsSD"):
                assert math.isclose(
                    row[metric], standard_deviation, rel_tol=1e-4, abs_tol=1e-4
                )

    short = data["max"]
    for operation in ("write", "read"):
        samples = expected.get(operation)
        if samples is None:
            assert f"{operation}MiB" not in short
            continue
        assert short[f"{operation}CompletedRepetitions"] == len(samples)
        assert short[f"{operation}Repetitions"] == repetitions
        maximum = short[f"{operation}MiB"]
        assert (maximum is None) == (not samples)
        if samples:
            assert math.isclose(maximum, expected_max(samples), rel_tol=1e-12)


def check_text(fixture, repetitions, expected):
    output = subprocess.check_output(
        ["./testsummary", fixture, "text"], universal_newlines=True
    )
    assert any("completed" in line for line in output.splitlines())
    rows = {}
    for line in output.splitlines():
        match = re.match(r"^(write|read)\s", line)
        if match:
            rows[match.group(1)] = line.split()
    assert set(rows) == set(expected)
    for operation, samples in expected.items():
        fields = rows[operation]
        assert int(fields[15]) == repetitions
        assert int(fields[16]) == len(samples)
        if samples:
            mean = sum(samples) / len(samples)
            standard_deviation = expected_sd(samples)
            for index in (1, 5):
                assert math.isclose(float(fields[index]), max(samples), rel_tol=1e-2)
            for index in (2, 6):
                assert math.isclose(float(fields[index]), min(samples), rel_tol=1e-2)
            for index in (3, 7):
                assert math.isclose(float(fields[index]), mean, rel_tol=1e-2)
            for index in (4, 8):
                assert math.isclose(
                    float(fields[index]), standard_deviation, rel_tol=1e-2
                )
        else:
            assert all(value == "NA" for value in fields[1:10])

    short_rows = {}
    for line in output.splitlines():
        match = re.match(
            r"Max (Write|Read):\s+(NA|[0-9.]+) MiB/sec \((NA|[0-9.]+) MB/sec\), completed: (\d+)/(\d+)",
            line,
        )
        if match:
            short_rows[match.group(1).lower()] = match.groups()
    assert set(short_rows) == set(expected)
    for operation, samples in expected.items():
        row = short_rows[operation]
        assert int(row[3]) == len(samples)
        assert int(row[4]) == repetitions
        if samples:
            assert math.isclose(float(row[1]), max(samples), rel_tol=1e-2)
        else:
            assert row[1] == row[2] == "NA"
    assert not re.search(r"(?i)(?<![a-z])[-+]?(?:nan|inf(?:inity)?)(?![a-z])", output)


for fixture, (repetitions, expected) in FIXTURES.items():
    check_json(fixture, repetitions, expected)
    check_text(fixture, repetitions, expected)

print("summary output regression cases passed")
