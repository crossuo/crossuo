#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# deploy.sh - build and package the release binaries into dist/.
#
# Wraps scripts/cross-build.sh for the deployment targets, then verifies that
# every target produced its binary and its package archive.
#
# Usage: ./scripts/deploy.sh [--cross-build.sh flags] [target...|all]
#
# Default targets (no args): linux-x64 linux-arm64 windows-x64 macos-x64
# `all`: every cross-build target, one after another (see
#        ./scripts/cross-build.sh --list)
# Flags are forwarded to cross-build.sh (--jobs N, --debug, --no-package, ...).
#
# Examples:
#   ./scripts/deploy.sh                     # the four release targets
#   ./scripts/deploy.sh all                 # every target, sequentially
#   ./scripts/deploy.sh windows-x64         # single target
#   ./scripts/deploy.sh --debug linux-x64   # forwarded flag
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

DEFAULT_TARGETS=(linux-x64 linux-arm64 windows-x64 macos-x64)

FLAGS=()
TARGETS=()
for arg in "$@"; do
  case "$arg" in
    --*) FLAGS+=("$arg");;
    all) TARGETS+=( $(./scripts/cross-build.sh --list) );;
    *) TARGETS+=("$arg");;
  esac
done
if [[ ${#TARGETS[@]} -eq 0 ]]; then
  TARGETS=("${DEFAULT_TARGETS[@]}")
fi

expected_binary() {
  case "$1" in
    linux-*) echo "crossuo";;
    windows-*) echo "crossuo.exe";;
    macos-*) echo "CrossUO.app/Contents/MacOS/crossuo";;
    wasm) echo "crossuo.wasm";;
  esac
}

# Regenerate dist/wasm/shards.cfg from the shards depot via the shared
# scripts/wasm-shards.sh (same logic used by cross-build.sh packaging).
# Non-fatal: the wasm deploy must not fail on this.
regenerate_wasm_shards() {
  ./scripts/wasm-shards.sh "dist/wasm" || true
}

# Build xuoproxy in release (native host binary) and ship it inside dist/wasm
# so the deployed folder is directly servable:
#   dist/wasm/xuoproxy --listen 127.0.0.1:8080 --dist dist/wasm
build_xuoproxy() {
  if ! command -v cargo >/dev/null 2>&1; then
    echo ">> deploy: cargo not found - cannot build xuoproxy" >&2
    return 1
  fi
  mkdir -p dist/wasm
  (cd tools/xuoproxy && cargo build --release) || return 1
  cp tools/xuoproxy/target/release/xuoproxy dist/wasm/
}

failed_builds=()
proxy_failed=0
for t in "${TARGETS[@]}"; do
  echo ""
  echo "=================================================================="
  echo "== deploy: $t"
  echo "=================================================================="
  if [[ ${#FLAGS[@]} -gt 0 ]]; then
    ./scripts/cross-build.sh "${FLAGS[@]}" "$t" || { failed_builds+=("$t"); continue; }
  else
    ./scripts/cross-build.sh "$t" || { failed_builds+=("$t"); continue; }
  fi
  if [[ "$t" == "wasm" ]]; then
    regenerate_wasm_shards
    build_xuoproxy || proxy_failed=1
  fi
done

echo ""
echo "================= verification ================="
printf "%-18s %-12s %s\n" "TARGET" "BINARY" "PACKAGE"
rc=0
for t in "${TARGETS[@]}"; do
  if [[ " ${failed_builds[*]:-} " == *" $t "* ]]; then
    printf "%-18s %-12s %s\n" "$t" "BUILD FAILED" "-"
    rc=1
    continue
  fi
  bin="dist/$t/$(expected_binary "$t")"
  if [[ -f "$bin" ]]; then
    b="ok ($(du -h "$bin" | cut -f1))"
  else
    b="MISSING"
    rc=1
  fi
  pkg="$(ls dist/crossuo-"$t"-* 2>/dev/null | head -n1)"
  if [[ -n "$pkg" ]]; then
    p="ok ($(basename "$pkg"))"
  else
    p="MISSING"
    rc=1
  fi
  printf "%-18s %-12s %s\n" "$t" "$b" "$p"
  if [[ "$t" == "wasm" ]]; then
    if [[ -f dist/wasm/xuoproxy ]]; then
      printf "%-18s %-12s %s\n" "" "proxy: ok ($(du -h dist/wasm/xuoproxy | cut -f1))" ""
    else
      printf "%-18s %-12s %s\n" "" "proxy: MISSING" ""
      rc=1
    fi
  fi
done
echo "================================================"

if [[ $rc -eq 0 ]]; then
  echo "deploy: all targets built and packaged."
else
  echo "deploy: FAILED targets above - see logs." >&2
fi
exit $rc
