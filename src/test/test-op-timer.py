#!/usr/bin/env python3
"""Check that per-operation CSV failures cannot report benchmark success."""

import csv
import os
from pathlib import Path
import subprocess
import sys
import tempfile


binaries = [Path(arg).resolve() for arg in sys.argv[1:]]
env = os.environ.copy()
env["LC_ALL"] = "C"


def run(binary, directory, prefix, large=False):
    if binary.name == "ior":
        args = ["-a", "DUMMY", "-w", "-b", "8000000" if large else "32",
                "-t", "8", "-i", "1", "-o", "data",
                "-O", "savePerOpDataCSV=" + str(prefix)]
    else:
        args = ["-a", "DUMMY", "-C", "-F", "-n", "2", "-i", "1",
                "--savePerOpDataCSV=" + str(prefix)]
    return subprocess.run([str(binary)] + args, cwd=str(directory),
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          universal_newlines=True, env=env, timeout=120)


def expect_failure(proc, diagnostic):
    if proc.returncode == 0 or diagnostic not in proc.stdout:
        raise AssertionError("status=%d, expected %r\n%s" %
                             (proc.returncode, diagnostic, proc.stdout))


with tempfile.TemporaryDirectory(prefix="ior-op-timer-") as temporary:
    root = Path(temporary)
    for binary in binaries:
        directory = root / binary.name
        directory.mkdir()
        prefix = directory / "timing"
        suffix = ("-0-00000.csv" if binary.name == "ior"
                  else "-File creation-00000.csv")
        output = Path(str(prefix) + suffix)

        proc = run(binary, directory, prefix)
        if proc.returncode != 0:
            raise AssertionError(proc.stdout)
        with output.open() as stream:
            rows = list(csv.reader(stream))
        expected_rows = 5 if binary.name == "ior" else 3
        if rows[0] != ["time", "runtime", "tp"] or len(rows) != expected_rows:
            raise AssertionError(rows)
        for row in rows[1:]:
            if len(row) != 3 or any(float(value) < 0 for value in row):
                raise AssertionError(row)
        print("%s: normal CSV contents passed" % binary.name)
        output.unlink()

        missing = directory / "missing" / "timing"
        proc = run(binary, directory, missing)
        expect_failure(proc, "Cannot create OpTimer file '%s%s':" % (missing, suffix))
        print("%s: missing output directory diagnosed" % binary.name)

        if Path("/dev/full").is_char_device() and os.access("/dev/full", os.W_OK):
            output.symlink_to("/dev/full")
            proc = run(binary, directory, prefix)
            expect_failure(proc, "Cannot close OpTimer file:")
            print("%s: buffered CSV failure diagnosed" % binary.name)
            if binary.name == "ior":
                proc = run(binary, directory, prefix, large=True)
                expect_failure(proc, "Cannot write records to OpTimer file:")
                print("ior: automatic record-buffer flush failure diagnosed")
            output.unlink()
        else:
            print("/dev/full unavailable; write-failure cases not exercised")
