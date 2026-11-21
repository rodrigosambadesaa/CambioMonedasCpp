#!/usr/bin/env python3
"""Actual HTTP traffic under Valgrind/ASan: assert resource cleanup at exit."""
import argparse
import http.client
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time

ap = argparse.ArgumentParser()
ap.add_argument("binary")
ap.add_argument("--valgrind", action="store_true")
args = ap.parse_args()
binary = Path(args.binary).resolve()
root = Path.cwd()

def request(port, method, path, body=None):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    try:
        headers = {"Content-Type": "text/csv"} if body is not None else {}
        conn.request(method, path, body=body, headers=headers)
        resp = conn.getresponse()
        return resp.status, resp.read()
    finally:
        conn.close()

def raw(port, value):
    with socket.create_connection(("127.0.0.1", port), timeout=10) as conn:
        conn.settimeout(10)
        conn.sendall(value)
        conn.shutdown(socket.SHUT_WR)
        data = bytearray()
        while chunk := conn.recv(8192):
            data.extend(chunk)
        return bytes(data)

with tempfile.TemporaryDirectory(prefix="cpp-http-memory-") as temp:
    work = Path(temp)
    for name in ("monedas.txt", "stock.txt", "rates_stub.json"):
        shutil.copy2(root / name, work / name)
    sock = socket.socket()
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    env = {**os.environ, "PROGVORAZ_HTTP_MAX_REQUESTS": "20"}
    env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1:abort_on_error=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    cmd = [str(binary), "--server", "--port", str(port)]
    if args.valgrind:
        cmd = ["valgrind", "--quiet", "--leak-check=full",
               "--errors-for-leak-kinds=definite,possible",
               "--error-exitcode=99", *cmd]
    log = work / "server.log"
    with log.open("wb") as output:
        proc = subprocess.Popen(cmd, cwd=work, env=env, stdout=output, stderr=subprocess.STDOUT)
        try:
            for _ in range(200):
                try:
                    status, body = request(port, "GET", "/health")
                    break
                except OSError:
                    if proc.poll() is not None:
                        raise AssertionError(f"early exit: {log.read_text(errors='replace')}")
                    time.sleep(0.05)
            else:
                raise AssertionError("HTTP server not ready")
            assert (status, body) == (200, b"OK")
            assert request(port, "GET", "/export_stock_json")[0] == 200
            assert request(port, "GET", "/unknown")[0] == 404
            csv = b"mode,currency,amount,range\na,euro,250,\n"
            status, data = request(port, "POST", "/batch", csv)
            assert status == 200 and b"OK total=" in data, (status, data[:200])
            invalid = b"mode,currency,amount,range\na,unknown,10,\n"
            status, data = request(port, "POST", "/batch", invalid)
            assert status == 200 and b"ERROR" in data, (status, data[:200])
            assert raw(port, b"POST /batch HTTP/1.1\r\nHost: localhost\r\n"
                       b"Content-Length: 18446744073709551615\r\n\r\n").startswith(b"HTTP/1.1 413")
            assert raw(port, b"POST /batch HTTP/1.1\r\nHost: localhost\r\n"
                       b"Content-Length: garbage\r\n\r\n").startswith(b"HTTP/1.1 400")
            raw(port, b"POST /batch HTTP/1.1\r\nHost: localhost\r\n"
                b"Content-Length: 50\r\n\r\nshort")
            # 8 requests above + 12 to close the finite-lifetime server.
            fd_path = Path(f"/proc/{proc.pid}/fd")
            baseline = len(list(fd_path.iterdir())) if fd_path.is_dir() else None
            for i in range(12):
                assert request(port, "GET", "/health") == (200, b"OK")
                if i == 10 and baseline is not None:
                    count = len(list(fd_path.iterdir()))
                    assert count <= baseline + 2, f"FD growth: {baseline} -> {count}"
                    print(f"PASS fd stability: {baseline} -> {count}", flush=True)
            exit_code = proc.wait(timeout=60)
            text = log.read_text(errors="replace")
            assert exit_code == 0, (exit_code, text[-6000:])
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait(timeout=5)
print(f"PASS: 20 HTTP requests and resource cleanup ({'Valgrind' if args.valgrind else 'ASan/LSan/UBSan'})")
