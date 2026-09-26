#!/usr/bin/env python3
"""Development server for the wasm build.

Serves dist/wasm with the Cross-Origin headers the pthread runtime requires
(SharedArrayBuffer), and reverse-proxies the streaming product hosts under
/patch/ so the browser can fetch them same-origin (the official patch servers
send no CORS headers):
    /patch/manifest/<path> -> manifest repo   (broadsword.com)
    /patch/files/<path>    -> file repo       (ak-cdn.eamythic.com)
    /patch/eamythic/<path> -> aops root       (patch.uo.eamythic.com)

This is a NORMALIZING origin for the streaming client:
  * text manifests (.prod/.mft, MythicMFT XML) get their absolute repo urls
    rewritten to the local prefixes, and every compressed transfer
    (ct="1") rewritten to uncompressed (ct="0", cl:=ul) - the blobs are
    decompressed here once, so the client sees a plain range-able source and
    its refine step splits the pack into per-block assets like any other
  * file blobs of rewritten (formerly compressed) transfers are served from
    a decompressed disk cache; Range requests stream straight from it, which
    is what makes per-block punctual fetches work for the ct=1 giants
    (tileart/map/sound/MultiCollection)
  * file responses carry immutable cache headers (the tails are
    content-addressed): the browser HTTP cache is the persistent store
    across sessions - a warm session should show cache hits, not transfers

Usage: python3 scripts/serve-wasm.py [dir] [port]
Default: serve dist/wasm on http://localhost:8777
"""
import http.server
import os
import re
import sys
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET
import zlib

DIRECTORY = sys.argv[1] if len(sys.argv) > 1 else "dist/wasm"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8777
CACHE_DIR = os.environ.get("XUO_PROXY_CACHE", os.path.join(DIRECTORY, ".proxy-cache"))

PROXY_ROUTES = [
    ("/patch/manifest/", "http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest/"),
    ("/patch/files/", "http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files/"),
    ("/patch/eamythic/", "http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/"),
]
TEXT_REWRITES = [
    ("http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest/", "/patch/manifest/"),
    ("http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files/", "/patch/files/"),
    ("http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/", "/patch/eamythic/"),
]
IMMUTABLE = "public, max-age=31536000, immutable"


def looks_zlib(data):
    return len(data) >= 2 and data[0] == 0x78 and ((data[0] << 8) | data[1]) % 31 == 0


class BlobCache:
    """Decompressed artifacts for rewritten (formerly ct=1) transfers."""

    def __init__(self, root):
        self.root = root
        os.makedirs(root, exist_ok=True)

    def path_for(self, name):
        safe = name.replace("..", "_").replace("\\", "_").lstrip("/")
        return os.path.join(self.root, safe)

    def get(self, name, upstream_base):
        dst = self.path_for(name)
        if os.path.exists(dst):
            return dst
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        url = upstream_base + name
        print(f"[proxy] fetching+inflating compressed blob: {name}")
        with urllib.request.urlopen(url, timeout=180) as resp:
            raw = resp.read()
        data = zlib.decompress(raw) if looks_zlib(raw) else raw
        tmp = dst + ".tmp"
        with open(tmp, "wb") as f:
            f.write(data)
        os.replace(tmp, dst)
        print(f"[proxy] decompressed {name}: {len(raw)} -> {len(data)} bytes")
        return dst


CACHE = BlobCache(CACHE_DIR)
# exact blob tails (<rpath>/<pack_rpath>/<ph><sh>) that the manifest rewrite
# normalized to uncompressed; their requests route through the inflater
COMPRESSED_TAILS = set()


def rewrite_manifest(text):
    """Rewrite repo urls + compressed transfers. Returns the new text."""
    for upstream, local in TEXT_REWRITES:
        text = text.replace(upstream, local)
    try:
        root = ET.fromstring(text)
    except ET.ParseError as e:
        print(f"[proxy] manifest parse skipped: {e}")
        return text

    def walk(elem, manifest_rpath, pack_rpath):
        rpath = elem.get("rpath") or manifest_rpath
        is_pack = elem.tag == "p"
        child_pack_rpath = elem.get("rpath") if is_pack else pack_rpath
        for child in elem:
            if child.tag == "f" and child.get("ct") == "1":
                ul = child.get("ul")
                ph, sh = child.get("ph"), child.get("sh")
                if ul:
                    child.set("ct", "0")
                    child.set("cl", ul)
                    if ph and sh:
                        tail = f"{rpath or 'base'}/{pack_rpath or 'unpacked'}/{int(ph, 16):08x}{int(sh, 16):08x}"
                        COMPRESSED_TAILS.add(tail)
                        print(f"[proxy] manifest: ct=1 blob normalized: {tail}")
            walk(child, rpath, child_pack_rpath)

    walk(root, None, None)
    return ET.tostring(root, encoding="unicode")


