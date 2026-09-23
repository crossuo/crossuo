# Portable Linux x86_64 build environment: glibc 2.17 floor (CentOS 7 era),
# so the produced binary runs on virtually every distribution released since
# ~2014 (the "one binary for most distros" approach - the oldest glibc you
# can reasonably build against; the only remaining dynamic deps are glibc
# itself and libGL.so.1).
#
#   docker build -t crossuo/manylinux2014 scripts/docker/manylinux2014
#
# cross-build.sh builds/uses this image automatically for linux-x64.
ARG FROM_IMAGE=dockcross/manylinux2014-x64
FROM ${FROM_IMAGE}

ENV DEFAULT_DOCKCROSS_IMAGE=crossuo/manylinux2014

# CentOS 7 is EOL: point yum at vault.centos.org in case the image still
# references the dead mirrorlist (no-op if already fixed upstream)
RUN sed -i -e 's|^mirrorlist=|#mirrorlist=|g' \
           -e 's|^#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' \
           /etc/yum.repos.d/*.repo || true

RUN yum install -y \
        libX11-devel libXcursor-devel libXrandr-devel libXi-devel \
        libXinerama-devel libXScrnSaver-devel libXxf86vm-devel \
        mesa-libGL-devel alsa-lib-devel zlib-static \
    && yum clean all
