// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Sound over Wi-Fi: settings, and the thread that plays the mix on the car (see evo_audio.hpp).
#include "evo_audio.hpp"

#include <errno.h>
#include <time.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "aa_protocol.hpp"
#include "audio_route.hpp"
#include "evo_log.hpp"

namespace evo {

namespace {
const char* const kTag = "Audio";
const size_t kPeriodFrames = 480;         // 10 ms per write
const unsigned kLatencyUs = 100000;       // libasound's buffer
const uint32_t kDrainAfterMs = 60;        // nothing to play this long: what was written plays to its end
const uint32_t kCloseAfterMs = 5000;      // then the PCM is given back
const uint32_t kReleaseAfterStopMs = 2000;  // a pause the phone announced (STOP): the radio comes back sooner
const uint32_t kRetryOpenAfterMs = 2000;  // a PCM that did not open is not tried again before this
const uint32_t kMusicRecentMs = 3000;     // the phone's music counts as playing this long after its last sound
const uint32_t kPromptMaxMs = 15000;      // a prompt waiting for its route keeps this much, the oldest goes
const uint32_t kPromptPrebufferMs = 60;   // as the mix does for speech
const uint32_t kPromptFlushMs = 80;
const unsigned kGapsLogged = 20;  // gaps and trims of the music in a session, each: then one in kGapsLogEvery
const unsigned kGapsLogEvery = 20;

bool logged(unsigned count) {
    return count <= kGapsLogged || count % kGapsLogEvery == 0;
}

// The formats of the description sent to the phone (core/include/dhu_sdp.h, as core/sdr_builder.cpp leaves it):
// channel 9 has one configuration, channel 8 two.
bool formatOf(uint8_t channel, uint32_t config, AudioMixer::Stream& stream, unsigned& rate, unsigned& channels) {
    if (channel == aa::channel::kMediaAudio && config == 0) {
        stream = AudioMixer::kMedia;
        rate = 48000;
        channels = 2;
        return true;
    }
    if (channel == aa::channel::kGuidanceAudio && config <= 1) {
        stream = AudioMixer::kSpeech;
        rate = config == 0 ? 16000 : 48000;
        channels = 1;
        return true;
    }
    return false;
}

bool streamOf(uint8_t channel, AudioMixer::Stream& stream) {
    if (channel != aa::channel::kMediaAudio && channel != aa::channel::kGuidanceAudio) {
        return false;
    }
    stream = channel == aa::channel::kMediaAudio ? AudioMixer::kMedia : AudioMixer::kSpeech;
    return true;
}

// 0 to 100, else `keep`.
int percentOr(const std::string& text, int keep) {
    if (text.empty() || text.size() > 3) {
        return keep;
    }
    int value = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return keep;
        }
        value = value * 10 + (text[i] - '0');
    }
    return value <= 100 ? value : keep;
}

// A route number in hex, 0 to ffff. False, and `route` untouched, otherwise.
bool hexRoute(const std::string& text, uint16_t& route) {
    char* end = nullptr;
    const unsigned long value = strtoul(text.c_str(), &end, 16);
    if (text.empty() || !end || *end || value > 0xffff) {
        return false;
    }
    route = static_cast<uint16_t>(value);
    return true;
}
}  // namespace

