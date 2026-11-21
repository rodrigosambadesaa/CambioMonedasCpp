#!/usr/bin/env python3
"""Validate bounded, unambiguous, deterministic C++ exchange-rate stubs."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

if len(sys.argv) != 2:
    raise SystemExit("usage: test_rate_stub_validation.py BINARY")
binary = Path(sys.argv[1]).resolve()
root = Path.cwd()
with tempfile.TemporaryDirectory(prefix="cpp-rate-stub-") as tmp:
    work = Path(tmp)
    for f in ("monedas.txt", "stock.txt"):
        shutil.copy2(root / f, work / f)
    env = {**os.environ, "PROGVORAZ_EXCHANGE_SOURCE": "stub"}
    examples = [
        ('{"EUR_USD":1.09, "USD_EUR":0.917}', 0, "Destino=109"),
        ('{"EUR_USD":1.09, "EUR_USD":9.99}', 2, "Tasa duplicada"),
        ('{"EUR_USD":1.09, "EUR_USD":"evil"}', 2, "Tasa duplicada"),
        ('{"EUR_USD":-1}', 2, "Tasa invalida"),
        ('{"EUR_USD":1.' + "0"*20000 + '}', 2, "demasiado grande"),
    ]
    for stub, exit_code, marker in examples:
        (work/"rates_stub.json").write_text(stub, encoding="utf-8")
        outcome = subprocess.run([str(binary), "--convert", "EUR", "USD", "100"],
                                 cwd=work, env=env, capture_output=True,
                                 text=True, timeout=30)
        all_output = outcome.stdout + outcome.stderr
        assert outcome.returncode == exit_code and marker.lower() in all_output.lower(), (
            f"bad stub contract: result={outcome.returncode}, marker={marker}, "
            f"stdout={outcome.stdout[-500:]}, stderr={outcome.stderr[-1000:]}"
        )
        print("PASS rate stub:", marker, flush=True)
print("PASS: strict bounded C++ exchange rates")
