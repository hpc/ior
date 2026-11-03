#!/usr/bin/env python3
"""Check that MMAP sizes individual files independently of the MPI task count."""

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile


binary = str(Path(sys.argv[1]).resolve())
env = os.environ.copy()
env["LC_ALL"] = "C"
launcher = shlex.split(env.get("MPIEXEC", "mpiexec"))
help_text = subprocess.run([binary, "-h"], env=env, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, universal_newlines=True,
                           timeout=60).stdout
if "Module MMAP" not in help_text or not launcher or not shutil.which(launcher[0]):
    sys.exit(77)


with tempfile.TemporaryDirectory(prefix="mmap-size-", dir=".") as temporary:
    root = Path(temporary).resolve()
    for per_process in (False, True):
        output = root / ("per-process" if per_process else "shared")
        args = ["-a", "MMAP", "-b", "16k", "-t", "4k", "-s", "2",
                "-G", "123", "-k", "-o", str(output)]
        if per_process:
            args += ["-F"]
        for mode in (["-w", "-r", "-W", "-R"], ["-r", "-R"]):
            proc = subprocess.run(launcher + ["-n", "2", binary] + args + mode,
                                  env=env, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, universal_newlines=True,
                                  timeout=90)
            if proc.returncode != 0:
                raise AssertionError(proc.stdout)
        paths = sorted(root.glob(output.name + ".*")) if per_process else [output]
        if len(paths) != (2 if per_process else 1):
            raise AssertionError(paths)
        expected_size = 16 * 1024 * 2 * (1 if per_process else 2)
        for path in paths:
            if path.stat().st_size != expected_size:
                raise AssertionError("MMAP file size mismatch: %s: %d != %d" %
                                     (path, path.stat().st_size, expected_size))
        print("MMAP %s: exact file sizes passed" % ("per-process" if per_process else "shared"))
