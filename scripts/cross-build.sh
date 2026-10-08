#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# cross-build.sh - cross compile X:UO for Linux (glibc + musl), Windows and
# macOS from one machine, using dockcross (https://github.com/dockcross/dockcross)
# toolchain containers. Only docker (or podman, set OCI_EXE) is required on the
# host; every toolchain runs inside its image and writes into build/<target>.
#
# Usage: ./scripts/cross-build.sh [options] <target>...
#
# Targets:
#   linux-x64        glibc 2.17 portable    crossuo/manylinux2014  (built locally,
#                          FROM dockcross/manylinux2014-x64; binary targets all
#                          distros ~2014+, only libGL.so.1 stays dynamic)
#   linux-x86        glibc i686              dockcross/linux-x86
#   linux-arm64      glibc aarch64           dockcross/linux-arm64-full
#   linux-armv7      glibc armhf             dockcross/linux-armv7
#   linux-musl-x64   musl x86_64  (static)   crossuo/alpine-musl    (built locally)
#   linux-musl-arm64 musl aarch64 (static)   dockcross/linux-arm64-musl
#   linux-musl-armv7 musl armhf   (static)   dockcross/linux-armv7l-musl
#   windows-x64      MinGW x86_64 (static)   dockcross/windows-static-x64
#   windows-x86      MinGW i686   (static)   dockcross/windows-static-x86
#   windows-arm64    llvm-mingw aarch64      dockcross/windows-arm64
#   macos-x64        macOS x86_64            crossuo/macos          (built locally*)
#   macos-arm64      macOS arm64             crossuo/macos          (built locally*)
#   all              every target above
#
#   *) the macOS image wraps osxcross and needs a macOS SDK tarball dropped
#      into scripts/docker/macos/sdk/ first - see scripts/docker/macos/Dockerfile.
#
# Options:
#   --jobs N        parallel build jobs            (default: nproc)
#   --debug         build Debug instead of Release
#   --no-package    compile only, skip dist/ packaging
#   --clean         remove build/<target> and dist/<target> first
#   --list          list targets and exit
#   -h | --help     this help
#
# Per-invocation overrides (environment):
#   XUO_IMAGE=<img>     replace the docker image for the targets being built
#   XUO_RENDER=GL3|GLES2  force the render backend (default: GL3 desktop, GLES2 arm)
#   XUO_STATIC=0|1      force -static linking on/off (default: on for musl)
#   XUO_CMAKE_EXTRA="..." extra cmake -D arguments appended to the configure line
#
# Examples:
#   ./scripts/cross-build.sh all
#   ./scripts/cross-build.sh linux-x64 windows-x64
#   XUO_RENDER=GLES2 ./scripts/cross-build.sh linux-armv7
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

DOCKER="${OCI_EXE:-docker}"
# dockcross wrappers prefer podman when both are installed; podman requires
# localhost/-qualified names for local images, so pin docker unless overridden
export OCI_EXE="${OCI_EXE:-docker}"
BUILD_TYPE=Release
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
PACKAGE=1
CLEAN=0
TARGETS=()

ALL_TARGETS=(
  linux-x64 linux-x86 linux-arm64 linux-armv7
  linux-musl-x64 linux-musl-arm64 linux-musl-armv7
  windows-x64 windows-x86 windows-arm64
  wasm
  macos-x64 macos-arm64
)

while [[ $# -gt 0 ]]; do
  case "$1" in
    --jobs) JOBS="$2"; shift 2;;
    --debug) BUILD_TYPE=Debug; shift;;
    --no-package) PACKAGE=0; shift;;
    --clean) CLEAN=1; shift;;
    --list) echo "${ALL_TARGETS[*]}"; exit 0;;
    -h|--help) usage; exit 0;;
    all) TARGETS+=("${ALL_TARGETS[@]}"); shift;;
    -*) echo "unknown option: $1" >&2; usage >&2; exit 1;;
    *) TARGETS+=("$1"); shift;;
  esac
done