bool validPcmName(const std::string& name) {
    if (name.empty() || name.size() > 63) {
        return false;
    }
    for (size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
                        c == '-' || c == '.' || c == ':' || c == ',' || c == '=';
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool loadAudioSettings(const char* path, AudioSettings& settings) {
    FILE* file = fopen(path, "r");
    if (!file) {
        return false;
    }
    char text[96];
    bool routeSet = false, promptRouteSet = false;
    while (fgets(text, sizeof(text), file)) {
        std::string line(text);
        while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r')) {
            line.erase(line.size() - 1);
        }
        if (line.compare(0, 4, "pcm=") == 0 && validPcmName(line.substr(4))) {
            settings.pcm = line.substr(4);
        } else if (line.compare(0, 5, "gain=") == 0) {
            settings.gainPercent = percentOr(line.substr(5), settings.gainPercent);
        } else if (line.compare(0, 5, "duck=") == 0) {
            settings.duckPercent = percentOr(line.substr(5), settings.duckPercent);
        } else if (line.compare(0, 6, "route=") == 0) {
            routeSet = hexRoute(line.substr(6), settings.route) || routeSet;
        } else if (line.compare(0, 11, "prompt-pcm=") == 0 && validPcmName(line.substr(11))) {
            settings.promptPcm = line.substr(11);
        } else if (line.compare(0, 13, "prompt-route=") == 0) {
            promptRouteSet = hexRoute(line.substr(13), settings.promptRoute) || promptRouteSet;
        }
    }
    fclose(file);
    // 0x315 is the route to "entertainment" only: another PCM gets none unless the file names one.
    if (!routeSet && settings.pcm != "entertainment") {
        settings.route = 0;
    }
    // CarPlay's prompt route goes with its media route: a sound set up another way keeps its prompts in the mix.
    if (!promptRouteSet && (settings.pcm != "entertainment" || settings.route != 0x315)) {
        settings.promptRoute = 0;
    }
    return true;
}

namespace {
// Only the result crosses threads. The request keeps its own reference until the helper finishes.
using RouteResult = std::shared_ptr<std::atomic<int> >;

void routeDone(bool granted, void* user) {
    std::unique_ptr<RouteResult> result(static_cast<RouteResult*>(user));
    (*result)->store(granted ? 1 : -1);
}

RouteResult buildRoute(uint16_t route, bool restore = false) {
    RouteResult result = std::make_shared<std::atomic<int> >(0);
    requestAudioRoute(true, route, false, routeDone, new RouteResult(result), restore);
    return result;
}
}  // namespace

EvoAudio::EvoAudio()
    : thread_(0),
      started_(false),
      quit_(false),
      lastMixMs_(0),
      mediaRefusedMs_(0),
      mediaBackoff_(false),
      mediaPlayed_(nullptr),
      mediaPlayedUser_(nullptr),
      mediaDryMs_(0),
      mediaGaps_(0),
      mediaTrims_(0),
      promptThread_(0),
      promptThreadRunning_(false),
      promptHead_(0),
      promptRate_(0),
      promptAlt_(false),
      promptEnded_(false),
      promptPlaying_(false),
      promptPushMs_(0),
      musicOpen_(false),
      mediaStopped_(false),
      musicSoundMs_(0) {
    pthread_mutex_init(&mu_, nullptr);
    // Timed waits on the monotonic clock: the car's wall clock jumps when its GPS sets it.
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&cv_, &attr);
    pthread_cond_init(&promptCv_, &attr);
    pthread_condattr_destroy(&attr);
}

EvoAudio::~EvoAudio() {
    stop();
    pthread_cond_destroy(&promptCv_);
    pthread_cond_destroy(&cv_);
    pthread_mutex_destroy(&mu_);
}

void EvoAudio::askPromptRoute(bool build) {
    if (build) {
        promptRoute_ = buildRoute(settings_.promptRoute);
    } else {
        promptRoute_.reset();
        requestAudioRoute(false, settings_.promptRoute);
    }
}

