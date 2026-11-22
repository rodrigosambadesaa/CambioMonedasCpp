#!/usr/bin/env python3
"""Regression: a stalled client must not block the single-thread HTTP server."""
import http.client
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent
EXE = (ROOT / "progvoraz").resolve()

def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]

def health(port, timeout=16):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        start = time.monotonic()
        conn.request("GET", "/health")
        response = conn.getresponse()
        assert (response.status, response.read()) == (200, b"OK")
        return time.monotonic() - start
    finally:
        conn.close()

def stalled_client(port, request):
    sock = socket.create_connection(("127.0.0.1", port), timeout=5)
    sock.sendall(request)
    return sock

with tempfile.TemporaryDirectory(prefix="cpp-http-deadline-") as dirname:
    work = Path(dirname)
    for name in ("monedas.txt", "stock.txt", "rates_stub.json"):
        shutil.copy2(ROOT/name, work/name)
    port = free_port()
    env = os.environ.copy()
    env.pop("PROGVORAZ_HTTP_BIND_ALL", None)
    env["PROGVORAZ_HTTP_MAX_REQUESTS"] = "5"
    p = subprocess.Popen([str(EXE), "--server", "--port", str(port)],
                         cwd=work, env=env, stdout=subprocess.DEVNULL,
                         stderr=subprocess.PIPE, text=True)
    try:
        for attempt in range(100):
            try:
                health(port, 1)
                break
            except (OSError, TimeoutError):
                if p.poll() is not None:
                    raise AssertionError(f"server exited: {p.returncode}")
                time.sleep(.05)
        else:
            raise AssertionError("server never started")

        # Linux 127/8 routes to loopback. A service bound to INADDR_ANY
        # would accept 127.0.0.2, unlike one bound only to 127.0.0.1.
        with socket.socket() as probe:
            probe.settimeout(2)
            assert probe.connect_ex(("127.0.0.2", port)) != 0, \
                "HTTP server is exposed on interfaces beyond 127.0.0.1"

        slow = stalled_client(port,
            b"GET /health HTTP/1.1\r\nHost: localhost\r\nX-Slow: ")
        try:
            time.sleep(.1)
            elapsed = health(port)
            assert elapsed < 13, f"header-stalling peer blocked health for {elapsed:.2f}s"
        finally:
            slow.close()

        slow = stalled_client(port,
            b"POST /batch HTTP/1.1\r\nHost: localhost\r\n"
            b"Content-Length: 1000\r\n\r\npartial")
        try:
            time.sleep(.1)
            elapsed = health(port)
            assert elapsed < 13, f"body-stalling peer blocked health for {elapsed:.2f}s"
        finally:
            slow.close()

        rc = p.wait(timeout=20)
        assert rc == 0, (rc, p.stderr.read())
        print("PASS: loopback-only listener and 8s total HTTP request deadline")
    finally:
        if p.poll() is None:
            p.kill()
            p.wait(timeout=5)
        p.stderr.close()
