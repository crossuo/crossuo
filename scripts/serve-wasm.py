#!/usr/bin/env python3
"""Static file server with the Cross-Origin headers the wasm build needs.

The pthread-based wasm build requires SharedArrayBuffer, which browsers only
expose when the page is served with:
    Cross-Origin-Opener-Policy: same-origin
    Cross-Origin-Embedder-Policy: require-corp

Usage: python3 scripts/serve-wasm.py [dir] [port]
Default: serve dist/wasm on http://localhost:8080
"""
import http.server
import os
import sys

DIRECTORY = sys.argv[1] if len(sys.argv) > 1 else "dist/wasm"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8080


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()


if __name__ == "__main__":
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    print(f"serving {DIRECTORY} on http://localhost:{PORT} (COOP/COEP enabled)")
    http.server.ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
