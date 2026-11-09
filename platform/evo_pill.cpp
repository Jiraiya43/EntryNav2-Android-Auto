// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Volume pill over Android Auto: see evo_pill.hpp.
#include "evo_pill.hpp"

#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "evo_ilm.hpp"
#include "evo_log.hpp"
#include "volume_pill.hpp"

namespace evo {

namespace {

struct wl_proxy;
struct wl_display;
// Only the first field is read here; the interfaces themselves come from the car's libraries.
struct WlInterface {
    const char* name;
};

// Request and event numbers of the protocols the car speaks (wayland.xml, ivi-application.xml of its compositor).
const uint32_t kDisplayGetRegistry = 1, kRegistryBind = 0, kCompositorCreateSurface = 0;
const uint32_t kShmCreatePool = 0, kPoolCreateBuffer = 0, kPoolDestroy = 1, kBufferDestroy = 0;
const uint32_t kSurfaceDestroy = 0, kSurfaceAttach = 1, kSurfaceDamage = 2, kSurfaceCommit = 6;
const uint32_t kIviApplicationSurfaceCreate = 0, kIviSurfaceDestroy = 0;
const uint32_t kShmFormatArgb8888 = 0;

}  // namespace

struct EvoPill::Wayland {
    void* lib;
    void* ivi;
    wl_display* (*connect)(const char*);
    void (*disconnect)(wl_display*);
    int (*roundtrip)(wl_display*);
    int (*dispatch)(wl_display*);
    int (*flush)(wl_display*);
    int (*getFd)(wl_display*);
    void (*marshal)(wl_proxy*, uint32_t, ...);
    wl_proxy* (*construct)(wl_proxy*, uint32_t, const WlInterface*, ...);
    int (*addListener)(wl_proxy*, void (**)(void), void*);
    void (*destroy)(wl_proxy*);
    const WlInterface *registryI, *compositorI, *surfaceI, *shmI, *poolI, *bufferI, *iviAppI, *iviSurfaceI;

    wl_display* display;
    wl_proxy *registry, *compositor, *shm, *iviApp, *surface, *iviSurface, *pool, *buffers[2];
    bool busy[2];
    int lastCommitted;
    uint32_t* pixels;
    size_t mapped;

    template <typename T>
    bool sym(void* from, const char* name, T& out) {
        void* p = dlsym(from, name);
        memcpy(&out, &p, sizeof(p));
        return p != nullptr;
    }
    bool load() {
        lib = dlopen("libwayland-client.so.0", RTLD_NOW | RTLD_LOCAL);
        ivi = dlopen("libivi-application.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!lib || !ivi) {
            ELOG("VolumePill", "Wayland library missing (%s): no pill", dlerror());
            return false;
        }
        const bool ok =
            sym(lib, "wl_display_connect", connect) && sym(lib, "wl_display_disconnect", disconnect) &&
            sym(lib, "wl_display_roundtrip", roundtrip) && sym(lib, "wl_display_dispatch", dispatch) &&
            sym(lib, "wl_display_flush", flush) && sym(lib, "wl_display_get_fd", getFd) &&
            sym(lib, "wl_proxy_marshal", marshal) && sym(lib, "wl_proxy_marshal_constructor", construct) &&
            sym(lib, "wl_proxy_add_listener", addListener) && sym(lib, "wl_proxy_destroy", destroy) &&
            sym(lib, "wl_registry_interface", registryI) && sym(lib, "wl_compositor_interface", compositorI) &&
            sym(lib, "wl_surface_interface", surfaceI) && sym(lib, "wl_shm_interface", shmI) &&
            sym(lib, "wl_shm_pool_interface", poolI) && sym(lib, "wl_buffer_interface", bufferI) &&
            sym(ivi, "ivi_application_interface", iviAppI) && sym(ivi, "ivi_surface_interface", iviSurfaceI);
        if (!ok) {
            ELOG("VolumePill", "Wayland functions missing: no pill");
        }
        return ok;
    }

