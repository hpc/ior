#!/usr/bin/env python3
import json
import math
import re
import subprocess


FIXTURES = {
    "full": (4, {"write": [1, 2, 3, 4], "read": [2, 4, 6, 8]}, 2.0),
    "partial": (4, {"write": [1, 3, 4], "read": [2]}, 2.0),
    "write-only": (4, {"write": [1, 3]}, 2.0),
    "read-only": (4, {"read": [4, 8]}, 2.0),
    "single": (1, {"write": [1], "read": [2]}, 2.0),
    "rates": (4, {"write": [1, 2, 3, 4], "read": [2, 4, 6, 8]}, 4.0),
    "zero": (4, {"write": [], "read": []}, 2.0),
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


def check_json(fixture, repetitions, expected, ops_factor):
    output = subprocess.check_output(
        ["./testsummary", fixture, "json"], universal_newlines=True
    )
    data = json.loads(output, parse_constant=reject_constant)
    assert_finite_json(data)

    long_rows = {row["operation"]: row for row in data["long"]}
    assert set(long_rows) == set(expected)
    for operation, bw_samples in expected.items():
        row = long_rows[operation]
        ops_samples = [sample * ops_factor for sample in bw_samples]
        assert row["repetitions"] == repetitions
        assert row["completedRepetitions"] == len(bw_samples)
        for metric in ("bwMaxMIB", "bwMinMIB", "bwMeanMIB", "bwStdMIB",
                       "OPsMax", "OPsMin", "OPsMean", "OPsSD", "MeanTime",
                       "xsizeMiB"):
            assert (row[metric] is None) == (not bw_samples), (
                fixture, operation, metric, row[metric]
            )
        if bw_samples:
            for prefix, samples in (("bw", bw_samples), ("OPs", ops_samples)):
                suffixes = {
                    "Max": expected_max(samples),
                    "Min": min(samples),
                    "Mean": sum(samples) / len(samples),
                    "SD": expected_sd(samples),
                }
                if prefix == "bw":
                    metric_names = {
                        "Max": "bwMaxMIB",
                        "Min": "bwMinMIB",
                        "Mean": "bwMeanMIB",
                        "SD": "bwStdMIB",
                    }
                else:
                    metric_names = {
                        "Max": "OPsMax",
                        "Min": "OPsMin",
                        "Mean": "OPsMean",
                        "SD": "OPsSD",
                    }
                for suffix, expected_value in suffixes.items():
                    metric = metric_names[suffix]
                    assert math.isclose(
                        row[metric], expected_value, rel_tol=1e-4, abs_tol=1e-4
                    ), (fixture, operation, metric, row[metric], expected_value)

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


def check_text(fixture, repetitions, expected, ops_factor):
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
    for operation, bw_samples in expected.items():
        ops_samples = [sample * ops_factor for sample in bw_samples]
        fields = rows[operation]
        assert int(fields[15]) == repetitions
        assert int(fields[16]) == len(bw_samples)
        if bw_samples:
            metric_groups = (
                (1, bw_samples),
                (5, ops_samples),
            )
            for first_index, samples in metric_groups:
                expected_values = (
                    expected_max(samples),
                    min(samples),
                    sum(samples) / len(samples),
                    expected_sd(samples),
                )
                for offset, expected_value in enumerate(expected_values):
                    assert math.isclose(
                        float(fields[first_index + offset]),
                        expected_value,
                        rel_tol=1e-2,
                        abs_tol=5e-3,
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
            assert math.isclose(float(row[1]), expected_max(samples), rel_tol=1e-2)
        else:
            assert row[1] == row[2] == "NA"
    assert not re.search(r"(?i)(?<![a-z])[-+]?(?:nan|inf(?:inity)?)(?![a-z])", output)


for fixture, (repetitions, expected, ops_factor) in FIXTURES.items():
    check_json(fixture, repetitions, expected, ops_factor)
    check_text(fixture, repetitions, expected, ops_factor)

print("summary output regression cases passed")
