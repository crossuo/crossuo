#!/usr/bin/env python3
"""Development server for the wasm build.

Serves dist/wasm with the Cross-Origin headers the pthread runtime requires
(SharedArrayBuffer), and reverse-proxies the streaming product hosts under
/patch/ so the browser can fetch them same-origin (the official patch servers
send no CORS headers):
    /patch/manifest/<path> -> manifest repo   (broadsword.com)
    /patch/files/<path>    -> file repo       (ak-cdn.eamythic.com)
    /patch/eamythic/<path> -> aops root       (patch.uo.eamythic.com)
Text responses (product/pkg manifests) have the absolute repo urls rewritten
to the local prefixes, so every follow-up fetch stays same-origin too.

Usage: python3 scripts/serve-wasm.py [dir] [port]
Default: serve dist/wasm on http://localhost:8777
"""
import http.server
import os
import sys
import urllib.error
import urllib.request

DIRECTORY = sys.argv[1] if len(sys.argv) > 1 else "dist/wasm"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8777

# same-origin proxy routes: local prefix -> upstream base url
PROXY_ROUTES = [
    ("/patch/manifest/", "http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest/"),
    ("/patch/files/", "http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files/"),
    ("/patch/eamythic/", "http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/"),
]

# upstream absolute urls rewritten to the local proxies inside text manifests
TEXT_REWRITES = [
    ("http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest/", "/patch/manifest/"),
    ("http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files/", "/patch/files/"),
    ("http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/", "/patch/eamythic/"),
]


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def do_GET(self):
        for prefix, upstream in PROXY_ROUTES:
            if self.path.startswith(prefix):
                self.proxy_get(upstream, self.path[len(prefix):])
                return
        super().do_GET()

    def proxy_get(self, upstream_base, tail):
        url = upstream_base + tail
        headers = {}
        range_header = self.headers.get("Range")
        if range_header:
            headers["Range"] = range_header
        try:
            req = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(req, timeout=60) as resp:
                self.relay(resp)
        except urllib.error.HTTPError as e:
            # non-2xx upstream answers (404, 206, ...) still carry a body
            self.relay(e)
        except Exception as e:
            self.send_error(502, f"proxy fetch failed: {e} ({url})")

    def relay(self, resp):
        status = getattr(resp, "status", None) or resp.code
        body = resp.read()
        ctype = resp.headers.get("Content-Type", "application/octet-stream")
        try:
            # rewrite only real text (manifests): binary payloads (zlib blobs)
            # must pass through untouched
            text = body.decode("utf-8")
            for upstream, local in TEXT_REWRITES:
                text = text.replace(upstream, local)
            body = text.encode("utf-8")
        except UnicodeDecodeError:
            pass
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for header in ("Content-Range", "Accept-Ranges", "ETag", "Last-Modified"):
            value = resp.headers.get(header)
            if value:
                self.send_header(header, value)
        self.end_headers()
        self.wfile.write(body)


if __name__ == "__main__":
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    print(f"serving {DIRECTORY} on http://localhost:{PORT} (COOP/COEP enabled)")
    print("proxying: " + ", ".join(f"{p}* -> {u}" for p, u in PROXY_ROUTES))
    http.server.ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
