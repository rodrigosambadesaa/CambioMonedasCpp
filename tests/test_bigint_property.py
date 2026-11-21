#!/usr/bin/env python3
"""Reproducible property tests: the Python integer model is the external oracle."""
import argparse
import os
import random
import subprocess
import sys

sys.set_int_max_str_digits(0)
parser = argparse.ArgumentParser()
parser.add_argument("binary", help="path to compiled tests/bigint_oracle_cli.c")
parser.add_argument("--quick", action="store_true", help="smaller cross-platform CI sample")
parser.add_argument("--valgrind", action="store_true", help="run every case under Valgrind")
args = parser.parse_args()

rng = random.Random(0xC2023)
def decimal(digits):
    if digits == 1 and rng.randrange(7) == 0:
        return "0"
    return str(rng.randrange(1, 10)) + "".join(str(rng.randrange(10)) for _ in range(digits - 1))

boundaries = ["0", "1", "2", "9", "10", "99", "100", "99999999",
              str(2**31-1), str(2**32), str(2**63), str(2**64-1),
              "9"*40, "1" + "0"*40]
pairs = [(a, b) for a in boundaries for b in ("0", "1", "2", "9", "100")]
count = 9 if args.valgrind else (35 if args.quick else 100)
sizes = [1, 2, 8, 19, 38, 80, 160, 512, 1024]
for _ in range(count):
    pairs.append((decimal(rng.choice(sizes)), decimal(rng.choice(sizes))))
if not args.valgrind and not args.quick:
    # Much larger than uint64_t, including a 10,000-digit carry propagation.
    pairs += [("9"*10000, "1"), ("1" + "0"*4095, "9"*200),
              (decimal(1800), decimal(1700)), ("0"*80+"1234", "00012")]
if args.valgrind:
    pairs = pairs[::10] + [("0", "123456789"), ("99999999999", "0")]

base = [args.binary]
if args.valgrind:
    base = ["valgrind", "--quiet", "--leak-check=full",
            "--show-leak-kinds=all", "--errors-for-leak-kinds=definite,possible",
            "--track-origins=yes", "--error-exitcode=99", args.binary]

tested = 0
for a_text, b_text in pairs:
    a, b = int(a_text), int(b_text)
    candidates = [("add", str(a + b)), ("mul", str(a * b)), ("init-reuse", str(a))]
    if a >= b:
        candidates.append(("sub", str(a - b)))
    if b:
        q, r = divmod(a, b)
        candidates.append(("div", f"{q} {r}"))
    # A pre-populated output exercises replacement and free() ownership.
    if a == 0 or b == 0:
        candidates.append(("mul-reuse", "0"))
    for operation, expected in candidates:
        completed = subprocess.run([*base, operation, a_text, b_text],
                                   text=True, capture_output=True, timeout=90)
        actual = completed.stdout.strip()
        if completed.returncode != 0 or actual != expected:
            raise AssertionError(
                f"{operation} with {len(a_text)}/{len(b_text)} digits: "
                f"exit={completed.returncode}, expected={expected[:100]!r}, "
                f"got={actual[:100]!r}; stderr={completed.stderr[-2000:]}"
            )
        tested += 1

# Division by zero, unsupported negative BigInt, malformed string, and
# subtraction below zero must fail safely (not crash or print a result).
for operation, a, b in (("div", "123", "0"), ("add", "-1", "2"),
                        ("mul", "5x", "3"), ("sub", "1", "2")):
    completed = subprocess.run([*base, operation, a, b],
                               text=True, capture_output=True, timeout=90)
    if completed.returncode == 0 or completed.returncode < 0 or completed.returncode == 99:
        raise AssertionError(f"Unsafe rejection: {operation} {a} {b}: "
                             f"exit={completed.returncode}; {completed.stderr[-1000:]}")
    tested += 1

print(f"PASS: {tested} BigInt oracle cases (seed 0xC2023, Valgrind={args.valgrind})")
