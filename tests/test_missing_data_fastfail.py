#!/usr/bin/env python3
"""An actually missing data file must fail immediately on Unix."""
from pathlib import Path
import subprocess
import sys
import tempfile
import time

binary=Path(sys.argv[1]).resolve(strict=True)
with tempfile.TemporaryDirectory(prefix="cpp-missing-data-") as work:
    start=time.monotonic()
    proc=subprocess.run([str(binary),"--summary","euro"],cwd=work,
        capture_output=True,text=True,timeout=3)
    elapsed=time.monotonic()-start
    assert proc.returncode != 0, "missing currency file was accepted"
    assert elapsed < 2.0, f"missing currency took {elapsed:.2f} seconds"
print("PASS: missing POSIX data files fail fast rather than retrying 1000 times")
