#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# wasm-shards.sh - regenerate <dist dir>/shards.cfg from the shards depot.
#
# Shared by scripts/deploy.sh (post-build step of the wasm target) and by
# scripts/cross-build.sh package_target (so a direct wasm build also ships
# the shard list). Kept standalone so the two entry points cannot drift.
#
# Usage: ./scripts/wasm-shards.sh [dist dir]     (default: dist/wasm)
#
# The depot defaults to $ROOT/../shards (override with XUO_SHARDS_DEPOT).
# A native host shardchk binary validates every <depot>/shards/*.cfg and
# writes an aggregated shards.cfg into the CURRENT working directory, hence
# the scratch dir; the result is copied into the dist dir.
#
# shardchk selection (unless XUO_SHARDCHK is set and executable, which is
# trusted as-is): runnable candidates from build/*/tools/xuoi/shardchk and
# ../build_*/tools/xuoi/shardchk that are NEWER than the shard schema
# sources, newest by mtime. On a missing depot or no usable shardchk the
# script exits 0 without touching the target (graceful skip). On a hard
# generation failure it exits nonzero, leaving any existing target untouched.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

dist="${1:-dist/wasm}"
# Allow callers to pass an absolute path but keep relative paths rooted at ROOT.
[[ "$dist" = /* ]] || dist="$ROOT/$dist"

depot="${XUO_SHARDS_DEPOT:-$ROOT/../shards}"
if [[ ! -d "$depot/shards" ]]; then
  # current upstream depot layout: a single aggregated shards.cfg at the
  # depot root (crossuo/shards dropped the per-shard files) - serve it
  # as-is; no shardchk validation or aggregation is possible or needed
  if [[ -f "$depot/shards.cfg" ]]; then
    mkdir -p "$dist"
    if cp "$depot/shards.cfg" "$dist/shards.cfg"; then
      echo ">> [shards] installed prebuilt $depot/shards.cfg into $dist/"
      exit 0
    fi
    echo ">> [shards] failed to copy $depot/shards.cfg into $dist/" >&2
    exit 1
  fi
  echo ">> [shards] depot not found at $depot/shards - skipping shards.cfg generation"
  exit 0
fi

# A candidate shardchk is only eligible if it runs on this host AND is newer
# than the shard schema sources: an older binary cannot parse the current
# shards.cfg grammar. build/ also holds cross-built foreign-arch/musl
# binaries, hence the runnable check (--help exits without touching files).
srcs=("$ROOT/tools/xuoi/shards.cpp" "$ROOT/tools/xuoi/cfg_shard.h")

shardchk=""
override="${XUO_SHARDCHK:-}"
if [[ -n "$override" && -x "$override" ]]; then
  # An explicit override is trusted as-is, no freshness check.
  shardchk="$override"
else
  # Collect runnable candidates first (avoids re-running foreign binaries
  # during the freshness pass below).
  cand="" runnable=()
  for cand in "$ROOT"/build/*/tools/xuoi/shardchk \
              "$ROOT"/../build_*/tools/xuoi/shardchk; do
    [[ -x "$cand" ]] || continue
    shopt -s nullglob
    if "$cand" --help >/dev/null 2>&1; then
      runnable+=("$cand")
    fi
    shopt -u nullglob
  done

  # Keep only candidates newer than every schema source.
  eligible=()
  for cand in "${runnable[@]}"; do
    fresh=1
    for src in "${srcs[@]}"; do
      if [[ -e "$src" && ! "$cand" -nt "$src" ]]; then
        fresh=0
        break
      fi
    done
    [[ $fresh -eq 1 ]] && eligible+=("$cand")
  done

  if [[ ${#eligible[@]} -eq 0 ]]; then
    if [[ ${#runnable[@]} -gt 0 ]]; then
      echo "" >&2
      echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" >&2
      echo "!! STALE shardchk in build tree(s) - shards.cfg NOT regenerated" >&2
      echo "!! Every runnable candidate predates the shard schema sources:" >&2
      for src in "${srcs[@]}"; do echo "!!   $src" >&2; done
      echo "!! Rebuild it, e.g.: ninja -C <build dir> shardchk" >&2
      for cand in "${runnable[@]}"; do echo "!!   stale: $cand" >&2; done
      echo "!! Existing shards.cfg left untouched." >&2
      echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" >&2
      echo "" >&2
      exit 1
    fi
    echo ">> [shards] no native shardchk found (set XUO_SHARDCHK) - skipping shards.cfg generation"
    exit 0
  fi

  # Newest eligible candidate wins.
  shardchk="$(printf '%s\n' "${eligible[@]}" | while read -r f; do
    printf '%s\t%s\n' "$(stat -c '%Y' "$f")" "$f"
  done | sort -k1,1nr | head -n1 | cut -f2-)"
fi
if [[ -z "$shardchk" ]]; then
  echo ">> [shards] no native shardchk found (set XUO_SHARDCHK) - skipping shards.cfg generation"
  exit 0
fi
echo ">> [shards] using shardchk: $shardchk"

tmp="$(mktemp -d)" || { echo ">> [shards] mktemp failed - skipping shards.cfg generation"; exit 0; }
rc=0
( cd "$tmp" && "$shardchk" -p "$depot/shards" ) || rc=$?
if [[ $rc -ne 0 || ! -f "$tmp/shards.cfg" ]]; then
  echo ">> [shards] shardchk failed (rc=$rc) - leaving existing shards.cfg untouched" >&2
  rm -rf "$tmp"
  exit 1
fi

mkdir -p "$dist"
if cp "$tmp/shards.cfg" "$dist/shards.cfg"; then
  echo ">> [shards] regenerated $dist/shards.cfg from $depot/shards"
else
  echo ">> [shards] failed to copy shards.cfg into $dist/" >&2
  rm -rf "$tmp"
  exit 1
fi
rm -rf "$tmp"
exit 0
