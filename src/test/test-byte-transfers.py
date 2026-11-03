#!/usr/bin/env python3
"""Exercise transfers that do not fill an integer number of 64-bit words."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


binary = str(Path(sys.argv[1]).resolve())
env = os.environ.copy()
env["LC_ALL"] = "C"


def run(args):
    proc = subprocess.run([binary] + args, env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, universal_newlines=True,
                          timeout=60)
    if proc.returncode != 0:
        raise AssertionError(proc.stdout)
    return proc.stdout


help_text = subprocess.run([binary, "-h"], env=env, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, universal_newlines=True,
                           timeout=60).stdout
if "Module " not in help_text:
    raise AssertionError(help_text)
backends = [("POSIX", []), ("MMAP", []), ("MPIIO", []),
            ("MPIIO", ["--mpiio.useFileView"]),
            ("MPIIO", ["--mpiio.useFileView", "--mpiio.useStridedDatatype"]),
            ("HDF5", []), ("NCMPI", [])]
with tempfile.TemporaryDirectory(prefix="ior-byte-transfers-") as temporary:
    root = Path(temporary)
    for index, (api, flags) in enumerate(backends):
        if "Module " + api not in help_text:
            continue
        for transfer in (1, 13, 50):
            output = root / ("data-%d-%d" % (index, transfer))
            common = ["-a", api, "-o", str(output), "-b", str(transfer * 4),
                      "-t", str(transfer), "-s", "2", "-G", "123"] + flags
            run(common + ["-w", "-r", "-W", "-R", "-k"])
            # Reopen the existing dataset rather than relying only on write-check.
            run(common + ["-R", "-k"])
            print("%s %s: %d-byte transfers verified" % (api, flags, transfer))

    if "Module POSIX" in help_text:
        for pattern in ("o", "i", "r"):
            run(["-a", "POSIX", "-o", str(root / ("pattern-" + pattern)),
                 "-b", "52", "-t", "13", "-s", "2", "-G", "123",
                 "-l", pattern, "-w", "-W", "-R"])
            print("POSIX: 13-byte packet type %s verified" % pattern)
