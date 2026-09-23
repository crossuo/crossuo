# x86_64 Linux/musl fully static build environment.
# Alpine IS musl, so this is a native build (no cross toolchain); combined
# with -DCMAKE_EXE_LINKER_FLAGS=-static (applied by cross-build.sh for
# linux-musl-* targets) it produces statically linked binaries.
#
#   docker build -t crossuo/alpine-musl scripts/docker/alpine-musl
#
FROM alpine:3.20

RUN apk add --no-cache \
      bash build-base cmake ninja git pkgconf zip \
      linux-headers \
      mesa-dev mesa-gl \
      libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev \
      libxext-dev libxfixes-dev libxrender-dev libxscrnsaver-dev \
      libxxf86vm-dev libxkbcommon-dev xkeyboard-config \
      wayland-dev wayland-protocols libuuid \
      alsa-lib-dev pulseaudio-dev

# static archives for direct -static linking. X11/ALSA/OpenGL are loaded at
# runtime by SDL (dlopen), so only zlib ends up on the static link line.
RUN apk add --no-cache zlib-static

ENV CC=gcc CXX=g++