class Handler(http.server.SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def __init__(self, *args, **kwargs):
        self._xuo_immutable = False
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", IMMUTABLE if self._xuo_immutable else "no-cache")
        super().end_headers()

    def do_GET(self):
        for prefix, upstream in PROXY_ROUTES:
            if self.path.startswith(prefix):
                self.proxy_get(upstream, self.path[len(prefix):])
                return
        # local content-addressed artifacts cache forever; the shell stays fresh
        if re.search(r"\.(wasm|data|bin|uop|mul|idx|rle|iff|mem)$", self.path):
            self._xuo_immutable = True
        super().do_GET()

    def proxy_get(self, upstream_base, tail):
        name = tail.split("?")[0]
        upper = name.upper()
        if upper.endswith((".PROD", ".MFT", ".PKG", ".XML", ".MANIFEST")):
            self.proxy_manifest(upstream_base, tail)
            return
        if name in COMPRESSED_TAILS:
            self.serve_decompressed(upstream_base, name)
            return
        self.proxy_passthrough(upstream_base, name)

    def proxy_manifest(self, upstream_base, tail):
        url = upstream_base + tail
        try:
            with urllib.request.urlopen(url, timeout=60) as resp:
                body = resp.read()
        except urllib.error.HTTPError as e:
            try:
                body = e.read()
            except Exception:
                self.send_error(502, f"manifest fetch failed: {tail}")
                return
        except Exception as e:
            self.send_error(502, f"manifest fetch failed: {e} ({url})")
            return
        try:
            text = body.decode("utf-8")
        except UnicodeDecodeError:
            self.relay_bytes(200, body, "application/octet-stream", {"Cache-Control": "no-cache"})
            return
        data = rewrite_manifest(text).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/xml")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        self.wfile.write(data)

    def proxy_passthrough(self, upstream_base, name):
        url = upstream_base + name
        headers = {}
        range_header = self.headers.get("Range")
        if range_header:
            headers["Range"] = range_header
        try:
            req = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(req, timeout=120) as resp:
                self.stream(resp)
        except urllib.error.HTTPError as e:
            self.stream(e)

    def serve_decompressed(self, upstream_base, name):
        try:
            path = CACHE.get(name, upstream_base)
        except Exception as e:
            self.send_error(502, f"decompress failed: {e} ({name})")
            return
        size = os.path.getsize(path)
        start, end = 0, size - 1
        status = 200
        rng = self.headers.get("Range")
        if rng:
            m = re.match(r"bytes=(\d*)-(\d*)$", rng)
            if m and (m.group(1) or m.group(2)):
                start = int(m.group(1)) if m.group(1) else max(0, size - int(m.group(2)))
                end = min(int(m.group(2)) if m.group(2) else size - 1, size - 1)
                if start > end or start >= size:
                    self.send_response(416)
                    self.send_header("Content-Range", f"bytes */{size}")
                    self.end_headers()
                    return
                status = 206
                self.send_response(206)
                self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        else:
            self.send_response(200)
        length = end - start + 1
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(length))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Cache-Control", IMMUTABLE)
        self.end_headers()
        with open(path, "rb") as f:
            f.seek(start)
            remaining = length
            while remaining > 0:
                chunk = f.read(min(1 << 18, remaining))
                if not chunk:
                    break
                self.wfile.write(chunk)
                remaining -= len(chunk)

    def stream(self, resp):
        status = getattr(resp, "status", None) or resp.code
        self.send_response(status)
        for header in ("Content-Range", "Accept-Ranges", "ETag", "Last-Modified", "Content-Type"):
            value = resp.headers.get(header)
            if value:
                self.send_header(header, value)
        body = resp.read()
        self.send_header("Content-Type", resp.headers.get("Content-Type", "application/octet-stream"))
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", IMMUTABLE)
        self.end_headers()
        self.wfile.write(body)

    def relay_bytes(self, status, body, ctype, headers):
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for k, v in headers.items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)


if __name__ == "__main__":
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    print(f"serving {DIRECTORY} on http://localhost:{PORT} (COOP/COEP enabled)")
    print("proxying: " + ", ".join(f"{p}* -> {u}" for p, u in PROXY_ROUTES))
    print(f"decompress cache: {CACHE_DIR}")
    http.server.ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
