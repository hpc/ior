#!/usr/bin/env python3
"""Check MMAP's POSIX delegation and preservation of its own advice options."""

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


if "Module MMAP" not in run(["-h"]).stdout:
    sys.exit(77)

with tempfile.TemporaryDirectory(prefix="mmap-options-", dir=".") as temporary:
    root = Path(temporary).resolve()
    options = [[], ["--mmap.madv_pattern"], ["--mmap.madv_dont_need"],
               ["--mmap.madv_pattern", "--mmap.madv_dont_need"], ["-e"], ["-Y"]]
    for index, flags in enumerate(options):
        output = root / ("data-%d" % index)
        common = ["-a", "MMAP", "-b", "16k", "-t", "4k", "-s", "2",
                  "-G", "123", "-o", str(output)] + flags
        for mode in (["-w", "-r", "-W", "-R", "-k"], ["-R"]):
            proc = run(common + mode)
            if proc.returncode != 0:
                raise AssertionError(proc.stdout)
        print("MMAP advice/sync options %s passed" % flags)

    # Multiple RUN entries exercise backend option cloning between tests.
    config = root / "multi.ior"
    config.write_text("IOR START\napi=MMAP\nblockSize=16k\ntransferSize=4k\n"
                      "segmentCount=2\nwriteFile=1\nreadFile=1\ncheckWrite=1\n"
                      "checkRead=1\nsetTimeStampSignature=123\n"
                      "testFile=" + str(root / "multi.data") + "\nRUN\nRUN\nIOR STOP\n")
    proc = run(["-f", str(config)])
    if proc.returncode != 0:
        raise AssertionError(proc.stdout)
    print("MMAP options cloned between script tests")
