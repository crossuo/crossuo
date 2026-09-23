#!/usr/bin/env python3
"""Headless browser smoke test: load the wasm client, capture console logs and
a screenshot. Usage: python3 scripts/webtest.py [url] [seconds]"""
import base64
import json
import sys
import time
import urllib.request

URL = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8123/"
WAIT = int(sys.argv[2]) if len(sys.argv) > 2 else 15
DRIVER = "http://127.0.0.1:9515"


def req(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    r = urllib.request.Request(
        DRIVER + path, data=data, method=method,
        headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(r) as resp:
        payload = json.loads(resp.read())
    if payload.get("status") not in (0, None):
        raise RuntimeError(payload)
    return payload.get("value")


sub = urllib.request.urlopen(urllib.request.Request(
    DRIVER + "/status")).read()
session = req("POST", "/session", {
    "capabilities": {
        "alwaysMatch": {
            "goog:chromeOptions": {
                "args": [
                    "--headless=new", "--no-sandbox", "--use-angle=swiftshader",
                    "--enable-unsafe-swiftshader", "--window-size=960,620",
                ],
                "prefs": {},
            },
            "goog:loggingPrefs": {"browser": "ALL"},
        }
    }
})
if isinstance(session, dict):
    session = session.get("sessionId")
try:
    req("POST", f"/session/{session}/url", {"url": URL})
    time.sleep(3)
    try:
        early = req("POST", f"/session/{session}/execute/sync",
                    {"script": "return document.getElementById('log').textContent;",
                     "args": []})
        print("=== EARLY PAGE LOG (3s) length:", len(early), "===")
        with open("/tmp/early-log.txt", "w") as f:
            f.write(early)
        print("written to /tmp/early-log.txt")
    except Exception as e:
        print("early read failed:", e)
    time.sleep(max(0, WAIT - 3))
    logs = None
    for path, method in (("/se/log", "POST"), ("/log", "POST"), ("/log", "GET")):
        try:
            logs = req(method, f"/session/{session}{path}", {"type": "browser"})
            break
        except Exception:
            continue
    for entry in logs or []:
        text = entry.get("message", "")
        if "TEXT:" in text:
            text = text.split("TEXT:", 1)[1]
        print(f"[{entry.get('level','?'):>7}] {text.strip()[:2000]}")
    try:
        page_log = req("POST", f"/session/{session}/execute/sync",
                       {"script": "return document.getElementById('log').textContent;",
                        "args": []})
        print("=== client log (page) ===")
        print(page_log[-4000:])
    except Exception as e:
        print("page log unavailable:", e)
    shot = req("GET", f"/session/{session}/screenshot")
    with open("/tmp/wasm-shot.png", "wb") as f:
        f.write(base64.b64decode(shot))
    print("screenshot: /tmp/wasm-shot.png")
finally:
    try:
        req("DELETE", f"/session/{session}")
    except Exception:
        pass
