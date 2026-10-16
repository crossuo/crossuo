# Portable Linux aarch64 build environment (glibc floor of the dockcross
# aarch64 cross toolchain).
#
# dockcross/manylinux_2_28-aarch64 is an x86_64 container with an aarch64
# cross toolchain; its sysroot ships only glibc, so this image extracts the
# target-architecture GL/X11/ALSA/zlib development files from AlmaLinux 8
# aarch64 RPMs directly into the cross sysroot (headers + link stubs only -
# libGL/X11 are resolved on the target system at runtime).
#
#   docker build -t crossuo/manylinux_2_28-aarch64 scripts/docker/manylinux_2_28-aarch64
#
FROM dockcross/manylinux_2_28-aarch64

ENV DEFAULT_DOCKCROSS_IMAGE=crossuo/manylinux_2_28-aarch64
ENV SYSROOT=/usr/xcc/aarch64-unknown-linux-gnu/aarch64-unknown-linux-gnu/sysroot

RUN dnf install -y cpio curl grep \
    && mkdir -p /tmp/rpms && cd /tmp/rpms \
    && BASE=https://repo.almalinux.org/almalinux/8 \
    && for repo in AppStream BaseOS PowerTools; do \
         curl -s "$BASE/$repo/aarch64/os/Packages/" -o "/tmp/list-$repo"; \
       done \
    && PKGS="libglvnd libglvnd-opengl libglvnd-glx mesa-libGL libX11 libxcb libXau libXdmcp libXcursor \
             libXext libX11 libxcb \
             libXfixes libXrandr libXi libXinerama libXScrnSaver libXxf86vm \
             alsa-lib zlib \
             libglvnd-devel xorg-x11-proto-devel libX11-devel libXcursor-devel \
             libXrandr-devel libXi-devel libXinerama-devel libXScrnSaver-devel \
             libXxf86vm-devel alsa-lib-devel zlib-devel zlib-static libxcb-devel \
             libXext-devel" \
    && for pkg in $PKGS; do \
         found=""; \
         for repo in AppStream BaseOS PowerTools; do \
           found=$(grep -aoE "href=\"$pkg-[0-9][^\"]*\.rpm\"" "/tmp/list-$repo" 2>/dev/null \
                   | head -n1 | sed 's/href="//;s/"$//'); \
           [ -n "$found" ] && { curl -s "$BASE/$repo/aarch64/os/Packages/$found" -o "$found"; break; }; \
         done; \
         [ -n "$found" ] || echo "WARNING: no rpm found for $pkg"; \
       done \
    && for r in /tmp/rpms/*.rpm; do \
         rpm2cpio "$r" | cpio -idm --quiet -D "$SYSROOT"; \
       done \
    && rm -rf /tmp/rpms /tmp/list-* \
    && for libdir in "$SYSROOT/usr/lib64" "$SYSROOT/usr/lib"; do \
         cd "$libdir"; \
         for so in libX*.so libxcb.so libGL.so; do \
           { [ -e "$so" ] || [ -L "$so" ]; } || continue; \
           base="${so%.so}"; \
           { [ -e "$base.so.6" ] || [ -L "$base.so.6" ]; } || ln -s "$so" "$base.so.6"; \
         done; \
       done \
    && ls "$SYSROOT/usr/include/GL/gl.h" "$SYSROOT/usr/include/X11/Xlib.h" \
          "$SYSROOT/usr/lib64/libGL.so" "$SYSROOT/usr/lib64/libz.a" \
          "$SYSROOT/usr/lib/libXext.so.6"
