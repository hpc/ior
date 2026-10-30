#!/usr/bin/env python3
"""Check that verification errors produce a nonzero process exit status."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


binary = str(Path(sys.argv[1]).resolve())
env = os.environ.copy()
env["LC_ALL"] = "C"
help_text = subprocess.run([binary, "-h"], stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, universal_newlines=True,
                           env=env, timeout=60).stdout
if "Module POSIX" not in help_text:
    print("POSIX backend unavailable; skipping verification exit test")
    sys.exit(77)

with tempfile.TemporaryDirectory(prefix="ior-exit-") as directory:
    root = Path(directory)
    original = root / "original"
    common = ["-a", "POSIX", "-k", "-b", "257k", "-t", "1k", "-G", "123"]
    subprocess.run([binary, "-w", *common, "-o", str(original)], check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                   universal_newlines=True, env=env, timeout=60)

    for count in (0, 1, 2, 127, 128, 255, 256, 257):
        data = root / ("case-%d" % count)
        shutil.copyfile(original, data)
        with data.open("r+b") as stream:
            for index in range(count):
                stream.seek(index * 1024 + 8)
                value = stream.read(1)
                stream.seek(index * 1024 + 8)
                stream.write(bytes([value[0] ^ 1]))
        proc = subprocess.run([binary, "-R", *common, "-o", str(data)],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              universal_newlines=True, env=env, timeout=60)
        match = re.search(r"Incorrect data on read \((\d+) errors found\)", proc.stdout)
        reported = int(match.group(1)) if match else 0
        expected_status = min(count, 255)
        if reported != count or proc.returncode != expected_status:
            raise AssertionError("corrupt blocks=%d, reported=%d, status=%d, expected=%d\n%s"
                                 % (count, reported, proc.returncode, expected_status,
                                    proc.stdout))
        print("corrupt blocks=%d, reported=%d, status=%d" %
              (count, reported, proc.returncode))