if [[ ${#TARGETS[@]} -eq 0 ]]; then
  usage; exit 1
fi
for t in "${TARGETS[@]}"; do
  if [[ ! " ${ALL_TARGETS[*]} " == *" $t "* ]]; then
    echo "unknown target: $t (see --list)" >&2; exit 1
  fi
done

command -v "$DOCKER" >/dev/null || { echo "error: $DOCKER not found in PATH" >&2; exit 1; }

# ---------------------------------------------------------------------------
# per-target configuration
# ---------------------------------------------------------------------------
target_image() {
  case "$1" in
    linux-x64)        echo "${XUO_IMAGE:-crossuo/manylinux2014}";;
    linux-x86)        echo "${XUO_IMAGE:-dockcross/linux-x86}";;
    linux-arm64)      echo "${XUO_IMAGE:-crossuo/manylinux_2_28-aarch64}";;
    linux-armv7)      echo "${XUO_IMAGE:-dockcross/linux-armv7}";;
    linux-musl-x64)   echo "${XUO_IMAGE:-crossuo/alpine-musl}";;
    linux-musl-arm64) echo "${XUO_IMAGE:-dockcross/linux-arm64-musl}";;
    linux-musl-armv7) echo "${XUO_IMAGE:-dockcross/linux-armv7l-musl}";;
    windows-x64)      echo "${XUO_IMAGE:-dockcross/windows-static-x64}";;
    windows-x86)      echo "${XUO_IMAGE:-dockcross/windows-static-x86}";;
    windows-arm64)    echo "${XUO_IMAGE:-dockcross/windows-arm64}";;
    wasm)             echo "${XUO_IMAGE:-dockcross/web-wasm}";;
    macos-x64|macos-arm64) echo "${XUO_IMAGE:-crossuo/macos}";;
  esac
}

target_render() {
  if [[ -n "${XUO_RENDER:-}" ]]; then echo "$XUO_RENDER"; return; fi
  case "$1" in
    linux-armv7|linux-musl-arm64|linux-musl-armv7|wasm) echo GLES2;;
    *) echo GL3;;
  esac
}

target_static() {
  if [[ -n "${XUO_STATIC:-}" ]]; then echo "$XUO_STATIC"; return; fi
  case "$1" in
    linux-musl-*) echo 1;;
    windows-*) echo 1;;    # MXE/llvm-mingw static toolchains, -static is the point
    *) echo 0;;
  esac
}

# "dockcross" images expose their toolchain through the bundled dockcross
# wrapper script; "plain" images (alpine) are run directly.
target_runner() {
  case "$1" in
    linux-musl-x64) echo plain;;
    *) echo dockcross;;
  esac
}

target_arch() {
  case "$1" in
    macos-arm64) echo arm64;;
    macos-x64) echo x86_64;;
  esac
}

# ---------------------------------------------------------------------------
# docker plumbing
# ---------------------------------------------------------------------------
_dockcross_wrapper() {
  local image="$1"
  # cache dir must NOT be named `.dockcross` - dockcross executes a file of
  # that name as per-project config when present
  local wrapper=".dockcross-cache/${image//\//_}"
  if [[ ! -x "$wrapper" ]]; then
    mkdir -p .dockcross-cache
    echo ">> bootstrapping dockcross wrapper for $image"
    $DOCKER run --rm ${DOCKCROSS_ARGS:-} "$image" > "$wrapper"
    chmod +x "$wrapper"
  fi
  echo "$wrapper"
}