bool EvoAudio::start(const AudioSettings& settings) {
    if (started_) {
        return true;
    }
    if (!out_.load()) {
        ELOG(kTag, "%s: no sound over Wi-Fi", out_.lastError().c_str());
        return false;
    }
    settings_ = settings;
    mixer_ = AudioMixer(settings.gainPercent, settings.duckPercent);
    quit_ = false;
    if (pthread_create(&thread_, nullptr, threadStub, this) != 0) {
        ELOG(kTag, "cannot start the thread: no sound over Wi-Fi");
        return false;
    }
    started_ = true;
    // Prompts take their own path only with their PCM and their thread; else they stay in the mix, as before.
    promptThreadRunning_ =
        settings_.promptRoute && promptOut_.load() && pthread_create(&promptThread_, nullptr, promptStub, this) == 0;
    pthread_mutex_lock(&mu_);
    path_ = PromptPath(promptThreadRunning_);
    pthread_mutex_unlock(&mu_);
    ELOG(kTag, "ready: PCM '%s', gain %d %%, media at %d %% under speech, route %s", settings_.pcm.c_str(),
         settings_.gainPercent, settings_.duckPercent, settings_.route ? "asked at the first sound" : "none");
    if (promptThreadRunning_) {
        ELOG(kTag, "prompts without the phone's music: PCM '%s' under route 0x%x, the car's sound lowered",
             settings_.promptPcm.c_str(), settings_.promptRoute);
    } else {
        ELOG(kTag, "prompts: always in the mix%s", settings_.promptRoute ? " (their own path could not start)" : "");
    }
    return true;
}

void EvoAudio::stop() {
    if (!started_) {
        return;
    }
    pthread_mutex_lock(&mu_);
    quit_ = true;
    pthread_cond_signal(&cv_);
    pthread_cond_signal(&promptCv_);
    pthread_mutex_unlock(&mu_);
    pthread_join(thread_, nullptr);
    if (promptThreadRunning_) {
        pthread_join(promptThread_, nullptr);
        promptThreadRunning_ = false;
    }
    started_ = false;
    pthread_mutex_lock(&mu_);
    const PromptPath::Action prompt = path_.stop();
    promptAlt_ = false;
    if (prompt == PromptPath::kRemove) {
        askPromptRoute(false);
    }
    releaseRoute();
    pthread_mutex_unlock(&mu_);
    waitForAudioRoutes();
}

void EvoAudio::releaseRoute(bool restore) {
    if (mediaRoute_) {
        requestAudioRoute(false, settings_.route, false, nullptr, nullptr, restore);
        mediaRoute_.reset();
    }
    mediaBackoff_ = false;
}

void EvoAudio::updateMediaRoute(bool have, uint32_t now) {
    if (!settings_.route) {
        return;
    }
    if (mediaRoute_ && mediaRoute_->load() < 0) {
        mediaRefusedMs_ = now;
        mediaBackoff_ = true;
        // A timeout can also mean a lost answer. Remove before the next build.
        requestAudioRoute(false, settings_.route);
        mediaRoute_.reset();
    }
    if (have) {
        lastMixMs_ = now;
        if (!mediaRoute_ && (!mediaBackoff_ || now - mediaRefusedMs_ >= kRetryOpenAfterMs)) {
            mediaRoute_ = buildRoute(settings_.route, true);  // the radio it replaces comes back at its release
            mediaBackoff_ = false;
        }
    } else if (mediaRoute_ && now - lastMixMs_ >= (mediaStopped_ ? kReleaseAfterStopMs : kCloseAfterMs)) {
        // Silence without STOP stays 5 s: a Wi-Fi stall of a few seconds must not bring the radio in mid-song.
        releaseRoute();
    }
}

bool EvoAudio::holdsRoute() {
    pthread_mutex_lock(&mu_);
    const bool held = mediaRoute_ && mediaRoute_->load() == 1;
    pthread_mutex_unlock(&mu_);
    return held;
}

bool EvoAudio::musicPlaying(uint32_t nowMs) const {
    return musicOpen_ || (musicSoundMs_ && nowMs - musicSoundMs_ < kMusicRecentMs);
}

