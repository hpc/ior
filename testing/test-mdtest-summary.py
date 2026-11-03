#!/usr/bin/env python3
import json
import math
import sys


def reject_constant(value):
    raise ValueError(f"non-standard JSON constant: {value}")


def check_metric(metric, label):
    assert isinstance(metric, dict), label
    assert metric["unit"] in {"ops/sec", "ms/op"}, label
    for key in ("max", "min", "mean", "stddev"):
        value = metric[key]
        assert value is None or (isinstance(value, (int, float)) and math.isfinite(value)), (label, key, value)


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: test-mdtest-summary.py SUMMARY.json ITERATIONS TASKS")
    path, expected_iterations, expected_tasks = sys.argv[1:]
    expected_iterations = int(expected_iterations)
    expected_tasks = int(expected_tasks)
    with open(path, encoding="utf-8") as stream:
        data = json.load(stream, parse_constant=reject_constant)

    assert isinstance(data, dict) and isinstance(data["summary"], list), "summary must be an array"
    assert data["summary"], "summary must contain at least one operation"
    for row in data["summary"]:
        assert row["tasks"] == expected_tasks, row
        assert row["iterations"] == expected_iterations, row
        assert isinstance(row["operation"], str) and row["operation"], row
        metrics = [value for key, value in row.items() if key in ("rate", "time")]
        assert metrics, row
        for metric in metrics:
            check_metric(metric, row["operation"])
            if expected_iterations == 1:
                assert metric["stddev"] == 0, (row["operation"], metric)
    print("mdtest summary JSON regression cases passed")


if __name__ == "__main__":
    main()
