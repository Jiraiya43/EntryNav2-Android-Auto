// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// OMAP dma-buf pool (see omap_dmabuf.hpp).
#include "omap_dmabuf.hpp"

#include <unistd.h>

#include <cstring>

#include "evo_log.hpp"

namespace evo {

DmaBufPool::DmaBufPool() : drmFd_(-1), dev_(nullptr) {}
DmaBufPool::~DmaBufPool() {
    destroy();
}

bool DmaBufPool::init(unsigned count, uint32_t bufSize) {
    drmFd_ = drmOpen("omapdrm", nullptr);
    if (drmFd_ < 0) {
        ELOG("Video", "drmOpen(omapdrm) failed");
        return false;
    }
    dev_ = omap_device_new(drmFd_);
    if (!dev_) {
        ELOG("Video", "omap_device_new failed");
        drmClose(drmFd_);
        drmFd_ = -1;
        return false;
    }
    for (unsigned i = 0; i < count; ++i) {
        Buf b;
        b.bo = omap_bo_new(dev_, bufSize, kOmapBoWriteCombine);
        if (!b.bo) {
            ELOG("Video", "omap_bo_new(%u) failed at buffer %u", bufSize, i);
            destroy();
            return false;
        }
        b.fd = omap_bo_dmabuf(b.bo);
        b.map = static_cast<uint8_t*>(omap_bo_map(b.bo));
        b.size = bufSize;
        b.inFlight = false;
        if (b.fd < 0 || !b.map) {
            ELOG("Video", "dmabuf/map unavailable at buffer %u", i);
            if (b.fd >= 0) {
                ::close(b.fd);
            }
            omap_bo_del(b.bo);
            destroy();
            return false;
        }
        bufs_.push_back(b);
    }
    ELOG("Video", "dma-buf pool: %u buffers of %u B (omapdrm fd %d)", count, bufSize, drmFd_);
    return true;
}

void DmaBufPool::destroy() {
    for (size_t i = 0; i < bufs_.size(); ++i) {
        if (bufs_[i].fd >= 0) {
            ::close(bufs_[i].fd);
        }
        if (bufs_[i].bo) {
            omap_bo_del(bufs_[i].bo);
        }
    }
    bufs_.clear();
    if (dev_) {
        omap_device_del(dev_);
        dev_ = nullptr;
    }
    if (drmFd_ >= 0) {
        drmClose(drmFd_);
        drmFd_ = -1;
    }
}

int DmaBufPool::acquireFree() {
    for (size_t i = 0; i < bufs_.size(); ++i) {
        if (!bufs_[i].inFlight) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int DmaBufPool::find(int fd) {
    for (size_t i = 0; i < bufs_.size(); ++i) {
        if (bufs_[i].fd == fd) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace evo
