// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// One playback PCM of the car, through the HU's own libasound.so.2 loaded with dlopen (as platform/evo_mic.hpp does
// for the microphone): S16LE interleaved, blocking writes. Used by the service (platform/evo_audio.hpp) and by the
// evo-sound tool run by hand. Nothing is sent on vMOST/MOST: only what the car's /etc/asound.conf declares is opened.
//
// Not thread-safe: one thread opens, writes and closes.
#ifndef AA_ENTRYNAV_PLATFORM_EVO_ALSA_OUT_HPP
#define AA_ENTRYNAV_PLATFORM_EVO_ALSA_OUT_HPP

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace evo {

class AlsaOut {
public:
    AlsaOut();
    ~AlsaOut();

    // The library file loaded instead of libasound.so.2 (tests/fake_asound.c); empty: the car's.
    static void setLibraryForTest(const char* path);

    // False when libasound or one of its functions is missing: lastError() says which.
    bool load();
    // Opens `pcm` for playback. latencyUs: the buffer libasound keeps, which is also how late the sound comes out.
    bool open(const char* pcm, unsigned rate, unsigned channels, unsigned latencyUs);
    bool isOpen() const { return handle_ != nullptr; }
    // The frames the open PCM holds before a write has to wait (what libasound granted, else what was asked).
    size_t bufferFrames() const { return bufferFrames_; }
    // Blocks until every frame is taken. An underrun is recovered and counted; false when the PCM is lost.
    bool write(const int16_t* frames, size_t count);
    // What was written plays to its end (a short sound never fills the buffer that starts the PCM by itself), and the
    // PCM is ready for more.
    void drain();
    void close();
    unsigned underruns() const { return underruns_; }
    const std::string& lastError() const { return error_; }

    // The PCM names the car's configuration declares for playback, with their description when there is one.
    bool list(std::vector<std::string>& names);

private:
    void fail(const char* what, int err);

    void* lib_;
    void* handle_;
    unsigned channels_;
    size_t bufferFrames_;
    unsigned underruns_;
    std::string error_;
    // libasound entry points (resolved with dlsym)
    int (*open_)(void**, const char*, int, int);
    int (*setParams_)(void*, int, int, unsigned, unsigned, int, unsigned);
    long (*writei_)(void*, const void*, unsigned long);
    int (*recover_)(void*, int, int);
    int (*drain_)(void*);
    int (*prepare_)(void*);
    int (*getParams_)(void*, unsigned long*, unsigned long*);
    int (*close_)(void*);
    const char* (*strerror_)(int);
    int (*nameHint_)(int, const char*, void***);
    char* (*getHint_)(const void*, const char*);
    int (*freeHint_)(void**);
};

}  // namespace evo

#endif  // AA_ENTRYNAV_PLATFORM_EVO_ALSA_OUT_HPP
