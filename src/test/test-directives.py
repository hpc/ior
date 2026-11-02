#!/usr/bin/env python3
"""Check complete directive values through the executable and script reader."""

import json
import os
from pathlib import Path
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

with tempfile.TemporaryDirectory(prefix="ior-directives-") as temporary:
    root = Path(temporary)
    common = ["-a", "POSIX", "-b", "8k", "-t", "4k", "-s", "2", "-G", "123",
              "-w", "-r", "-W", "-R", "-k"]
    control = root / "control"
    result = run(common + ["-o", str(control)])
    if result.returncode != 0:
        raise AssertionError(result.stdout)
    expected = control.read_bytes()
    for form in ("command", "lf", "crlf"):
        output = root / (form + " data = value")
        directives = ("\tTeStFiLe \t= " + str(output)
                      + " \t, blockSize = 8k, transferSize = 4k # ignored=1,more=2")
        if form == "command":
            args = ["-O", directives]
        else:
            script = root / (form + ".ior")
            lines = ["IOR START", "\t# indented comment", directives, "RUN", "IOR STOP"]
            newline = "\r\n" if form == "crlf" else "\n"
            script.write_bytes((newline.join(lines) + newline).encode())
            args = ["-f", str(script)]
        result = run(common + args)
        if result.returncode != 0:
            raise AssertionError(result.stdout)
        if not output.exists():
            raise AssertionError("directive output path missing: %s; actual files: %s"
                                 % (output, sorted(p.name for p in root.iterdir())))
        if output.read_bytes() != expected:
            raise AssertionError("directive output differs from -o control: " + form)
        print("%s: complete filename and data verified" % form)

    summary = root / "summary with spaces.json"
    result = run(common + ["-o", str(root / "summary-data"), "-O",
                          "summaryFormat=JSON, summaryFile = " + str(summary) + " \t"])
    if result.returncode != 0:
        raise AssertionError(result.stdout)
    with summary.open() as stream:
        json.load(stream)
    print("summaryFile: complete filename and JSON verified")

    for directive in ("platform=", "=value", "platform", "bad name=value",
                      "platform=" + "x" * 4096, "x" * 4096 + "=value"):
        result = run(["-a", "POSIX", "-O", directive])
        if result.returncode == 0 or "Syntax error in configuration options:" not in result.stdout:
            raise AssertionError("invalid directive was not diagnosed: %r\n%s"
                                 % (directive[:80], result.stdout))
    print("empty, malformed, and overlong directives rejected")
