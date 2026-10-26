#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# wasm-shards-fetch.sh - fetch the shards depot and a native host shardchk,
# then regenerate <dist>/shards.cfg via wasm-shards.sh.
#
# Self-contained prerequisite provider for machines without a local shards
# checkout (CI, fresh clones):
#   1. depot: clone https://github.com/crossuo/shards, or git pull --ff-only
#      when the checkout already exists. Location: $XUO_SHARDS_DEPOT
#      (default ../shards - the same path wasm-shards.sh uses).
#   2. shardchk: build a native host binary into build/shardchk-host unless
#      a fresh one is already there or XUO_SHARDCHK names a trusted override.
#   3. generation + validation stay in wasm-shards.sh (single source of
#      truth); this script only guarantees its inputs exist.
#
# Usage: ./scripts/wasm-shards-fetch.sh [dist dir]     (default: dist/wasm)
#
# Exit status mirrors wasm-shards.sh: 0 on graceful skips (no usable tool,
# empty depot), nonzero on hard failures (clone error, generation error) so
# callers can decide whether to treat shards.cfg as mandatory.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

dist="${1:-dist/wasm}"
depot="${XUO_SHARDS_DEPOT:-$ROOT/../shards}"
shards_repo="https://github.com/crossuo/shards"
build_dir="build/shardchk-host"

# ---------------------------------------------------------------------------
# 1. depot
# ---------------------------------------------------------------------------
has_depot_shards() {
  # legacy layout: per-shard cfg files in <depot>/shards (validated and
  # aggregated by wasm-shards.sh); current upstream layout: a single
  # aggregated shards.cfg at the depot root (served as-is)
  [[ -d "$depot/shards" || -f "$depot/shards.cfg" ]]
}
if has_depot_shards; then
  if git -C "$depot" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    if ! GIT_TERMINAL_PROMPT=0 git -C "$depot" pull --ff-only --quiet; then
      echo ">> [shards] warning: depot update failed - using existing $depot" >&2
    fi
  fi
else
  echo ">> [shards] cloning $shards_repo into $depot"
  if ! GIT_TERMINAL_PROMPT=0 git clone --depth 1 "$shards_repo" "$depot"; then
    echo ">> [shards] error: could not clone $shards_repo into $depot" >&2
    exit 1
  fi
fi
if ! has_depot_shards; then
  echo ">> [shards] error: no $depot/shards or $depot/shards.cfg after fetch" >&2
  exit 1
fi
export XUO_SHARDS_DEPOT="$depot"

# ---------------------------------------------------------------------------
# 2. native host shardchk
# ---------------------------------------------------------------------------
# wasm-shards.sh accepts any build/<dir>/tools/xuoi/shardchk that is runnable
# and newer than the shard schema sources; a trusted XUO_SHARDCHK override
# skips this whole section.
# the aggregated depot layout needs no shardchk at all: wasm-shards.sh
# serves the file as-is, so skip the whole native host build
override="${XUO_SHARDCHK:-}"
if [[ -f "$depot/shards.cfg" && ! -d "$depot/shards" ]]; then
  :
elif [[ -z "$override" || ! -x "$override" ]]; then
  host_bin="$ROOT/$build_dir/tools/xuoi/shardchk"
  srcs=("$ROOT/tools/xuoi/shards.cpp" "$ROOT/tools/xuoi/cfg_shard.h")
  fresh=1
  for src in "${srcs[@]}"; do
    if [[ ! -e "$src" || ! "$host_bin" -nt "$src" ]]; then
      fresh=0
      break
    fi
  done
  if [[ ! -x "$host_bin" || $fresh -eq 0 ]]; then
    echo ">> [shards] building native host shardchk in $build_dir"
    jobs="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
    if ! cmake -B "$build_dir" -S . -DCMAKE_BUILD_TYPE=Release >/dev/null; then
      echo ">> [shards] error: cmake configure failed for $build_dir" >&2
      exit 1
    fi
    if ! cmake --build "$build_dir" --target shardchk --parallel "$jobs"; then
      echo ">> [shards] error: shardchk build failed" >&2
      exit 1
    fi
  fi
fi

# ---------------------------------------------------------------------------
# 3. generate
# ---------------------------------------------------------------------------
exec ./scripts/wasm-shards.sh "$dist"
