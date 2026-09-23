# macOS cross toolchain: osxcross on a dockcross base, so the resulting image
# ships the dockcross wrapper script just like any other dockcross/* image.
#
# Apple does not allow redistributing the macOS SDK, so you must supply one:
#
#   1) package it from your own Xcode (see
#      https://github.com/tpoechtrager/osxcross#packaging-the-sdk), or
#   2) grab a prepackaged SDK, e.g.
#      curl -L https://github.com/joseluisq/macosx-sdks/releases/latest/download/MacOSX15.1.sdk.tar.xz \
#           -o scripts/docker/macos/sdk/MacOSX15.1.sdk.tar.xz
#
# then build once (cached afterwards; cross-build.sh does this automatically):
#
#   docker build -t crossuo/macos scripts/docker/macos
#   docker run --rm crossuo/macos > .dockcross/crossuo_macos && chmod +x .dockcross/crossuo_macos
#
# Build options:
#   --build-arg ENABLE_ARCHS="x86_64 arm64"   target CPUs to build toolchains for
#   --build-arg OSX_VERSION_MIN=10.13         deployment target (11.0 for arm64)
FROM dockcross/base

ENV DEFAULT_DOCKCROSS_IMAGE=crossuo/macos

ARG ENABLE_ARCHS="x86_64 arm64"
ARG OSX_VERSION_MIN=10.13

RUN apt-get update && apt-get install -y --no-install-recommends \
        binutils clang llvm-dev libxml2-dev libssl-dev lzma-dev libbz2-dev \
        zlib1g-dev cpio libmpc-dev libmpfr-dev libgmp-dev patch python3 xz-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/osxcross
RUN git clone https://github.com/tpoechtrager/osxcross.git . \
    && mkdir -p tarballs
COPY sdk/ tarballs/
RUN UNATTENDED=1 \
    OSX_VERSION_MIN=${OSX_VERSION_MIN} \
    ENABLE_ARCHS="${ENABLE_ARCHS}" \
    TARGET_DIR=/usr/local/osxcross \
    ./build.sh -j"$(nproc)" \
    && cd / && rm -rf /opt/osxcross

ENV PATH=/usr/local/osxcross/bin:/usr/local/osxcross/target/bin:$PATH
ENV MACOSX_DEPLOYMENT_TARGET=${OSX_VERSION_MIN}
WORKDIR /work