// Under mu_. A prompt starts at `rate`, mono: its own path when the phone's music isn't playing on the car.
void EvoAudio::startPrompt(unsigned rate, uint32_t nowMs) {
    promptEnded_ = false;
    if (promptAlt_ && path_.inPrompt()) {
        return;  // the last one has not played out: this one follows it the same way
    }
    PromptPath::Action action = PromptPath::kNothing;
    const bool music = musicPlaying(nowMs);
    promptAlt_ = path_.start(music, nowMs, action);
    prompt_.clear();
    promptHead_ = 0;
    promptRate_ = rate;
    promptPlaying_ = false;
    if (promptAlt_) {
        ELOG(kTag, "prompt on PCM '%s'%s", settings_.promptPcm.c_str(),
             action == PromptPath::kBuild ? ", its route asked for" : ", route kept from the last one");
    } else if (path_.backingOff()) {
        ELOG(kTag, "prompt in the mix: the car refused its own route");
    } else if (music && promptThreadRunning_) {
        ELOG(kTag, "prompt in the mix: the phone's music plays");
    }
    if (action == PromptPath::kBuild) {
        askPromptRoute(true);
    }
}

void EvoAudio::stream(uint8_t channel, bool started, uint32_t config) {
    AudioMixer::Stream s = AudioMixer::kMedia;
    unsigned rate = 0, channels = 0;
    if (started) {
        const bool known = formatOf(channel, config, s, rate, channels);
        if (!known) {
            // Silent rather than at the wrong speed.
            ELOG(kTag, "channel %u configuration %u: format not known, stream not played", channel, config);
            if (!streamOf(channel, s)) {
                return;
            }
        }
        if (known) {
            ELOG(kTag, "%s starts: %u Hz, %u channel(s)", s == AudioMixer::kMedia ? "media" : "speech", rate, channels);
        }
        pthread_mutex_lock(&mu_);
        mixer_.configure(s, rate, channels);  // a prompt that goes its own way can still come back to the mix
        if (s == AudioMixer::kMedia) {
            if (known && mediaStopped_) {
                // iDrive may have selected FM even during a short pause. Reacquire on new sound, no radio in between.
                releaseRoute(false);
                mediaStopped_ = false;
            }
            musicOpen_ = known;
        } else if (known && channels == 1) {
            startPrompt(rate, monoMs());
        } else {
            promptAlt_ = false;
        }
        pthread_mutex_unlock(&mu_);
        return;
    }
    if (!streamOf(channel, s)) {
        return;
    }
    pthread_mutex_lock(&mu_);
    if (s == AudioMixer::kMedia) {
        musicOpen_ = false;
        mediaStopped_ = true;
        mediaDryMs_ = 0;  // stopped by the phone: its next sound is no gap ending
    }
    if (s == AudioMixer::kSpeech && promptAlt_) {
        promptEnded_ = true;
        pthread_cond_signal(&promptCv_);
    } else {
        mixer_.end(s);
        pthread_cond_signal(&cv_);
    }
    pthread_mutex_unlock(&mu_);
    ELOG(kTag, "%s stops", s == AudioMixer::kMedia ? "media" : "speech");
}

