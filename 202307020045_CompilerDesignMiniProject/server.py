"""Web playground for the mini compiler.

    python server.py      then open http://localhost:8000

The page (web/index.html) posts the typed code to /compile. This script saves it
to a temp folder, runs minicc.exe there, and sends back the report.html it writes.
The compiling itself is all done by the C program.
"""
import http.server
import os
import pathlib
import subprocess
import sys
import tempfile
import webbrowser

ROOT = pathlib.Path(__file__).resolve().parent
EXE = ROOT / "minicc.exe"
PORT = int(os.environ.get("PORT", 8000))
HOST = "0.0.0.0" if "PORT" in os.environ else "127.0.0.1"  # hosting sites (Render) set PORT
MAX_SOURCE = 100_000  # bytes


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def do_GET(self):
        if self.path == "/":
            self.path = "/web/index.html"
        super().do_GET()

    def do_POST(self):
        if self.path != "/compile":
            return self.send_error(404)
        length = int(self.headers.get("Content-Length", 0))
        if length > MAX_SOURCE:
            return self.send_error(413, "program too large")
        source = self.rfile.read(length)

        with tempfile.TemporaryDirectory() as tmp:
            tmp = pathlib.Path(tmp)
            (tmp / "input.mc").write_bytes(source)
            try:
                subprocess.run([str(EXE), "input.mc"], cwd=tmp, capture_output=True, timeout=5)
            except subprocess.TimeoutExpired:
                return self.send_error(504, "compiler took too long")
            if not (tmp / "report.html").exists():
                return self.send_error(500, "compiler crashed on this program")
            report = (tmp / "report.html").read_bytes()

        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(report)))
        self.end_headers()
        self.wfile.write(report)


if __name__ == "__main__":
    if not EXE.exists():
        sys.exit("minicc.exe not found - run build.bat first")
    server = http.server.ThreadingHTTPServer((HOST, PORT), Handler)
    url = f"http://localhost:{PORT}"
    print(f"Mini compiler playground running at {url}  (Ctrl+C to stop)")
    webbrowser.open(url)
    server.serve_forever()
