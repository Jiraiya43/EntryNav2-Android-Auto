// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// OMAP dma-buf pool that carries the H.264 stream to libxsvideo.

// Functions of the car's /usr/lib/libdrm_omap.so.1 and /usr/lib/libdrm.so.2, which export these symbols. The car
// ships no omap_drmif.h: the signatures are the public libdrm_omap ones.
// Same sequence as /usr/bin/early-video.digital: drmOpen("omapdrm") -> omap_device_new ->
// omap_bo_new(dev, size, OMAP_BO_WC) -> omap_bo_dmabuf (fd) + omap_bo_map (CPU pointer).
#ifndef ENTRY_AAD_PLATFORM_OMAP_DMABUF_HPP
#define ENTRY_AAD_PLATFORM_OMAP_DMABUF_HPP

#include <stddef.h>
#include <stdint.h>

#include <vector>

extern "C" {
// libdrm.so.2
int drmOpen(const char* name, const char* busid);
void drmClose(int fd);
// libdrm_omap.so.1
struct omap_device;
struct omap_bo;
struct omap_device* omap_device_new(int fd);
void omap_device_del(struct omap_device*);
struct omap_bo* omap_bo_new(struct omap_device*, uint32_t size, uint32_t flags);
void omap_bo_del(struct omap_bo*);
int omap_bo_dmabuf(struct omap_bo*);
void* omap_bo_map(struct omap_bo*);
}

// OMAP_BO flags (early-video uses 0x2, write-combine, suited to a buffer the CPU writes).
const uint32_t kOmapBoWriteCombine = 0x00000002U;

namespace evo {

// Pool of dma-bufs, each {omap_bo, fd, CPU pointer}. EvoVideo marks them in flight or free as the
// XSV_EVENT_DMABUF_COMPLETE events come in.
class DmaBufPool {
public:
    struct Buf {
        struct omap_bo* bo;
        int fd;
        uint8_t* map;
        uint32_t size;
        bool inFlight;
    };

    DmaBufPool();
    ~DmaBufPool();

    // Opens omapdrm and allocates `count` buffers of `bufSize` bytes each.
    bool init(unsigned count, uint32_t bufSize);
    void destroy();

    // Index of a free buffer, or -1 when all are in flight.
    int acquireFree();
    Buf& at(int i) { return bufs_[i]; }
    int find(int fd);  // index of the buffer with this fd, or -1

private:
    int drmFd_;
    struct omap_device* dev_;
    std::vector<Buf> bufs_;
};

}  // namespace evo

#endif  // ENTRY_AAD_PLATFORM_OMAP_DMABUF_HPP
