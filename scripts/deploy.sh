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
    # the web target has no native binary: the wasm bundle lands in html/
    # (cross-build.sh), and the client payload crossuo.wasm lives there too
    wasm) echo "html/crossuo.wasm";;
  esac
}

# Regenerate dist/wasm/html/shards.cfg from the shards depot via the shared
# scripts/wasm-shards-fetch.sh, which fetches whatever is missing first
# (clones crossuo/shards when there is no local depot, builds a native host
# shardchk when none exists - the CI and fresh-machine path).
# Non-fatal: the wasm deploy must not fail on this.
regenerate_wasm_shards() {
  mkdir -p dist/wasm/html
  ./scripts/wasm-shards-fetch.sh "dist/wasm/html" || true
}

# The wasm zip is produced by cross-build.sh BEFORE deploy.sh builds the
# proxy, so it never contains xuoproxy: refresh it in place (zip update mode
# only adds what is missing) so the archive carries the full deployment
# layout - xuoproxy + html/ (index.html, crossuo.js/wasm, shards.cfg)
refresh_wasm_zip() {
  local pkg zippkg targz
  zippkg="$(ls dist/crossuo-wasm-*.zip 2>/dev/null | head -n1)"
  targz="$(ls dist/crossuo-wasm-*.tar.gz 2>/dev/null | head -n1)"
  pkg="${zippkg:-$targz}"
  if [[ -z "$pkg" ]]; then
    echo ">> deploy: wasm: no crossuo-wasm-*.zip|tar.gz to refresh (package not built?)" >&2
    return 1
  fi
  if [[ ! -x dist/wasm/xuoproxy ]]; then
    echo ">> deploy: wasm: dist/wasm/xuoproxy missing - cannot refresh $pkg" >&2
    return 1
  fi
  echo ">> deploy: wasm: adding xuoproxy to $(basename "$pkg")"
  if [[ -n "$zippkg" ]]; then
    if ! command -v zip >/dev/null; then
      echo ">> deploy: wasm: zip(1) not found - $pkg will NOT contain xuoproxy" >&2
      return 1
    fi
    # note the ../: we cd into dist/wasm, the archive lives in dist/
    if ! (cd dist/wasm && zip -qry "../$(basename "$pkg")" xuoproxy); then
      echo ">> deploy: wasm: zip refresh failed" >&2
      return 1
    fi
  else
    if ! (cd dist/wasm && tar -rf "../$(basename "$pkg")" xuoproxy); then
      echo ">> deploy: wasm: tar refresh failed" >&2
      return 1
    fi
  fi
  # self-check: the archive must now carry the full deployment layout
  if [[ -n "$zippkg" ]] && command -v unzip >/dev/null; then
    for m in xuoproxy html/index.html html/crossuo.wasm html/crossuo.js html/shards.cfg; do
      if ! unzip -l "$pkg" "$m" >/dev/null 2>&1; then
        echo ">> deploy: wasm: $(basename "$pkg") is missing $m after refresh" >&2
        return 1
      fi
    done
  fi
  echo ">> deploy: wasm: package refreshed ($(basename "$pkg"), $(du -h "$pkg" | cut -f1))"
}

# Build xuoproxy in release (native host binary) and ship it inside dist/wasm
# beside the html/ folder the wasm build produces, so the deployed folder is
# directly servable:
#   dist/wasm/xuoproxy --listen 127.0.0.1:8080 --root html
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
    # the proxy is built after the zip was made: fold it in (and make the
    # archive match the deployed folder, xuoproxy + html/*)
    refresh_wasm_zip || proxy_failed=1
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
    # wasm-exclusive specifics: the deployment folder must be directly
    # servable - the proxy binary plus the full html/ payload
    for f in xuoproxy html/index.html html/crossuo.wasm html/crossuo.js html/shards.cfg; do
      if [[ -f "dist/wasm/$f" ]]; then
        printf "%-18s %-12s %s\n" "" "$f: ok ($(du -h "dist/wasm/$f" | cut -f1))" ""
      else
        printf "%-18s %-12s %s\n" "" "$f: MISSING" ""
        rc=1
      fi
    done
    # and the package must carry the same layout (proxy at the archive
    # root, client payload under html/) - only checkable with unzip
    if [[ -n "$pkg" ]] && command -v unzip >/dev/null; then
      local_missing=0
      for m in xuoproxy html/index.html html/crossuo.wasm html/crossuo.js html/shards.cfg; do
        if ! unzip -l "$pkg" "$m" >/dev/null 2>&1; then
          printf "%-18s %-12s %s\n" "" "zip: missing $m" ""
          local_missing=1
        fi
      done
      if [[ $local_missing -eq 0 ]]; then
        printf "%-18s %-12s %s\n" "" "zip: contents ok" ""
      else
        rc=1
      fi
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