void EvoAudio::data(uint8_t channel, const uint8_t* pcm, size_t len) {
    AudioMixer::Stream s = AudioMixer::kMedia;
    if (!streamOf(channel, s)) {
        return;
    }
    // Media that will never be played, or that left the queue: all of it unless the mix takes it below.
    size_t played = s == AudioMixer::kMedia ? len : 0;
    if (len < 2) {
        if (played && mediaPlayed_) {
            mediaPlayed_(played, mediaPlayedUser_);
        }
        return;
    }
    // The phone's bytes are little-endian samples at any address: copied into place first.
    std::vector<int16_t> samples(len / 2);
    memcpy(&samples[0], pcm, samples.size() * 2);
    const uint32_t now = monoMs();
    unsigned long trimmedMs = 0;
    unsigned trims = 0;
    pthread_mutex_lock(&mu_);
    if (started_ && !quit_) {
        if (s == AudioMixer::kSpeech && promptAlt_) {
            prompt_.insert(prompt_.end(), samples.begin(), samples.end());
            promptPushMs_ = now;
            const size_t keep = static_cast<size_t>(promptRate_) * kPromptMaxMs / 1000;
            if (prompt_.size() - promptHead_ > keep) {
                promptHead_ = prompt_.size() - keep;  // waiting too long for its route: the oldest goes
            }
            pthread_cond_signal(&promptCv_);
        } else {
            if (s == AudioMixer::kMedia) {
                musicSoundMs_ = now ? now : 1;
            }
            const unsigned long droppedBefore = mixer_.stats().dropped[s];
            const size_t taken = mixer_.push(s, &samples[0], samples.size(), now);
            if (s == AudioMixer::kMedia) {
                played = len - taken * sizeof(int16_t) + mixer_.takePlayedBytes(s);
                trimmedMs = (mixer_.stats().dropped[s] - droppedBefore) / (kMixRate / 1000);
                trims = trimmedMs ? ++mediaTrims_ : 0;
            }
            pthread_cond_signal(&cv_);
        }
    }
    pthread_mutex_unlock(&mu_);
    if (played && mediaPlayed_) {
        mediaPlayed_(played, mediaPlayedUser_);
    }
    if (trims && logged(trims)) {
        ELOG(kTag, "media: more than %u ms waited, the oldest %lu ms dropped (trim %u of this session)",
             static_cast<unsigned>(AudioMixer::kMediaMaxQueueMs), trimmedMs, trims);
    }
}

void EvoAudio::sessionEnded() {
    pthread_mutex_lock(&mu_);
    mixer_.clear();
    prompt_.clear();
    promptHead_ = 0;
    promptAlt_ = false;
    promptEnded_ = false;
    promptPlaying_ = false;
    musicOpen_ = false;
    mediaStopped_ = false;
    musicSoundMs_ = 0;
    mediaDryMs_ = 0;
    mediaGaps_ = mediaTrims_ = 0;
    const PromptPath::Action prompt = path_.stop();
    pthread_cond_signal(&promptCv_);
    if (prompt == PromptPath::kRemove) {
        askPromptRoute(false);
    }
    releaseRoute();
    pthread_mutex_unlock(&mu_);
    logStats("session ended");
}

void EvoAudio::onData(uint8_t channel, const uint8_t* pcm, size_t len, uint64_t, void* user) {
    static_cast<EvoAudio*>(user)->data(channel, pcm, len);
}

void EvoAudio::onStream(uint8_t channel, bool started, uint32_t config, void* user) {
    static_cast<EvoAudio*>(user)->stream(channel, started, config);
}

void EvoAudio::logStats(const char* when) {
    const AudioMixer::Stats stats = this->stats();
    ELOG(kTag,
         "%s: media %lu ms in (%lu ms lost, %lu gap(s), at most %lu ms waiting), speech %lu ms in (%lu ms lost, %lu "
         "gap(s)), %lu ms played",
         when, stats.framesIn[AudioMixer::kMedia] / 48, stats.dropped[AudioMixer::kMedia] / 48,
         stats.underruns[AudioMixer::kMedia], stats.mostWaiting[AudioMixer::kMedia] / 48,
         stats.framesIn[AudioMixer::kSpeech] / 48, stats.dropped[AudioMixer::kSpeech] / 48,
         stats.underruns[AudioMixer::kSpeech], stats.framesOut / 48);
}

void* EvoAudio::threadStub(void* arg) {
    static_cast<EvoAudio*>(arg)->loop();
    return nullptr;
}

// The PCM's buffer filled with silence. An empty buffer takes the first sound all at once, without waiting: the queues
// run dry at once and the stream stops and starts. Full, the PCM plays, and every write after it waits for the car's
// own sound clock, which is what paces the mix.
bool EvoAudio::prime(AlsaOut& out, unsigned channels) {
    const int16_t silence[kPeriodFrames * kMixChannels] = {0};
    const size_t perWrite = kPeriodFrames * kMixChannels / channels;
    for (size_t left = out.bufferFrames(); left;) {
        const size_t frames = left < perWrite ? left : perWrite;
        if (!out.write(silence, frames)) {
            return false;
        }
        left -= frames;
    }
    return true;
}

