# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Build image matching the EntryNav2 head unit: eglibc 2.15, GCC 4.7, armhf (VFPv3/NEON).
# docker build -t entrynav2-tc:4.7 toolchain   (the name toolchain/tc.sh runs)
# The 12.10 Docker Hub image uses the obsolete schema 1 format. Use the archived Ubuntu Core rootfs instead.
FROM ubuntu:24.04 AS bootstrap
ADD --checksum=sha256:1598091b2dcb731e5994a7f68c2762429e24c7f9c9147695a574f1d0e945f594 \
    https://old-releases.ubuntu.com/releases/ubuntu-core/releases/quantal/release/ubuntu-core-12.10-core-amd64.tar.gz \
    /tmp/quantal.tar.gz
RUN mkdir /quantal && tar -xzf /tmp/quantal.tar.gz -C /quantal

FROM scratch
COPY --from=bootstrap /quantal/ /
RUN echo 'deb http://old-releases.ubuntu.com/ubuntu quantal main universe' > /etc/apt/sources.list \
 && apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --force-yes \
      gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf binutils-arm-linux-gnueabihf \
      make pkg-config file \
 && apt-get clean

# armhf development headers (OpenSSL 1.0.1, D-Bus 1.6, ALSA, libdrm/omap, wayland, glib): packages downloaded and
# unpacked into /opt/hdr-armhf, not installed. The libraries actually linked are the HU's own (/sysroot).
RUN dpkg --add-architecture armhf \
 && echo "deb [arch=armhf] http://old-releases.ubuntu.com/ubuntu quantal main universe" > /etc/apt/sources.list.d/armhf.list \
 && sed -i -e 's#^deb http#deb [arch=amd64,i386] http#' /etc/apt/sources.list \
 && apt-get update \
 && mkdir -p /tmp/debs /opt/hdr-armhf && cd /tmp/debs \
 && apt-get download libssl-dev:armhf libdbus-1-dev:armhf libasound2-dev:armhf libdrm-dev:armhf \
      libwayland-dev:armhf libglib2.0-dev:armhf zlib1g-dev:armhf \
 && for d in *.deb; do dpkg -x "$d" /opt/hdr-armhf; done \
 && rm -rf /tmp/debs \
 && ls /opt/hdr-armhf/usr/include
WORKDIR /work
