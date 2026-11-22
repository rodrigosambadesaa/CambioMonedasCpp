#!/usr/bin/env python3
"""A stalled client must not block the sequential HTTP listener forever."""
import http.client
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time

executable=Path(sys.argv[1]).resolve(strict=True)
root=Path.cwd()
with tempfile.TemporaryDirectory(prefix="progvoraz-http-deadline-") as folder:
    work=Path(folder)
    for name in ("monedas.txt","stock.txt","rates_stub.json"):
        shutil.copy2(root/name,work/name)
    with socket.socket() as sock:
        sock.bind(("127.0.0.1",0))
        port=sock.getsockname()[1]
    env=os.environ.copy()
    env["PROGVORAZ_HTTP_MAX_REQUESTS"]="5"
    env["PROGVORAZ_HTTP_IO_TIMEOUT_MS"]="750"
    env["ASAN_OPTIONS"]="detect_leaks=1:halt_on_error=1:abort_on_error=1"
    process=subprocess.Popen([str(executable),"--server","--port",str(port)],
        cwd=work,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    def health():
        connection=http.client.HTTPConnection("127.0.0.1",port,timeout=4)
        try:
            connection.request("GET","/health")
            response=connection.getresponse()
            return response.status,response.read()
        finally:
            connection.close()
    try:
        for _ in range(100):
            try:
                if health()==(200,b"OK"):break
            except OSError:
                if process.poll() is not None:
                    raise AssertionError("server exited before ready")
                time.sleep(.05)
        else: raise AssertionError("server did not start")
        with socket.create_connection(("127.0.0.1",port),timeout=4) as slow:
            slow.sendall(b"GET /health HTTP/1.1\r\nHost: localhost\r\n")
            start=time.monotonic()
            assert health()==(200,b"OK"),"listener stuck on incomplete headers"
            assert time.monotonic()-start<3.5,"header timeout exceeded deadline"
        with socket.create_connection(("127.0.0.1",port),timeout=4) as slow:
            slow.sendall(b"POST /batch HTTP/1.1\r\nHost: localhost\r\n"
                         b"Content-Length: 600\r\n\r\nshort")
            start=time.monotonic()
            assert health()==(200,b"OK"),"listener stuck on incomplete POST"
            assert time.monotonic()-start<3.5,"body timeout exceeded deadline"
        output,_=process.communicate(timeout=12)
        assert process.returncode==0,(process.returncode,output.decode(errors="replace")[-2500:])
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate(timeout=5)
print("PASS: stalled HTTP headers/body time out and subsequent health requests succeed")