void* EvoAudio::promptStub(void* arg) {
    static_cast<EvoAudio*>(arg)->promptLoop();
    return nullptr;
}

// The prompts that go their own way: held until the car answers for their route, then written to their PCM, which
// is opened at the prompt's own rate, mono. The route is given back kHoldMs after the last one.
void EvoAudio::promptLoop() {
    std::vector<int16_t> chunk;
    bool toDrain = false, primed = false;
    unsigned openRate = 0;
    uint32_t refusedAt = 0;
    bool refused = false;
    for (;;) {
        pthread_mutex_lock(&mu_);
        if (quit_) {
            pthread_mutex_unlock(&mu_);
            break;
        }
        const uint32_t now = monoMs();
        if (promptRoute_) {
            const int result = promptRoute_->load();
            if (result) {
                path_.answered(path_.ask(), result > 0, now);
                promptRoute_.reset();
                if (result < 0) {
                    ELOG(kTag, "the car refused prompt route or did not answer; reverting to the mix");
                    requestAudioRoute(false, settings_.promptRoute);
                }
            }
        }
        const PromptPath::Action action = path_.tick(now);
        if (action == PromptPath::kRemove) {
            askPromptRoute(false);
        }
        const PromptPath::Play play = promptAlt_ ? path_.play(now) : PromptPath::kWait;
        bool fellBack = false, finished = false, waited = false;
        const unsigned rate = promptRate_;
        chunk.clear();
        if (play == PromptPath::kFallBack) {
            // The car refused the route: what came goes into the mix, and the rest of the prompt follows it there.
            if (prompt_.size() > promptHead_) {
                mixer_.push(AudioMixer::kSpeech, &prompt_[promptHead_], prompt_.size() - promptHead_, now);
            }
            if (promptEnded_) {
                mixer_.end(AudioMixer::kSpeech);
            }
            prompt_.clear();
            promptHead_ = 0;
            promptAlt_ = false;
            promptPlaying_ = false;
            path_.done(now);
            pthread_cond_signal(&cv_);
            fellBack = true;
        } else if (play == PromptPath::kPlay) {
            const size_t have = prompt_.size() - promptHead_;
            const size_t prebuffer = static_cast<size_t>(rate) * kPromptPrebufferMs / 1000;
            promptPlaying_ = promptPlaying_ ||
                             (have && (have >= prebuffer || promptEnded_ || now - promptPushMs_ >= kPromptFlushMs));
            if (promptPlaying_ && have) {
                const size_t take = std::min(have, static_cast<size_t>(rate) / 100);  // 10 ms
                chunk.assign(prompt_.begin() + static_cast<std::ptrdiff_t>(promptHead_),
                             prompt_.begin() + static_cast<std::ptrdiff_t>(promptHead_ + take));
                for (size_t i = 0; i < chunk.size(); ++i) {  // the gain the mix applies (no sum here: no clipping)
                    chunk[i] = static_cast<int16_t>(chunk[i] * settings_.gainPercent / 100);
                }
                promptHead_ += take;
                if (promptHead_ * 2 >= prompt_.size()) {
                    prompt_.erase(prompt_.begin(), prompt_.begin() + static_cast<std::ptrdiff_t>(promptHead_));
                    promptHead_ = 0;
                }
            } else if (!have && promptEnded_) {
                promptAlt_ = false;
                promptPlaying_ = false;
                path_.done(now);
                finished = true;
            }
        }
        const bool idle = !promptAlt_ && !path_.routeOn() && !path_.routeAsked();
        if (chunk.empty() && !finished && !fellBack && action == PromptPath::kNothing) {
            // Waiting for sound, for the route, or for the route's hold to end.
            const long waitMs = promptAlt_ ? 10 : idle ? 1000 : 50;
            struct timespec until;
            clock_gettime(CLOCK_MONOTONIC, &until);
            until.tv_nsec += (waitMs % 1000) * 1000000L;
            until.tv_sec += waitMs / 1000 + until.tv_nsec / 1000000000L;
            until.tv_nsec %= 1000000000L;
            pthread_cond_timedwait(&promptCv_, &mu_, &until);
            waited = true;
        }
        pthread_mutex_unlock(&mu_);
        if (fellBack) {
            ELOG(kTag, "prompt sent back to the mix");
        }
        if (!chunk.empty()) {
            if (promptOut_.isOpen() && openRate != rate) {
                promptOut_.close();
            }
            if (!promptOut_.isOpen()) {
                if (refused && now - refusedAt < kRetryOpenAfterMs) {
                    continue;  // lost, as in the mix: the queue must not wait behind a PCM that does not open
                }
                if (!promptOut_.open(settings_.promptPcm.c_str(), rate, 1, kLatencyUs)) {
                    if (!refused) {
                        ELOG(kTag, "cannot open PCM '%s' (%s): prompts lost, tried again every %u ms",
                             settings_.promptPcm.c_str(), promptOut_.lastError().c_str(), kRetryOpenAfterMs);
                    }
                    refused = true;
                    refusedAt = now;
                    continue;
                }
                refused = false;
                primed = false;
                openRate = rate;
                ELOG(kTag, "PCM '%s' open: %u Hz mono", settings_.promptPcm.c_str(), rate);
            }
            const bool written = (primed || prime(promptOut_, 1)) && promptOut_.write(&chunk[0], chunk.size());
            primed = true;
            if (!written) {
                ELOG(kTag, "PCM '%s' lost (%s): closed", settings_.promptPcm.c_str(), promptOut_.lastError().c_str());
                promptOut_.close();
                toDrain = false;
                continue;
            }
            toDrain = true;
        }
        if (finished && toDrain) {
            promptOut_.drain();  // plays to its end
            toDrain = false;
            primed = false;
        }
        if (waited && idle && promptOut_.isOpen() && !toDrain) {
            promptOut_.close();  // the route is given back: the PCM too
        }
    }
    if (toDrain) {
        promptOut_.drain();
    }
    promptOut_.close();
}