    static void onGlobal(void* data, wl_proxy* reg, uint32_t name, const char* iface, uint32_t) {
        Wayland* w = static_cast<Wayland*>(data);
        if (!iface) {
            return;
        }
        const WlInterface* want = nullptr;
        wl_proxy** slot = nullptr;
        if (!strcmp(iface, "wl_compositor")) {
            want = w->compositorI;
            slot = &w->compositor;
        } else if (!strcmp(iface, "wl_shm")) {
            want = w->shmI;
            slot = &w->shm;
        } else if (!strcmp(iface, "ivi_application")) {
            want = w->iviAppI;
            slot = &w->iviApp;
        }
        if (slot && !*slot) {
            *slot = w->construct(reg, kRegistryBind, want, name, want->name, 1u, nullptr);  // version 1, as offered
        }
    }
    static void onGlobalRemove(void*, wl_proxy*, uint32_t) {}
    static void onRelease(void* data, wl_proxy* buffer) {
        Wayland* w = static_cast<Wayland*>(data);
        for (int i = 0; i < 2; ++i) {
            if (w->buffers[i] == buffer) {
                w->busy[i] = false;
            }
        }
    }

    // Connection, the three globals, the surface and two buffers. False at the first refusal.
    bool open(uint32_t surfaceId) {
        display = connect(nullptr);
        if (!display) {
            ELOG("VolumePill", "the compositor refused the connection: no pill");
            return false;
        }
        registry = construct(reinterpret_cast<wl_proxy*>(display), kDisplayGetRegistry, registryI, nullptr);
        static void (*const kRegistryListener[2])(void) = {reinterpret_cast<void (*)(void)>(&onGlobal),
                                                           reinterpret_cast<void (*)(void)>(&onGlobalRemove)};
        if (!registry || addListener(registry, const_cast<void (**)(void)>(kRegistryListener), this) != 0 ||
            roundtrip(display) < 0 || roundtrip(display) < 0) {
            ELOG("VolumePill", "the compositor's interface list unreadable: no pill");
            return false;
        }
        if (!compositor || !shm || !iviApp) {
            ELOG("VolumePill", "compositor without %s: no pill",
                 !compositor ? "wl_compositor"
                 : !shm      ? "wl_shm"
                             : "ivi_application");
            return false;
        }
        surface = construct(compositor, kCompositorCreateSurface, surfaceI, nullptr);
        iviSurface = surface ? construct(iviApp, kIviApplicationSurfaceCreate, iviSurfaceI, surfaceId, surface, nullptr)
                             : nullptr;
        if (!iviSurface || !makeBuffers()) {
            return false;
        }
        return roundtrip(display) >= 0;  // a taken id ends the connection here
    }
    bool makeBuffers() {
        const size_t one = static_cast<size_t>(pill::kWidth) * pill::kHeight * 4;
        mapped = one * 2;
        const char* dir = getenv("XDG_RUNTIME_DIR");
        std::string path = std::string(dir && *dir ? dir : "/tmp") + "/entry-aad-pill-XXXXXX";
        int fd = mkstemp(&path[0]);
        if (fd < 0) {
            ELOG("VolumePill", "no shared memory (%s): no pill", strerror(errno));
            return false;
        }
        unlink(path.c_str());
        void* p = ftruncate(fd, static_cast<off_t>(mapped)) == 0
                      ? mmap(nullptr, mapped, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)
                      : MAP_FAILED;
        if (p == MAP_FAILED) {
            ::close(fd);
            ELOG("VolumePill", "no shared memory: no pill");
            return false;
        }
        pixels = static_cast<uint32_t*>(p);
        memset(pixels, 0, mapped);
        pool = construct(shm, kShmCreatePool, poolI, nullptr, fd, static_cast<int32_t>(mapped));
        ::close(fd);  // the request carries its own copy of the descriptor
        static void (*const kBufferListener[1])(void) = {reinterpret_cast<void (*)(void)>(&onRelease)};
        for (int i = 0; pool && i < 2; ++i) {
            buffers[i] = construct(pool, kPoolCreateBuffer, bufferI, nullptr, static_cast<int32_t>(one * i),
                                   pill::kWidth, pill::kHeight, pill::kWidth * 4, kShmFormatArgb8888);
            if (!buffers[i] || addListener(buffers[i], const_cast<void (**)(void)>(kBufferListener), this) != 0) {
                return false;
            }
        }
        return pool != nullptr;
    }
    // Draws into a free buffer and commits it. A compositor that keeps both (a hidden surface may never release them)
    // gets the older one rewritten: at worst a torn frame of a small pill, never a frozen volume display.
    bool present(unsigned value, unsigned min, unsigned max, bool clear) {
        const int i = !busy[0] ? 0 : !busy[1] ? 1 : 1 - lastCommitted;
        uint32_t* px = pixels + static_cast<size_t>(pill::kWidth) * pill::kHeight * i;
        if (clear) {
            memset(px, 0, static_cast<size_t>(pill::kWidth) * pill::kHeight * 4);
        } else {
            pill::draw(px, pill::kWidth, value, min, max);
        }
        marshal(surface, kSurfaceAttach, buffers[i], 0, 0);
        marshal(surface, kSurfaceDamage, 0, 0, pill::kWidth, pill::kHeight);
        marshal(surface, kSurfaceCommit);
        busy[i] = true;
        lastCommitted = i;
        flush(display);
        return true;
    }
    void shutdown() {
        if (!display) {
            return;
        }
        for (int i = 0; i < 2; ++i) {
            if (buffers[i]) {
                marshal(buffers[i], kBufferDestroy);
                destroy(buffers[i]);
            }
        }
        if (pool) {
            marshal(pool, kPoolDestroy);
            destroy(pool);
        }
        if (iviSurface) {
            marshal(iviSurface, kIviSurfaceDestroy);
            destroy(iviSurface);
        }
        if (surface) {
            marshal(surface, kSurfaceDestroy);
            destroy(surface);
        }
        flush(display);
        disconnect(display);  // the compositor drops whatever is left with the connection
        display = nullptr;
        if (pixels) {
            munmap(pixels, mapped);
            pixels = nullptr;
        }
    }
};

EvoPill::EvoPill()
    : started_(false),
      running_(false),
      ready_(false),
      surfaceId_(0),
      layerId_(0),
      screenW_(0),
      screenH_(0),
      showPending_(false),
      hidePending_(false),
      value_(0),
      min_(0),
      max_(0) {
    pthread_mutex_init(&mu_, nullptr);
    wake_[0] = wake_[1] = -1;
}

EvoPill::~EvoPill() {
    stop();
    pthread_mutex_destroy(&mu_);
}

bool EvoPill::start(uint32_t surfaceId, uint32_t layerId, int screenW, int screenH) {
    if (started_ || !surfaceId || screenW < pill::kWidth || screenH < pill::kHeight + pill::kBottomMargin) {
        return false;
    }
    if (pipe(wake_) != 0) {
        return false;
    }
    fcntl(wake_[1], F_SETFL, fcntl(wake_[1], F_GETFL, 0) | O_NONBLOCK);
    surfaceId_ = surfaceId;
    layerId_ = layerId;
    screenW_ = screenW;
    screenH_ = screenH;
    running_ = true;
    if (pthread_create(&thread_, nullptr, threadMain, this) != 0) {
        running_ = false;
        ::close(wake_[0]);
        ::close(wake_[1]);
        wake_[0] = wake_[1] = -1;
        return false;
    }
    started_ = true;
    return true;
}

void EvoPill::show(unsigned value, unsigned min, unsigned max) {
    pthread_mutex_lock(&mu_);
    showPending_ = true;
    hidePending_ = false;
    value_ = value;
    min_ = min;
    max_ = max;
    pthread_mutex_unlock(&mu_);
    wake();
}

void EvoPill::hide() {
    pthread_mutex_lock(&mu_);
    const bool needed = !hidePending_;
    showPending_ = false;
    hidePending_ = true;
    pthread_mutex_unlock(&mu_);
    if (needed) {
        wake();
    }
}

bool EvoPill::ready() const {
    return ready_;
}

void EvoPill::wake() {
    if (wake_[1] >= 0) {
        const char c = 1;
        ssize_t w = write(wake_[1], &c, 1);
        (void)w;
    }
}

void EvoPill::stop() {
    if (!started_) {
        return;
    }
    running_ = false;
    wake();
    pthread_join(thread_, nullptr);
    started_ = false;
    ::close(wake_[0]);
    ::close(wake_[1]);
    wake_[0] = wake_[1] = -1;
}

void* EvoPill::threadMain(void* self) {
    static_cast<EvoPill*>(self)->run();
    return nullptr;
}

void EvoPill::run() {
    Wayland w = Wayland();  // plain data: every pointer null
    EvoIlm ilm;
    const uint32_t x = static_cast<uint32_t>((screenW_ - pill::kWidth) / 2);
    const uint32_t y = static_cast<uint32_t>(screenH_ - pill::kHeight - pill::kBottomMargin);
    bool placed = false;
    if (!ilm.surfaceAbsent(surfaceId_)) {
        ELOG("VolumePill", "surface 0x%x already taken or inventory unreadable: no pill", surfaceId_);
    } else if (w.load() && w.open(surfaceId_) && w.present(0, 0, 0, true)) {
        // The layer manager knows the surface once the compositor has its first buffer.
        for (int i = 0; i < 20 && running_ && !ilm.surfaceExists(surfaceId_); ++i) {
            usleep(100000);
        }
        placed = ilm.attachOverlay(surfaceId_, layerId_, x, y, pill::kWidth, pill::kHeight);
        ELOG("VolumePill", "pill 0x%x %s (%ux%u at %u,%u, above Android Auto)", surfaceId_,
             placed ? "ready" : "refused by the layer manager", pill::kWidth, pill::kHeight, x, y);
    }
    ready_ = placed;
    bool visible = false;
    uint32_t hideAt = 0;
    const int fd = w.display ? w.getFd(w.display) : -1;
    while (running_ && placed) {
        struct pollfd p[2];
        p[0].fd = wake_[0];
        p[0].events = POLLIN;
        p[0].revents = 0;
        p[1].fd = fd;
        p[1].events = POLLIN;
        p[1].revents = 0;
        const int r = poll(p, 2, 100);
        if (r < 0 && errno != EINTR) {
            break;
        }
        if (p[0].revents & POLLIN) {
            char buf[32];
            ssize_t n = read(wake_[0], buf, sizeof(buf));
            (void)n;
        }
        if ((p[1].revents & (POLLERR | POLLHUP)) || ((p[1].revents & POLLIN) && w.dispatch(w.display) < 0)) {
            ELOG("VolumePill", "compositor disconnected: pill stopped, back to dimming");
            break;
        }
        pthread_mutex_lock(&mu_);
        const bool wantShow = showPending_, wantHide = hidePending_;
        const unsigned v = value_, lo = min_, hi = max_;
        pthread_mutex_unlock(&mu_);
        const uint32_t now = monoMs();
        if (wantShow && w.present(v, lo, hi, false)) {
            pthread_mutex_lock(&mu_);
            showPending_ = false;  // a level that arrived meanwhile stays pending for the next turn
            if (value_ != v || min_ != lo || max_ != hi) {
                showPending_ = true;
            }
            pthread_mutex_unlock(&mu_);
            if (!visible) {
                // A new Android Auto session puts the picture on top again: back above it before showing.
                visible = ilm.raiseSurface(surfaceId_, layerId_) && ilm.setSurfaceVisibility(surfaceId_, true);
            }
            hideAt = now + kShowMs;
        }
        if (visible && (wantHide || static_cast<int32_t>(now - hideAt) >= 0)) {
            ilm.setSurfaceVisibility(surfaceId_, false);
            visible = false;
        }
        if (wantHide) {
            pthread_mutex_lock(&mu_);
            hidePending_ = false;
            pthread_mutex_unlock(&mu_);
        }
    }
    ready_ = false;
    if (placed) {
        ilm.setSurfaceVisibility(surfaceId_, false);
        ilm.detachSurface(surfaceId_, layerId_);
    }
    w.shutdown();
}

}  // namespace evo
