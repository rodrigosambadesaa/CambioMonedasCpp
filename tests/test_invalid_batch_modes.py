#!/usr/bin/env python3
"""Property/regression tests for unknown modes on batch and stream interfaces."""
import csv
import io
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
BIN = (ROOT / "progvoraz").resolve()
INVALID = ("x", "aa", "Z", "1", "ab")
VALID = ("a", "B", "C", "")
MODES = INVALID + VALID
PAYLOAD = ("mode,currency,amount,range\n" +
           "".join(f"{mode},euro,3,\n" for mode in MODES))
with tempfile.TemporaryDirectory(prefix="invalid-batch-mode-") as tmp:
    work = Path(tmp)
    for name in ("monedas.txt", "stock.txt", "rates_stub.json"):
        if (ROOT / name).exists():
            shutil.copy2(ROOT / name, work / name)
    for kind in ("stream", "batch"):
        if kind == "stream":
            command = [str(BIN), "--stream"]
            input_text = PAYLOAD
        else:
            (work / "input.csv").write_text(PAYLOAD)
            command = [str(BIN), "--input", "input.csv", "--output", "out.csv"]
            input_text = None
        result = subprocess.run(command, cwd=work, input=input_text,
                                text=True, capture_output=True, timeout=50)
        assert result.returncode == 0, (kind, result.stderr[-1600:])
        data = result.stdout if kind == "stream" else (work / "out.csv").read_text()
        rows = list(csv.reader(io.StringIO(data)))
        assert len(rows) == len(MODES)+1, (kind, len(rows), rows)
        for index, mode in enumerate(MODES):
            row = rows[index+1]
            assert len(row) == 5, (kind, mode, row)
            if mode in INVALID:
                assert row[3] == "ERROR" and "modo" in row[4].lower(), (
                    kind, mode, row)
            else:
                assert row[3].startswith("OK total=") or row[3] == "NO", (
                    kind, mode, row)
        print(f"PASS {kind}: invalid modes rejected and valid modes retained")
