# Optional helper: adds target-architecture X11/OpenGL/ALSA development
# headers to a (Debian based) dockcross linux image, so the vendored SDL2
# builds its X11 video driver and find_package(OpenGL) succeeds.
#
# Not needed for the '-full' dockcross images (they already ship cross GL
# libs); use it to upgrade the plain ones:
#
#   docker build -t crossuo/linux-armv7-deps \
#     --build-arg FROM_IMAGE=dockcross/linux-armv7 \
#     --build-arg DPKG_ARCH=armhf \
#     --build-arg PORTS_ARCH=armhf \
#     scripts/docker/linux-deps
#
#   XUO_IMAGE=crossuo/linux-armv7-deps ./scripts/cross-build.sh linux-armv7
#
ARG FROM_IMAGE=dockcross/linux-arm64
FROM ${FROM_IMAGE}

ARG DPKG_ARCH=arm64
# architecture name on ports.ubuntu.com (arm64, armhf, ...) - leave empty to
# reuse DPKG_ARCH
ARG PORTS_ARCH=
# i386 ships from the main archive, not ports; override for it
ARG PORTS_REPO=http://ports.ubuntu.com/ubuntu-ports
# set to an alpine arch (aarch64, armv7) to fetch musl-built GLES2 libs into
# /opt/alpine-gles instead of the Debian ones (needed for the static-musl
# targets, which cannot link glibc-based shared libraries)
ARG ALPINE_ARCH=
ARG SYSROOT_COPY_DST=
ARG DEB_ARM64_LIBXEXT=

ENV DEBIAN_FRONTEND=noninteractive

RUN if command -v dpkg >/dev/null 2>&1; then \
      arch="${PORTS_ARCH:-${DPKG_ARCH}}" \
      && dpkg --add-architecture "${DPKG_ARCH}" \
      && echo "deb [trusted=yes arch=${arch}] ${PORTS_REPO} jammy main universe" \
           > /etc/apt/sources.list.d/ports.list \
      && apt-get update \
      && apt-get install -y --no-install-recommends \
           libx11-dev:"${DPKG_ARCH}" libxcursor-dev:"${DPKG_ARCH}" \
           libxrandr-dev:"${DPKG_ARCH}" libxinerama-dev:"${DPKG_ARCH}" \
           libxi-dev:"${DPKG_ARCH}" libxext-dev:"${DPKG_ARCH}" \
           libxfixes-dev:"${DPKG_ARCH}" libxrender-dev:"${DPKG_ARCH}" \
           libgl1-mesa-dev:"${DPKG_ARCH}" libglu1-mesa-dev:"${DPKG_ARCH}" \
           libgl-dev:"${DPKG_ARCH}" libglx-dev:"${DPKG_ARCH}" \
           libopengl-dev:"${DPKG_ARCH}" \
           libgles2-mesa-dev:"${DPKG_ARCH}" libegl1-mesa-dev:"${DPKG_ARCH}" \
           libegl-dev:"${DPKG_ARCH}" libgles-dev:"${DPKG_ARCH}" \
           libwayland-dev:"${DPKG_ARCH}" wayland-protocols \
           libasound2-dev:"${DPKG_ARCH}" \
      && rm -rf /var/lib/apt/lists/*; \
    fi

RUN if [ -n "${ALPINE_ARCH}" ]; then \
      mkdir -p /opt/apk /opt/alpine-gles \
      && wget -qO- https://dl-cdn.alpinelinux.org/alpine/v3.20/main/x86_64/ \
           | grep -o 'apk-tools-static-[^"]*\.apk' | head -n1 > /opt/apk/apk.name \
      && wget -qO /opt/apk/apk-tools.apk \
           "https://dl-cdn.alpinelinux.org/alpine/v3.20/main/x86_64/$(cat /opt/apk/apk.name)" \
      && tar -xzf /opt/apk/apk-tools.apk -C /opt/apk \
      && /opt/apk/sbin/apk.static --root /opt/alpine-gles --arch "${ALPINE_ARCH}" \
           --repository https://dl-cdn.alpinelinux.org/alpine/v3.20/main \
           --allow-untrusted --no-cache --initdb add mesa-dev mesa-glapi; \
    fi

# glibc-built aarch64 libXext for the manylinux aarch64 sysroot (Debian
# arm64 .deb, extracted directly - no dpkg needed). Its libGL.so records
# libXext.so.6 as DT_NEEDED but ships no aarch64 copy of it.
RUN if [ -n "${DEB_ARM64_LIBXEXT}" ] && [ -n "${SYSROOT_COPY_DST}" ]; then \
      mkdir -p /opt/deb && cd /opt/deb \
      && wget -qO packages.gz "${DEB_ARM64_LIBXEXT%/}/Packages.gz" \
      && zcat packages.gz | awk '/^Package: libxext6$/{f=1} f && /^Filename:/{print $2; exit}' > f \
      && wget -q "https://deb.debian.org/debian/$(cat f)" \
      && ar x "$(basename $(cat f))" data.tar.xz \
      && mkdir -p "${SYSROOT_COPY_DST}" \
      && rm -f "${SYSROOT_COPY_DST%/lib64}/lib"/libXext.so* \
      && tar -xJf data.tar.xz -C "${SYSROOT_COPY_DST}" --strip-components=3 \
           ./usr/lib/aarch64-linux-gnu/libXext.so.6 ./usr/lib/aarch64-linux-gnu/libXext.so.6.4.0; \
    fi

# SYSROOT_COPY_DST (declared above with ALPINE_ARCH): where the alpine
# fetch's libXext copies land - a toolchain sysroot lib dir; the manylinux
# aarch64 sysroot lacks libXext.so.6, which its libGL.so records as
# DT_NEEDED
