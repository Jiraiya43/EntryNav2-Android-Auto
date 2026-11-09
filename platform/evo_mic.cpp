// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Captures the car microphone through the HU's libasound, on a thread woken by the phone's mic requests.
#include "evo_mic.hpp"

#include <dlfcn.h>

#include <cstdlib>

#include "evo_log.hpp"

namespace evo {

namespace {
const int kStreamCapture = 1;                 // SND_PCM_STREAM_CAPTURE
const int kFormatS16LE = 2;                   // SND_PCM_FORMAT_S16_LE
const int kAccessRwInterleaved = 3;           // SND_PCM_ACCESS_RW_INTERLEAVED
const unsigned kRate = 16000, kFrames = 320;  // 20 ms per read
const char* const kPcm = "mic1";              // the car's microphone (observed working on the car)
}  // namespace

EvoMic::EvoMic()
    : cb_(nullptr),
      user_(nullptr),
      lib_(nullptr),
      started_(false),
      quit_(false),
      want_(false),
      open_(nullptr),
      setParams_(nullptr),
      readi_(nullptr),
      recover_(nullptr),
      close_(nullptr),
      strerror_(nullptr) {
    pthread_mutex_init(&mu_, nullptr);
    pthread_cond_init(&cv_, nullptr);
}

EvoMic::~EvoMic() {
    stop();
    if (lib_) {
        dlclose(lib_);
    }
    pthread_cond_destroy(&cv_);
    pthread_mutex_destroy(&mu_);
}

bool EvoMic::load() {
    lib_ = dlopen("libasound.so.2", RTLD_NOW | RTLD_LOCAL);
    if (!lib_) {
        ELOG("Mic", "libasound.so.2 not found: no microphone");
        return false;
    }
    *reinterpret_cast<void**>(&open_) = dlsym(lib_, "snd_pcm_open");
    *reinterpret_cast<void**>(&setParams_) = dlsym(lib_, "snd_pcm_set_params");
    *reinterpret_cast<void**>(&readi_) = dlsym(lib_, "snd_pcm_readi");
    *reinterpret_cast<void**>(&recover_) = dlsym(lib_, "snd_pcm_recover");
    *reinterpret_cast<void**>(&close_) = dlsym(lib_, "snd_pcm_close");
    *reinterpret_cast<void**>(&strerror_) = dlsym(lib_, "snd_strerror");
    if (!open_ || !setParams_ || !readi_ || !recover_ || !close_ || !strerror_) {
        ELOG("Mic", "libasound incomplete: no microphone");
        dlclose(lib_);
        lib_ = nullptr;
        return false;
    }
    return true;
}

bool EvoMic::start(DataCallback cb, void* user) {
    if (started_) {
        return true;
    }
    if (!lib_ && !load()) {
        return false;
    }
    cb_ = cb;
    user_ = user;
    quit_ = false;
    want_ = false;
    if (pthread_create(&thread_, nullptr, threadStub, this) != 0) {
        ELOG("Mic", "cannot start the thread");
        return false;
    }
    started_ = true;
    ELOG("Mic", "ready (%s, 16 kHz mono)", kPcm);
    return true;
}

void EvoMic::request(bool open) {
    pthread_mutex_lock(&mu_);
    want_ = open;
    pthread_cond_signal(&cv_);
    pthread_mutex_unlock(&mu_);
}

void EvoMic::stop() {
    if (!started_) {
        return;
    }
    pthread_mutex_lock(&mu_);
    quit_ = true;
    want_ = false;
    pthread_cond_signal(&cv_);
    pthread_mutex_unlock(&mu_);
    pthread_join(thread_, nullptr);
    started_ = false;
}

void* EvoMic::threadStub(void* arg) {
    static_cast<EvoMic*>(arg)->loop();
    return nullptr;
}

void EvoMic::loop() {
    int16_t buf[kFrames];
    for (;;) {
        pthread_mutex_lock(&mu_);
        while (!quit_ && !want_) {
            pthread_cond_wait(&cv_, &mu_);
        }
        const bool quit = quit_;
        pthread_mutex_unlock(&mu_);
        if (quit) {
            return;
        }

        void* h = nullptr;
        int err = open_(&h, kPcm, kStreamCapture, 0);
        if (err >= 0) {
            err = setParams_(h, kFormatS16LE, kAccessRwInterleaved, 1, kRate, 1, 100000);
        }
        if (err < 0) {
            ELOG("Mic", "cannot open %s: %s", kPcm, strerror_(err));
            if (h) {
                close_(h);
            }
            // Wait for the next open request instead of retrying in a loop.
            pthread_mutex_lock(&mu_);
            while (!quit_ && want_) {
                pthread_cond_wait(&cv_, &mu_);
            }
            pthread_mutex_unlock(&mu_);
            continue;
        }
        ELOG("Mic", "open for the phone (%s)", kPcm);
        unsigned long frames = 0;
        unsigned errors = 0;
        for (;;) {
            pthread_mutex_lock(&mu_);
            const bool keep = want_ && !quit_;
            pthread_mutex_unlock(&mu_);
            if (!keep) {
                break;
            }
            long n = readi_(h, buf, kFrames);
            if (n < 0) {
                if (++errors <= 5) {
                    ELOG("Mic", "read: %s", strerror_(static_cast<int>(n)));
                }
                if (recover_(h, static_cast<int>(n), 1) < 0) {
                    break;
                }
                continue;
            }
            frames += static_cast<unsigned long>(n);
            if (cb_ && n > 0) {
                cb_(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(n) * 2, user_);
            }
        }
        close_(h);
        ELOG("Mic", "closed (%lu ms captured, %u error(s))", frames / 16, errors);
    }
}

}  // namespace evo
