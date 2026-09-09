#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 CrossUO Team
#
# vuoi fixture server (VUOI plan §13.2): a tiny, deterministic HTTP server for
# the streaming-install integration tests. Serves a directory tree over HTTP
# with:
#   - HTTP Range GETs (206 + correct Content-Range); --no-range disables it
#     (server reports Accept-Range: none) to exercise the M2 whole-file
#     fallback: pack parts and loose files are whole GETs anyway, so behavior
#     is identical - the mode documents the contract for M3 range streaming
#   - GET /hits -> JSON {"total": N, "counts": {"/path": count}} of every
#     request received per path (cache-miss assertions: "full cache => zero
#     new blob requests")
#   - GET /shutdown -> stops the server (exit 0) when stdin is still open;
#     closing stdin also shuts the server down (process stdin-close)
#   - --cors -> adds Access-Control-Allow-Origin: * (M6 browser backend)
#   - --coop-coep -> adds Cross-Origin-Opener-Policy: same-origin and
#     Cross-Origin-Embedder-Policy: require-corp (M6 browser backend, plan
#     §17.5: the SharedArrayBuffer the fetch worker pool needs requires them)
#
# The server binds an ephemeral port (port 0) and prints the chosen port as
# the first line of stdout ("port N"); --port-file also writes it for
# launchers that poll instead of pipe.

import argparse
import json
import os
import re
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

RANGE_RE = re.compile(r"bytes=(\d*)-(\d*)$")

HITS_LOCK = threading.Lock()
HITS = {}  # path -> count


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "vuoi-fixture/1.0"

    def log_message(self, fmt, *args):  # keep test output deterministic
        if self.server.verbose:
            sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _hits(self, path):
        with HITS_LOCK:
            HITS[path] = HITS.get(path, 0) + 1
            total = sum(HITS.values())
        return total

    def _security_headers(self):
        # cross-origin isolation (plan §17.5): a web build's worker pool needs
        # SharedArrayBuffer, which the browser only grants to COOP/COEP pages
        if self.server.coop_coep:
            self.send_header("Cross-Origin-Opener-Policy", "same-origin")
            self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        if self.server.cors:
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Expose-Headers", "Content-Range")

    def _send(self, code, body, content_type, ranges=None, total_len=None):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        if ranges is not None:
            start, end = ranges
            self.send_header("Content-Range", "bytes %d-%d/%d" % (start, end, total_len))
        self._security_headers()
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        path = self.path.split("?", 1)[0]

        if path == "/shutdown":
            self._send(200, b"bye\n", "text/plain")
            threading.Thread(target=self.server.shutdown, daemon=True).start()
            return

        if path == "/hits":
            with HITS_LOCK:
                payload = json.dumps({"total": sum(HITS.values()), "counts": dict(HITS)})
            self._send(200, payload.encode("utf-8"), "application/json")
            return

        # per-path counter as plain text: "GET /hits?path=/a/b" -> "2"
        if path == "/hit":
            query = self.path.split("?", 1)[1] if "?" in self.path else ""
            wanted = ""
            for part in query.split("&"):
                if part.startswith("path="):
                    wanted = part[5:]
            with HITS_LOCK:
                count = HITS.get(wanted, 0)
            self._send(200, ("%d\n" % count).encode("utf-8"), "text/plain")
            return

        self._hits(path)

        fs_path = os.path.join(self.server.root, *path.split("/"))
        fs_path = os.path.normpath(fs_path)
        root = os.path.normpath(self.server.root)
        # traversal guard: the join above can escape via "..", so require the
        # result to stay inside root (exact match allowed)
        if (fs_path != root and not fs_path.startswith(root + os.sep)) or not os.path.isfile(fs_path):
            body = b"not found\n"
            self._send(404, body, "text/plain")
            return

        with open(fs_path, "rb") as f:
            data = f.read()
        total_len = len(data)

        range_header = self.headers.get("Range")
        match = RANGE_RE.match(range_header) if (range_header and self.server.ranges) else None
        if match:
            start_s, end_s = match.groups()
            start = int(start_s) if start_s else 0
            end = int(end_s) if end_s else total_len - 1
            end = min(end, total_len - 1)
            if start > end or start >= total_len:
                self.send_response(416)
                self.send_header("Content-Range", "bytes */%d" % total_len)
                self.end_headers()
                return
            self._send(206, data[start : end + 1], "application/octet-stream", (start, end), total_len)
            return

        # 200: whole resource. Without range support the server must say so.
        if not self.server.ranges:
            self.send_response(200)
            self.send_header("Accept-Ranges", "none")
        else:
            self.send_response(200)
            self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Type", "application/octet-stream")
        self._security_headers()
        self.send_header("Content-Length", str(total_len))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)


class Server(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, addr, root, ranges, cors, coop_coep, verbose):
        self.root = root
        self.ranges = ranges
        self.cors = cors
        self.coop_coep = coop_coep
        self.verbose = verbose
        super().__init__(addr, Handler)


def main():
    ap = argparse.ArgumentParser(description="vuoi streaming fixture server")
    ap.add_argument("--root", default=".", help="directory tree to serve")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=0, help="0 = ephemeral")
    ap.add_argument("--no-range", action="store_true", help="disable Range support")
    ap.add_argument("--cors", action="store_true", help="send Access-Control-Allow-Origin: *")
    ap.add_argument(
        "--coop-coep",
        action="store_true",
        help="send COOP: same-origin + COEP: require-corp (cross-origin isolation)")
    ap.add_argument("--port-file", default=None, help="write the bound port here")
    ap.add_argument(
        "--no-stdin-watch",
        action="store_true",
        help="do not exit on stdin EOF (use /shutdown to stop)")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    server = Server(
        (args.host, args.port),
        args.root,
        not args.no_range,
        args.cors,
        args.coop_coep,
        args.verbose)
    port = server.server_address[1]
    print("port %d" % port, flush=True)
    if args.port_file:
        with open(args.port_file, "w") as f:
            f.write(str(port))

    # stdin watcher: EOF (launcher closed our stdin) shuts the server down
    if not args.no_stdin_watch:

        def watch_stdin():
            try:
                while sys.stdin.buffer.read(4096):
                    pass
            except Exception:
                pass
            server.shutdown()

        threading.Thread(target=watch_stdin, daemon=True).start()

    try:
        server.serve_forever(poll_interval=0.05)
    except KeyboardInterrupt:
        pass
    server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