AudioMixer::Stats EvoAudio::stats() {
    pthread_mutex_lock(&mu_);
    const AudioMixer::Stats stats = mixer_.stats();
    pthread_mutex_unlock(&mu_);
    return stats;
}

void EvoAudio::loop() {
    int16_t period[kPeriodFrames * kMixChannels];
    uint32_t wroteAt = 0;    // last write
    uint32_t refusedAt = 0;  // last open that failed
    bool refused = false;
    bool toDrain = false;    // written since the last drain
    bool primed = false;     // the PCM's buffer was filled since it last ran empty (open, drain, underrun)
    unsigned underruns = 0;  // the PCM's count when it was last primed
    for (;;) {
        pthread_mutex_lock(&mu_);
        bool have = false;
        size_t played = 0;                // media that left the queue in this turn
        unsigned gap = 0, back = 0;       // the music ran dry (its number in this session), or plays again
        uint32_t quietMs = 0, dryMs = 0;  // how long the phone had sent none, and how long nothing played
        if (!quit_) {
            const uint32_t mixAt = monoMs();
            const unsigned long gaps = mixer_.stats().underruns[AudioMixer::kMedia];
            have = mixer_.mix(period, kPeriodFrames, mixAt);
            updateMediaRoute(have, mixAt);
            played = mixer_.takePlayedBytes(AudioMixer::kMedia);
            if (mixer_.stats().underruns[AudioMixer::kMedia] != gaps) {
                gap = ++mediaGaps_;
                mediaDryMs_ = mixAt ? mixAt : 1;
                quietMs = mixAt - mixer_.lastPushMs(AudioMixer::kMedia);
            } else if (mediaDryMs_ && mixer_.playing(AudioMixer::kMedia)) {
                back = mediaGaps_;
                dryMs = mixAt - mediaDryMs_;
                mediaDryMs_ = 0;
            }
            if (!have) {
                // Queued but not playing yet: its prebuffer or its flush delay is close. Else wait for sound, waking
                // for the drain and the close of an open PCM.
                const long waitMs = mixer_.pending() ? 5 : toDrain ? 20 : (out_.isOpen() || mediaRoute_) ? 500 : 60000;
                struct timespec until;
                clock_gettime(CLOCK_MONOTONIC, &until);
                until.tv_nsec += (waitMs % 1000) * 1000000L;
                until.tv_sec += waitMs / 1000 + until.tv_nsec / 1000000000L;
                until.tv_nsec %= 1000000000L;
                pthread_cond_timedwait(&cv_, &mu_, &until);
            }
        }
        const bool quit = quit_;
        pthread_mutex_unlock(&mu_);
        if (quit) {
            break;
        }
        if (played && mediaPlayed_) {
            mediaPlayed_(played, mediaPlayedUser_);
        }
        // Each gap as it happens, with what tells a link that stalled (the phone sent nothing for a while) from a
        // phone that sends slower than the car plays (it sent a moment ago).
        if (gap && logged(gap)) {
            ELOG(kTag, "media ran dry: nothing from the phone for %u ms (gap %u of this session)", quietMs, gap);
        } else if (back && logged(back)) {
            ELOG(kTag, "media plays again after %u ms", dryMs);
        }
        const uint32_t now = monoMs();
        if (!have) {
            if (toDrain && now - wroteAt >= kDrainAfterMs) {
                out_.drain();
                toDrain = false;
                primed = false;
            }
            if (out_.isOpen() && !toDrain && now - wroteAt >= kCloseAfterMs) {
                out_.close();
                ELOG(kTag, "PCM '%s' closed after %u ms of silence", settings_.pcm.c_str(), kCloseAfterMs);
            }
            continue;
        }
        if (!out_.isOpen()) {
            // The car connects the PCM to the speakers once asked, which takes it a few seconds: the opens below are
            // tried again meanwhile (another source may hold the PCM until then).
            if (refused && now - refusedAt < kRetryOpenAfterMs) {
                continue;  // this period is lost: the queue must not fill up behind a PCM that does not open
            }
            if (!out_.open(settings_.pcm.c_str(), kMixRate, kMixChannels, kLatencyUs)) {
                if (!refused) {
                    ELOG(kTag, "cannot open PCM '%s' (%s): no sound, tried again every %u ms", settings_.pcm.c_str(),
                         out_.lastError().c_str(), kRetryOpenAfterMs);
                }
                refused = true;
                refusedAt = now;
                continue;
            }
            refused = false;
            primed = false;
            ELOG(kTag, "PCM '%s' open: 48 kHz stereo, buffer %u ms", settings_.pcm.c_str(),
                 static_cast<unsigned>(out_.bufferFrames() * 1000 / kMixRate));
        }
        bool written = true;
        if (!primed) {
            underruns = out_.underruns();
            written = prime(out_, kMixChannels);
            primed = true;
        }
        if (!written || !out_.write(period, kPeriodFrames)) {
            ELOG(kTag, "PCM '%s' lost (%s): closed, opened again at the next sound", settings_.pcm.c_str(),
                 out_.lastError().c_str());
            out_.close();
            refused = true;
            refusedAt = now;
            toDrain = false;
            continue;
        }
        wroteAt = monoMs();
        toDrain = true;
        if (out_.underruns() != underruns) {
            primed = false;  // the PCM ran empty: filled again before the next sound
        }
    }
    if (toDrain) {
        out_.drain();
    }
    out_.close();
}

}  // namespace evo
