#!/usr/bin/env python3
"""Verify CLI overrides and parameter inheritance across script tests."""

import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
env = os.environ.copy()
env["LC_ALL"] = "C"


def run(args):
    return subprocess.run([binary] + args, env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, universal_newlines=True,
                          timeout=60)


if "Module POSIX" not in run(["-h"]).stdout:
    sys.exit(77)

with tempfile.TemporaryDirectory(prefix="ior-script-inheritance-") as temporary:
    root = Path(temporary)
    cases = [
        ("command", None, ["from-cli"], [8192]),
        ("script-default", ["IOR START", "RUN", "IOR STOP"], ["from-cli"], [8192]),
        ("changed-options", ["IOR START", "platform=from-script", "RUN", "blockSize=16k", "RUN", "IOR STOP"], ["from-script"] * 2, [8192, 16384]),
        ("consecutive-runs", ["IOR START", "platform=from-script", "RUN", "RUN", "RUN", "IOR STOP"], ["from-script"] * 3, [8192] * 3),
        ("runs-then-options", ["IOR START", "platform=from-script", "RUN", "RUN", "blockSize=16k", "RUN", "IOR STOP"], ["from-script"] * 3, [8192, 8192, 16384]),
    ]
    for name, lines, platforms, blocks in cases:
        for form, newline in (("lf", "\n"), ("crlf", "\r\n")):
            summary = root / (name + "-" + form + ".json")
            output = root / (name + "-" + form + ".data")
            args = ["-a", "POSIX", "-b", "8k", "-t", "4k", "-G", "123", "-w", "-r", "-W", "-R", "-k",
                    "-o", str(output), "-O", "platform=from-cli,summaryFormat=JSON,summaryFile=" + str(summary)]
            if lines is not None:
                script = root / (name + "-" + form + ".ior")
                script.write_bytes((newline.join(lines) + newline).encode())
                args += ["-f", str(script)]
            result = run(args)
            if result.returncode != 0:
                raise AssertionError(result.stdout)
            with summary.open(encoding="utf-8") as stream:
                report = json.load(stream)
            tests = report["tests"]
            if [test["Parameters"]["platform"] for test in tests] != platforms:
                raise AssertionError("platform inheritance failed: " + name + "/" + form)
            if [test["Parameters"]["blockSize"] for test in tests] != blocks:
                raise AssertionError("script options or test count changed: " + name + "/" + form)
            if [test["TestID"] for test in tests] != list(range(len(blocks))):
                raise AssertionError("script test IDs changed: " + name + "/" + form)
            packet = b"".join(struct.pack("<Q", 123 + word) for word in range(512))
            if output.read_bytes() != packet * (blocks[-1] // 4096):
                raise AssertionError("script data changed: " + name + "/" + form)
    print("CLI overrides, LF/CRLF scripts, consecutive RUN, and later options verified")
