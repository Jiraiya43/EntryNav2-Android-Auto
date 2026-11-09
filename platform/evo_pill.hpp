// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Volume pill shown over Android Auto while the volume changes, on a Wayland surface of its own.

// Android Auto stays opaque; a small surface above it shows the level for 1.5 s. Making the Android Auto picture
// transparent instead would let whatever iDrive draws underneath show through.
//
// A Wayland client of its own, on its own thread: wl_shm buffer (premultiplied ARGB8888, platform/volume_pill.hpp)
// on an IVI surface, placed above every surface of the layer with LayerManagerControl (platform/evo_ilm.hpp).
// libwayland-client.so.0 and libivi-application.so.0 are loaded at run time, and the requests are marshalled by hand:
// the car's libwayland predates wl_proxy_marshal_flags, which current headers generate.
// Anything missing (library, interface, surface id taken, compositor refusing) leaves ready() false and Android Auto
// unchanged; the service then falls back to dimming the picture.
#ifndef ENTRY_AAD_PLATFORM_EVO_PILL_HPP
#define ENTRY_AAD_PLATFORM_EVO_PILL_HPP

#include <pthread.h>
#include <stdint.h>

#include <atomic>

namespace evo {

class EvoPill {
public:
    static const unsigned kShowMs = 1500;

    EvoPill();
    ~EvoPill();
    // Starts the thread: surface surfaceId on layerId, bottom centre of a screenW x screenH screen. Returns at once;
    // ready() turns true when the surface is in place (about a second).
    bool start(uint32_t surfaceId, uint32_t layerId, int screenW, int screenH);
    // Thread-safe. Draws the level and shows the pill for kShowMs (each new level extends it).
    void show(unsigned value, unsigned min, unsigned max);
    void hide();
    bool ready() const;
    void stop();

private:
    struct Wayland;
    static void* threadMain(void* self);
    void run();
    void wake();

    pthread_t thread_;
    mutable pthread_mutex_t mu_;
    bool started_;
    std::atomic<bool> running_, ready_;
    int wake_[2];
    uint32_t surfaceId_, layerId_;
    int screenW_, screenH_;
    // Requests from other threads, under mu_.
    bool showPending_, hidePending_;
    unsigned value_, min_, max_;
};

}  // namespace evo

#endif
