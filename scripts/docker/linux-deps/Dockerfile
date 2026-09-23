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

ENV DEBIAN_FRONTEND=noninteractive

RUN if command -v dpkg >/dev/null 2>&1; then \
      arch="${PORTS_ARCH:-${DPKG_ARCH}}" \
      && dpkg --add-architecture "${DPKG_ARCH}" \
      && echo "deb [arch=${arch}] http://ports.ubuntu.com/ubuntu-ports jammy main universe" \
           > /etc/apt/sources.list.d/ports.list \
      && apt-get update \
      && apt-get install -y --no-install-recommends \
           libx11-dev:"${DPKG_ARCH}" libxcursor-dev:"${DPKG_ARCH}" \
           libxrandr-dev:"${DPKG_ARCH}" libxinerama-dev:"${DPKG_ARCH}" \
           libxi-dev:"${DPKG_ARCH}" libxext-dev:"${DPKG_ARCH}" \
           libxfixes-dev:"${DPKG_ARCH}" libxrender-dev:"${DPKG_ARCH}" \
           libgl1-mesa-dev:"${DPKG_ARCH}" libglu1-mesa-dev:"${DPKG_ARCH}" \
           libasound2-dev:"${DPKG_ARCH}" \
      && rm -rf /var/lib/apt/lists/*; \
    fi
