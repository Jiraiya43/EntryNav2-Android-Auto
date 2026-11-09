// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Android Auto video: H.264 decoded by libxsvideo onto an IVI surface, shown and hidden through the LayerManager.

// The pieces:
//   libxsvideo type 1   H.264 decoder (IVA-HD/DCE) rendering NV12 onto an IVI surface; it connects to Wayland
//                       itself (see xsv_api.h).
//   OMAP dma-bufs       carry the Annex B H.264 stream to the decoder (omap_dmabuf.hpp).
//   LayerManager (ILM)  attaches and hides our surface in a layer that is already visible (evo_ilm.hpp).
//
// The phone renders a 1280x720 canvas; the panel is 1280x480 (omapdrm fb0 1280,480). The surface is cropped to the
// top 480 lines, which hold the phone's UI.
#ifndef ENTRY_AAD_PLATFORM_EVO_VIDEO_HPP
#define ENTRY_AAD_PLATFORM_EVO_VIDEO_HPP

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <vector>

#include "evo_ilm.hpp"
#include "omap_dmabuf.hpp"
#include "xsv_api.h"

namespace evo {

// IVI ids of our surfaces (observed working on the car): Android Auto on the HMI's full-screen layer, the id checked
// free before each start. The volume pill takes the next id, above Android Auto.
const uint32_t kAaSurfaceId = 0x7eaa0001, kPillSurfaceId = 0x7eaa0002, kAaLayerId = 0x80000000;

struct EvoVideoConfig {
    uint32_t width, height;                // phone stream: 1280x720
    uint32_t displayWidth, displayHeight;  // shown part of it: the 1280x480 panel
    uint32_t fps;                          // 0 = not imposed
    uint32_t surfaceId;                    // kAaSurfaceId
    uint32_t layerId;                      // kAaLayerId, already visible
    unsigned bufCount;                     // 8 dma-bufs
    uint32_t bufSize;                      // 1 MiB each
    EvoVideoConfig();                      // this car's values
};

class EvoVideo {
public:
    EvoVideo();
    ~EvoVideo();

    bool init(const EvoVideoConfig& cfg);
    bool start();  // opens and configures libxsvideo, starts the event thread, attaches the hidden surface
    void stop();

    void setVisible(bool visible);
    bool setOpacity(float opacity);  // our surface only; cached (no CLI call when unchanged)
    bool isVisible() const { return visible_; }
    bool isReady() const { return ready_; }  // start() completed: the surface is attached and can be shown
    bool firstFrameSeen();                   // compositor frame callback after H.264; not a visual confirmation

    // SPS/PPS (the Android Auto codec config): kept and prepended to the first IDR frame after each (re)start.
    void postCodecConfig(const uint8_t* data, size_t len);
    // One H.264 access unit, copied into a free dma-buf and handed to the decoder.
    // False when no buffer frees up in time (frame dropped) or the stream is not started.
    bool postVideoFrame(const uint8_t* data, size_t len, uint64_t pts);

private:
    static void* eventStub(void* self);
    void eventLoop();
    bool isIdr(const uint8_t* data, size_t len) const;  // holds a NAL unit of type 5?

    EvoVideoConfig cfg_;
    EvoIlm ilm_;
    DmaBufPool pool_;
    xsv_handle h_;
    std::atomic<bool> running_;
    std::atomic<bool> visible_;
    float opacity_;
    bool ready_;
    bool started_;
    bool configSent_;
    pthread_t evThread_;
    bool evStarted_;
    pthread_mutex_t mu_;
    pthread_cond_t cv_;              // signalled when a dma-buf is released; waits use CLOCK_MONOTONIC
    std::vector<uint8_t> codecCfg_;  // cached SPS/PPS
    bool seenFirstFrame_;
    uint64_t framesIn_, framesDropped_, idrSeen_, framesAtStart_;
};

}  // namespace evo

#endif  // ENTRY_AAD_PLATFORM_EVO_VIDEO_HPP