ensure_local_image() {
  local image="$1" dir="$2"
  if $DOCKER image inspect "$image" >/dev/null 2>&1; then return; fi
  if [[ "$image" == crossuo/macos ]] && ! ls scripts/docker/macos/sdk/*.tar.* >/dev/null 2>&1; then
    cat >&2 <<EOF
error: $image is not built and no macOS SDK found in scripts/docker/macos/sdk/.

Get a SDK tarball (MacOSX*.sdk.tar.xz) - either package one from your own
Xcode per https://github.com/tpoechtrager/osxcross#packaging-the-sdk, or
download a prepackaged one, e.g.:
  curl -L https://github.com/joseluisq/macosx-sdks/releases/latest/download/MacOSX15.1.sdk.tar.xz \\
       -o scripts/docker/macos/sdk/MacOSX15.1.sdk.tar.xz
then rerun this command (the image is built once and cached).
EOF
    exit 1
  fi
  echo ">> building local image $image from $dir"
  $DOCKER build -t "$image" "$dir"
}

run_tool() {
  local target="$1"; shift
  if [[ "$(target_runner "$target")" == dockcross ]]; then
    $(_dockcross_wrapper "$(target_image "$target")") "$@"
  else
    # plain image: run as the calling user so build outputs are not root-owned
    $DOCKER run --rm \
      -e CC=gcc -e CXX=g++ -e XUO_CI=1 \
      -v "$ROOT:/work" -w /work -u "$(id -u):$(id -g)" \
      "$(target_image "$target")" "$@"
  fi
}

# ---------------------------------------------------------------------------
# cmake configuration
# ---------------------------------------------------------------------------
darwin_toolchain_file() {
  local target="$1" arch; arch=$(target_arch "$target")
  local clang sdk
  clang=$(run_tool "$target" bash -c "ls /usr/local/osxcross/bin/${arch}-apple-darwin*-clang /usr/local/osxcross/target/bin/${arch}-apple-darwin*-clang 2>/dev/null | head -n1") \
    || clang=""
  sdk=$(run_tool "$target" bash -c "ls -d /usr/local/osxcross/SDK/MacOSX*.sdk /usr/local/osxcross/target/SDK/MacOSX*.sdk 2>/dev/null | head -n1") \
    || sdk=""
  if [[ -z "$clang" || -z "$sdk" ]]; then
    echo "error: osxcross toolchain for $arch not found in $(target_image "$target")" >&2
    exit 1
  fi
  local cxx="${clang%clang}clang++"
  local min="10.13"; [[ "$arch" == arm64 ]] && min="11.0"
  mkdir -p "build/$target"
  cat > "build/$target/toolchain.cmake" <<EOF
# generated by scripts/cross-build.sh - macOS ($arch) via osxcross
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR $arch)
set(CMAKE_C_COMPILER $clang)
set(CMAKE_CXX_COMPILER $cxx)
set(CMAKE_OSX_SYSROOT $sdk)
set(CMAKE_OSX_DEPLOYMENT_TARGET $min)
set(CMAKE_FIND_ROOT_PATH $sdk)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
  echo "build/$target/toolchain.cmake"
}

cmake_args() {
  local target="$1"
  local args=(-G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DXUO_DEPLOY=On)
  case "$(target_render "$target")" in
    GL3)   args+=(-DRENDER_GL3=On);;
    GLES2) args+=(-DRENDER_GLES2=On);;
  esac
  if [[ "$target" == linux-musl-* ]]; then
    # musl static: static client link; drop the wayland video driver (needs
    # static libwayland/libEGL archives which musl distros do not ship) and
    # force the static zlib
    args+=(-DXUO_STATIC_BINARY=On -DVIDEO_WAYLAND=OFF -DZLIB_LIBRARY=/lib/libz.a)
  elif [[ "$target" == linux-x64 ]]; then
    # portable glibc build: bake in the C++ runtime, no wayland dev stack,
    # static zlib - only libGL.so.1 + core glibc remain dynamic
    args+=(-DXUO_STATIC_CXX_RUNTIME=On -DVIDEO_WAYLAND=OFF -DZLIB_LIBRARY=/usr/lib64/libz.a)
  elif [[ "$target" == linux-arm64 ]]; then
    # same portable recipe; zlib comes from the aarch64 cross sysroot
    args+=(-DXUO_STATIC_CXX_RUNTIME=On -DVIDEO_WAYLAND=OFF -DZLIB_LIBRARY=/usr/xcc/aarch64-unknown-linux-gnu/aarch64-unknown-linux-gnu/sysroot/usr/lib64/libz.a)
  elif [[ "$target" == windows-* ]]; then
    # MinGW cross builds: SDL's DIRECTX backend requires the DirectX SDK
    # ($DXSDK_DIR) which MXE does not ship; winmm/standard input is used
    args+=(-DDIRECTX=OFF -DXUO_STATIC_CXX_RUNTIME=On)
  fi
  if [[ "$(target_runner "$target")" == dockcross ]]; then
    if [[ "$target" == macos-* ]]; then
      args+=(-DCMAKE_TOOLCHAIN_FILE="$(darwin_toolchain_file "$target")")
    else
      # the toolchain file path lives in the image environment
      local tc; tc=$(run_tool "$target" bash -c 'echo "${CMAKE_TOOLCHAIN_FILE:-}"')
      [[ -n "$tc" ]] && args+=(-DCMAKE_TOOLCHAIN_FILE="$tc")
    fi
  fi
  [[ -n "${XUO_CMAKE_EXTRA:-}" ]] && args+=(${XUO_CMAKE_EXTRA})
  echo "${args[@]}"
}

# ---------------------------------------------------------------------------
# build + package
# ---------------------------------------------------------------------------
build_target() {
  local target="$1"
  local image; image=$(target_image "$target")

  case "$image" in
    crossuo/*) ensure_local_image "$image" "scripts/docker/${image#crossuo/}";;
  esac

  if [[ "$CLEAN" == 1 ]]; then rm -rf "build/$target" "dist/$target"; fi

  local args; args=$(cmake_args "$target")
  echo ""
  echo ">> [$target] image=$image render=$(target_render "$target") static=$(target_static "$target")"
  echo ">> [$target] configure: cmake -B build/$target $args"
  run_tool "$target" cmake -B "build/$target" -S . $args \
    || { echo "configure failed for $target - if it complains about missing X11/OpenGL/EGL headers,
         use a '-full' dockcross image or scripts/docker/Dockerfile.linux-deps
         (XUO_IMAGE=<img> ./scripts/cross-build.sh $target)" >&2; exit 1; }
  echo ">> [$target] building ($JOBS jobs)"
  run_tool "$target" cmake --build "build/$target" --parallel "$JOBS"
}

package_target() {
  local target="$1"
  local b="build/$target" out="dist/$target"
  local ver; ver=$(git describe --always --dirty 2>/dev/null || echo dev)
  local name="crossuo-$target-$ver"

  rm -rf "$out"; mkdir -p "$out"

  case "$target" in
    linux-*)
      cp "$b/src/crossuo" "$out/"
      [[ -d "$b/src/assets" ]] && cp -r "$b/src/assets" "$out/assets"
      tar -czf "dist/$name.tar.gz" -C dist "$(basename "$out")"
      echo ">> [$target] packaged dist/$name.tar.gz"
      ;;
    windows-*)
      cp "$b/src/crossuo.exe" "$out/"
      [[ -d "$b/src/assets" ]] && cp -r "$b/src/assets" "$out/assets"
      if command -v zip >/dev/null; then
        (cd dist && zip -qr "$name.zip" "$(basename "$out")")
      else
        tar -czf "dist/$name.tar.gz" -C dist "$(basename "$out")"
      fi
      echo ">> [$target] packaged dist/$name.*"
      ;;
    macos-*)
      mkdir -p "$out/CrossUO.app/Contents/MacOS" "$out/CrossUO.app/Contents/Resources"
      cp "$b/src/crossuo" "$out/CrossUO.app/Contents/MacOS/"
      [[ -d "$b/src/assets" ]] && cp -r "$b/src/assets" "$out/CrossUO.app/Contents/MacOS/assets"
      cp resources/crossuo.icns "$out/CrossUO.app/Contents/Resources/"
      cp resources/Info.plist "$out/CrossUO.app/Contents/"
      if command -v zip >/dev/null; then
        (cd "$out" && zip -qry "../$name.zip" CrossUO.app)
        echo ">> [$target] packaged dist/$name.zip"
      else
        tar -czf "dist/$name.tar.gz" -C dist "$(basename "$out")"
        echo ">> [$target] packaged dist/$name.tar.gz"
      fi
      ;;
    wasm)
      cp "$b"/src/crossuo.js "$b"/src/crossuo.wasm "$out/" 2>/dev/null || true
      [[ -f "$b/src/crossuo.data" ]] && cp "$b/src/crossuo.data" "$out/"
      [[ -f "$b/src/crossuo.worker.js" ]] && cp "$b/src/crossuo.worker.js" "$out/"
      cp resources/shell.html "$out/index.html"
      # Regenerate the shard list so a direct wasm build ships a fresh
      # shards.cfg (the zip below must include it). Non-fatal: a missing
      # depot on a fresh machine must not fail the wasm build.
      if ! ./scripts/wasm-shards.sh "$out"; then
        echo ">> [$target] warning: shards.cfg regeneration failed - continuing"
      fi
      if command -v zip >/dev/null; then
        (cd "$out" && zip -qry "../$name.zip" .)
        echo ">> [$target] packaged dist/$name.zip"
      fi
      echo ">> [$target] serve with: tools/xuoproxy/target/release/xuoproxy --listen 127.0.0.1:8080 --dist dist/$target"
  esac
}

for t in "${TARGETS[@]}"; do
  build_target "$t"
  [[ "$PACKAGE" == 1 ]] && package_target "$t"
done

echo ""
echo "done. artifacts in dist/, build trees in build/<target>"
