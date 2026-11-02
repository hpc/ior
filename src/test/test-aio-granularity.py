#!/usr/bin/env python3
"""Verify the Linux AIO submission granularity bounds."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


ior, mdtest = (str(Path(arg).resolve()) for arg in sys.argv[1:3])
env = os.environ.copy()
env["LC_ALL"] = "C"
help_text = subprocess.run([ior, "-h"], stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, universal_newlines=True,
                           env=env, timeout=60).stdout
if "Module AIO" not in help_text:
    print("AIO backend unavailable; skipping granularity test")
    sys.exit(77)

with tempfile.TemporaryDirectory(prefix="ior-aio-granularity-") as directory:
    for binary in (ior, mdtest):
        for value in (0, -1, 9):
            if binary == ior:
                args = ["-w", "-b", "4k", "-t", "4k", "-o", os.path.join(directory, "data")]
            else:
                args = ["-n", "1", "-d", directory]
            command = [binary, "-a", "AIO"] + args + ["--aio.max-pending=8", "--aio.granularity=%d" % value]
            proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  universal_newlines=True, env=env, timeout=60)
            expected = ("AIO granularity must be at least 1, is %d" % value if value < 1
                        else "AIO granularity must be <= max-pending, is 9 > 8")
            if proc.returncode == 0 or expected not in proc.stdout or "AddressSanitizer" in proc.stdout:
                raise AssertionError("%s: status=%d, expected %s\n%s" %
                                     (command, proc.returncode, expected, proc.stdout))
            print("%s: rejected granularity %d" % (Path(binary).name, value))

    for value in (1, 8):
        path = os.path.join(directory, "data-%d" % value)
        command = [ior, "-a", "AIO", "-w", "-r", "-W", "-R", "-k", "-b", "16k", "-t", "4k",
                   "-G", "123", "-o", path, "--aio.max-pending=8", "--aio.granularity=%d" % value]
        proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              universal_newlines=True, env=env, timeout=60)
        if proc.returncode != 0 or os.path.getsize(path) != 16384:
            raise AssertionError("%s: status=%d\n%s" % (command, proc.returncode, proc.stdout))
        print("ior: accepted granularity %d" % value)
