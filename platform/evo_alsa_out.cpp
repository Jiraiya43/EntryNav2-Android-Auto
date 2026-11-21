// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// A playback PCM through the HU's libasound (see evo_alsa_out.hpp).
#include "evo_alsa_out.hpp"

#include <dlfcn.h>
#include <errno.h>

#include <cstdlib>
#include <cstring>

namespace evo {

namespace {
const int kStreamPlayback = 0;       // SND_PCM_STREAM_PLAYBACK
const int kFormatS16LE = 2;          // SND_PCM_FORMAT_S16_LE
const int kAccessRwInterleaved = 3;  // SND_PCM_ACCESS_RW_INTERLEAVED
const char* const kLibrary = "libasound.so.2";
std::string g_testLibrary;
}  // namespace

void AlsaOut::setLibraryForTest(const char* path) {
    g_testLibrary = path ? path : "";
}

AlsaOut::AlsaOut()
    : lib_(nullptr),
      handle_(nullptr),
      channels_(2),
      bufferFrames_(0),
      underruns_(0),
      open_(nullptr),
      setParams_(nullptr),
      writei_(nullptr),
      recover_(nullptr),
      drain_(nullptr),
      prepare_(nullptr),
      getParams_(nullptr),
      close_(nullptr),
      strerror_(nullptr),
      nameHint_(nullptr),
      getHint_(nullptr),
      freeHint_(nullptr) {}

AlsaOut::~AlsaOut() {
    close();
    if (lib_) {
        dlclose(lib_);
    }
}

void AlsaOut::fail(const char* what, int err) {
    error_ = what;
    if (err && strerror_) {
        error_ += ": ";
        error_ += strerror_(err);
    }
}

bool AlsaOut::load() {
    if (lib_) {
        return true;
    }
    const char* const file = g_testLibrary.empty() ? kLibrary : g_testLibrary.c_str();
    lib_ = dlopen(file, RTLD_NOW | RTLD_LOCAL);
    if (!lib_) {
        error_ = std::string(file) + " not found";
        return false;
    }
    *reinterpret_cast<void**>(&open_) = dlsym(lib_, "snd_pcm_open");
    *reinterpret_cast<void**>(&setParams_) = dlsym(lib_, "snd_pcm_set_params");
    *reinterpret_cast<void**>(&writei_) = dlsym(lib_, "snd_pcm_writei");
    *reinterpret_cast<void**>(&recover_) = dlsym(lib_, "snd_pcm_recover");
    *reinterpret_cast<void**>(&drain_) = dlsym(lib_, "snd_pcm_drain");
    *reinterpret_cast<void**>(&prepare_) = dlsym(lib_, "snd_pcm_prepare");
    *reinterpret_cast<void**>(&close_) = dlsym(lib_, "snd_pcm_close");
    *reinterpret_cast<void**>(&strerror_) = dlsym(lib_, "snd_strerror");
    // Optional: the buffer libasound really granted; without it, the one asked for.
    *reinterpret_cast<void**>(&getParams_) = dlsym(lib_, "snd_pcm_get_params");
    // Only list() needs these three: an older library without them still plays.
    *reinterpret_cast<void**>(&nameHint_) = dlsym(lib_, "snd_device_name_hint");
    *reinterpret_cast<void**>(&getHint_) = dlsym(lib_, "snd_device_name_get_hint");
    *reinterpret_cast<void**>(&freeHint_) = dlsym(lib_, "snd_device_name_free_hint");
    if (!open_ || !setParams_ || !writei_ || !recover_ || !drain_ || !prepare_ || !close_ || !strerror_) {
        error_ = std::string(file) + " incomplete";
        dlclose(lib_);
        lib_ = nullptr;
        return false;
    }
    return true;
}

bool AlsaOut::open(const char* pcm, unsigned rate, unsigned channels, unsigned latencyUs) {
    close();
    if (!load()) {
        return false;
    }
    void* h = nullptr;
    int err = open_(&h, pcm, kStreamPlayback, 0);
    if (err < 0) {
        fail("open", err);
        return false;
    }
    // Soft resampling allowed: a PCM of another rate still plays when the car's configuration can convert.
    err = setParams_(h, kFormatS16LE, kAccessRwInterleaved, channels, rate, 1, latencyUs);
    if (err < 0) {
        fail("parameters", err);
        close_(h);
        return false;
    }
    handle_ = h;
    channels_ = channels;
    bufferFrames_ = static_cast<size_t>(static_cast<uint64_t>(rate) * latencyUs / 1000000);
    unsigned long buffer = 0, period = 0;
    if (getParams_ && getParams_(h, &buffer, &period) >= 0 && buffer) {
        bufferFrames_ = buffer;
    }
    return true;
}

bool AlsaOut::write(const int16_t* frames, size_t count) {
    if (!handle_) {
        return false;
    }
    while (count) {
        const long n = writei_(handle_, frames, count);
        if (n < 0) {
            if (n == -EPIPE) {
                ++underruns_;
            }
            const int err = recover_(handle_, static_cast<int>(n), 1);
            if (err < 0) {
                fail("write", err);
                return false;
            }
            continue;
        }
        frames += static_cast<size_t>(n) * channels_;
        count -= static_cast<size_t>(n);
    }
    return true;
}

void AlsaOut::drain() {
    if (handle_) {
        drain_(handle_);
        prepare_(handle_);
    }
}

void AlsaOut::close() {
    if (handle_) {
        close_(handle_);
        handle_ = nullptr;
    }
}

bool AlsaOut::list(std::vector<std::string>& names) {
    names.clear();
    if (!load()) {
        return false;
    }
    if (!nameHint_ || !getHint_ || !freeHint_) {
        error_ = "this libasound cannot list its PCMs";
        return false;
    }
    void** hints = nullptr;
    const int err = nameHint_(-1, "pcm", &hints);
    if (err < 0 || !hints) {
        fail("list", err);
        return false;
    }
    for (void** hint = hints; *hint; ++hint) {
        char* name = getHint_(*hint, "NAME");
        char* io = getHint_(*hint, "IOID");  // null: both ways
        char* about = getHint_(*hint, "DESC");
        if (name && (!io || !strcmp(io, "Output"))) {
            std::string line(name);
            if (about) {
                line += "  (";
                for (const char* c = about; *c; ++c) {
                    line += *c == '\n' ? ' ' : *c;
                }
                line += ")";
            }
            names.push_back(line);
        }
        free(name);
        free(io);
        free(about);
    }
    freeHint_(hints);
    return true;
}

}  // namespace evo
