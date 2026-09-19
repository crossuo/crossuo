#!/usr/bin/env python3
"""Render command dump analyzer for CrossUO frame dumps (frameN.log).

Usage:
  renderdump.py stats <dump.log>              print command statistics
  renderdump.py diff <a.log> <b.log> [--strip-markers]  compare two dumps
  renderdump.py passes <dump.log>             print per-framebuffer pass summary

The dump is produced with XUO_DUMP_FRAME=1 (writes frame<N>.log in the
client working directory).
"""
import re
import sys
from collections import Counter

DRAW_CMDS = {
    "DrawQuadCmd",
    "DrawRotatedQuadCmd",
    "DrawCharacterSittingCmd",
    "DrawLandTileCmd",
    "DrawShadowCmd",
    "DrawCircleCmd",
    "DrawUntexturedQuadCmd",
    "DrawLineCmd",
}


def cmd_name(line):
    return line.split(":")[0].split(" ")[0]


def load(path, strip_markers=False):
    cmds = []
    with open(path, errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("===") or line.startswith("Dumping cmd list"):
                continue
            name = cmd_name(line)
            if strip_markers and name in ("PushDebugMarker", "PopDebugMarker"):
                continue
            # normalize addresses/pointers that change between runs
            line = re.sub(r"0x[0-9a-f]+", "0xADDR", line)
            cmds.append(line)
    return cmds


def stats(cmds):
    types = Counter(cmd_name(c) for c in cmds)
    draws = sum(types[d] for d in DRAW_CMDS)
    tex = [int(m.group(1)) for c in cmds if c.startswith("SetTextureCmd:") for m in [re.search(r"texture: (\d+)", c)] if m]
    fbs, prev, switches = [], None, 0
    for c in cmds:
        if c.startswith("SetFrameBufferCmd:"):
            v = re.search(r"frameBuffer: (\d+)", c).group(1)
            if v != prev:
                switches += 1
                fbs.append(v)
            prev = v
    return {
        "total": len(cmds),
        "draws": draws,
        "draw_breakdown": {d: types[d] for d in DRAW_CMDS if types[d]},
        "texBinds": len(tex),
        "texUnique": len(set(tex)),
        "fbSwitches": switches,
        "markers": types["PushDebugMarker"] + types["PopDebugMarker"],
        "setDrawMode": types["SetDrawModeCmd"],
        "stencil": sum(types[k] for k in types if "tencil" in k),
        "blend": sum(types[k] for k in types if "lend" in k),
        "viewParams": types["SetViewParamsCmd"],
        "clears": types["ClearRTCmd"],
        "palettes": types["SetColorPaletteCmd"],
    }


def print_stats(s):
    print(f"  total cmds:    {s['total']}")
    print(f"  draw calls:    {s['draws']}  {s['draw_breakdown']}")
    print(f"  texture binds: {s['texBinds']} (unique {s['texUnique']})")
    print(f"  fb switches:   {s['fbSwitches']}")
    print(f"  markers:       {s['markers']}")
    print(f"  setDrawMode:   {s['setDrawMode']}")
    print(f"  stencil cmds:  {s['stencil']}")
    print(f"  blend cmds:    {s['blend']}")
    print(f"  viewParams:    {s['viewParams']}  clears: {s['clears']}  palettes: {s['palettes']}")


def passes(cmds):
    rows, cur = [], None
    prev_fb = None
    for c in cmds:
        if c.startswith("SetFrameBufferCmd:"):
            if cur:
                rows.append(cur)
            cur = {"fb": re.search(r"frameBuffer: (\d+)", c).group(1), "draws": 0, "cmds": 0}
        elif cur is not None:
            n = cmd_name(c)
            cur["cmds"] += 1
            if n in DRAW_CMDS:
                cur["draws"] += 1
    if cur:
        rows.append(cur)
    print(f"{'FB':>10} {'draws':>6} {'cmds':>6}")
    for r in rows:
        print(f"{r['fb']:>10} {r['draws']:>6} {r['cmds']:>6}")
    single = sum(1 for r in rows if r["draws"] <= 1)
    print(f"passes: {len(rows)} (single-draw: {single})")


def diff(a, b):
    import difflib

    sm = difflib.SequenceMatcher(a=a, b=b, autojunk=False)
    removed, added = Counter(), Counter()
    chunks = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            continue
        for l in a[i1:i2]:
            removed[cmd_name(l)] += 1
        for l in b[j1:j2]:
            added[cmd_name(l)] += 1
        chunks.append((tag, i1, i2, j1, j2))
    print(f"A: {len(a)} cmds, B: {len(b)} cmds")
    if not chunks:
        print("IDENTICAL")
        return
    print(f"changed blocks: {len(chunks)}")
    for k in set(removed) | set(added):
        print(f"  {k}: -{removed.get(k,0)} +{added.get(k,0)}")
    show = 0
    for tag, i1, i2, j1, j2 in chunks:
        print(f"--- @{i1}")
        for l in a[max(0, i1 - 2):min(len(a), i2 + 2)]:
            print(f"  A| {l[:160]}")
        for l in b[max(0, j1 - 2):min(len(b), j2 + 2)]:
            print(f"  B| {l[:160]}")
        show += 1
        if show >= 8:
            print("  ... (truncated)")
            break


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    cmd, path = sys.argv[1], sys.argv[2]
    if cmd == "stats":
        print_stats(stats(load(path)))
    elif cmd == "passes":
        passes(load(path))
    elif cmd == "diff":
        strip = "--strip-markers" in sys.argv
        diff(load(path, strip), load(sys.argv[3], strip))
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
