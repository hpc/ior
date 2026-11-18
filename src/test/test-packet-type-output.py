#!/usr/bin/env python3
"""Check IOR packet labels in JSON summaries and verbose text output."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


CASES = (("t", 0), ("o", 1), ("i", 2), ("r", 3), (None, 0))


def find_packet_fields(value):
    found = []
    if isinstance(value, dict):
        if "dataPacketType" in value or "data packet type" in value:
            found.append((value.get("dataPacketType"), value.get("data packet type")))
        for child in value.values():
            found.extend(find_packet_fields(child))
    elif isinstance(value, list):
        for child in value:
            found.extend(find_packet_fields(child))
    return found


def read_json_fields(path):
    with path.open(encoding="utf-8") as stream:
        data = json.load(stream)
    return find_packet_fields(data)


def read_text_fields(output):
    numbers = re.findall(r"(?m)^\s*dataPacketType\s*:\s*([0-9]+)\s*$", output)
    labels = re.findall(r"(?m)^\s*data packet type\s*:\s*(\S+)\s*$", output)
    if len(numbers) != 1 or len(labels) != 1:
        raise ValueError("expected exactly one numeric field and one text field")
    return [(int(numbers[0]), labels[0])]


def check_case(command, mode, number, output_format, directory):
    label = mode if mode is not None else "t"
    argv = command + ["-a", "DUMMY", "-w", "-i", "1", "-s", "1", "-b", "65536",
                      "-t", "65536", "-o", "ior-packet-type-data"]
    if mode is not None:
        argv += ["-O", "dataPacketType=" + mode]
    if output_format == "json":
        argv += ["-O", "summaryFormat=JSON", "-O", "summaryFile=packet-summary.json"]
    else:
        argv += ["-v", "-v", "-v"]
    env = os.environ.copy()
    env["LC_ALL"] = "C"
    proc = subprocess.run(argv, cwd=directory, env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, encoding="utf-8", errors="replace", timeout=60)
    if proc.returncode != 0:
        raise RuntimeError("IOR exited with status %d" % proc.returncode)
    if output_format == "json":
        fields = read_json_fields(Path(directory) / "packet-summary.json")
    else:
        fields = read_text_fields(proc.stdout)
    if (len(fields) != 1 or type(fields[0][0]) is not int or
            fields[0] != (number, label)):
        raise ValueError("expected %r, observed %r" % ((number, label), fields))
    return fields[0]


def main():
    if len(sys.argv) < 2:
        raise SystemExit("usage: test-packet-type-output.py IOR_COMMAND [ARGS...]")
    command = sys.argv[1:]
    executable = command[0]
    if os.path.dirname(executable):
        command[0] = str(Path(executable).resolve())
    else:
        resolved = shutil.which(executable)
        if resolved is not None:
            command[0] = str(Path(resolved).resolve())
    failures = 0
    for output_format in ("json", "text"):
        for mode, number in CASES:
            name = mode if mode is not None else "default"
            try:
                with tempfile.TemporaryDirectory(prefix="ior-packet-type-") as directory:
                    actual = check_case(command, mode, number, output_format, directory)
                print("PASS %s %s: %d %s" % (output_format, name, actual[0], actual[1]), flush=True)
            except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as exc:
                failures += 1
                print("FAIL %s %s: %s: %s" % (output_format, name, type(exc).__name__, exc), flush=True)
    if failures:
        print("%d of %d packet output cases failed" % (failures, len(CASES) * 2))
        return 1
    print("all %d packet output cases passed" % (len(CASES) * 2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
