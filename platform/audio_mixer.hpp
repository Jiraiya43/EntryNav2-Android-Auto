// Copyright (c) 2026 CallMeLewis
// The phone's two sound streams mixed into the one the car plays: 48 kHz stereo S16.
//
// Media (channel 9) comes as 48 kHz stereo, speech and guidance (channel 8) as 16 kHz mono. Each is converted as it
// arrives and waits in its own queue. A stream starts playing once enough waits (so the network's jitter does not
// cut it), or at once when the phone ended it or sent nothing more for a moment (a short prompt). Media is lowered
// while speech plays, and comes back after it, both in a ramp. A queue that grows too long (the phone's clock runs
// faster than the car's, or the car's output stalls) loses its oldest sound rather than fall ever further behind.
//
// The phone sends media only as fast as it is played here (core/played_acks.hpp, takePlayedBytes): about ten of its
// messages wait, 0.4 s of music, where 0.1 s waited before. Media's queue limit is above that level and is reached
// only after the output stalled. Speech is sent on the phone's own clock, as before.
//
// Pure: no clock, no thread, no sound library. The caller gives the time and holds its own lock.
// tests/test_audio_mixer.cpp.
#ifndef AA_ENTRYNAV_PLATFORM_AUDIO_MIXER_HPP
#define AA_ENTRYNAV_PLATFORM_AUDIO_MIXER_HPP

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace evo {

const unsigned kMixRate = 48000;  // what goes to the car
const unsigned kMixChannels = 2;

class AudioMixer {
public:
    enum Stream { kMedia = 0, kSpeech = 1, kStreams = 2 };

    enum : unsigned {
        kMediaPrebufferMs = 100,
        kSpeechPrebufferMs = 60,
        kFlushMs = 80,            // nothing more received this long: what waits plays
        kMaxQueueMs = 400,        // speech: more than this waiting: the oldest goes...
        kKeepQueueMs = 150,       // ...down to this
        kMediaMaxQueueMs = 1200,  // media, whose usual level is the phone's window of unacked messages
        kMediaKeepQueueMs = 500,
        kDuckRampMs = 120,
        kDuckHoldMs = 400  // media stays lowered this long after the last speech
    };

    struct Stats {
        unsigned long framesIn[kStreams];     // after conversion
        unsigned long dropped[kStreams];      // lost to a queue too long
        unsigned long underruns[kStreams];    // ran dry while the phone had not ended it
        unsigned long mostWaiting[kStreams];  // the longest its queue has been, in frames
        unsigned long framesOut;
    };

    // gainPercent: the whole mix, 0 to 100. duckPercent: what is left of media under speech, 0 to 100.
    explicit AudioMixer(int gainPercent = 100, int duckPercent = 30)
        : gainQ15_(percentQ15(gainPercent)), duckQ15_(percentQ15(duckPercent)), duckNowQ15_(kOne), duckHold_(0) {
        for (int s = 0; s < kStreams; ++s) {
            in_[s] = Input();
            stats_.framesIn[s] = stats_.dropped[s] = stats_.underruns[s] = stats_.mostWaiting[s] = 0;
        }
        stats_.framesOut = 0;
    }

    // The format of a stream's PCM: mono or stereo at 16, 24 or 48 kHz. False for anything else: the stream stays
    // silent rather than play at the wrong speed.
    bool configure(Stream s, unsigned rate, unsigned channels) {
        Input& in = in_[s];
        const bool ok = (channels == 1 || channels == 2) && (rate == 16000 || rate == 24000 || rate == 48000);
        in.rate = ok ? rate : 0;
        in.channels = ok ? channels : 0;
        in.hasLast = false;
        in.ended = false;
        return ok;
    }

    // PCM as the phone sent it: `count` samples (not frames). Half a frame at the end is dropped. Returns the samples
    // taken, which takePlayedBytes() will account for: 0 for a stream that is not configured.
    size_t push(Stream s, const int16_t* samples, size_t count, uint32_t nowMs) {
        Input& in = in_[s];
        if (!in.rate || !count) {
            return 0;
        }
        const unsigned factor = kMixRate / in.rate;
        const size_t frames = count / in.channels;
        in.queue.reserve(in.queue.size() + frames * factor * kMixChannels);
        for (size_t i = 0; i < frames; ++i) {
            const int16_t left = samples[i * in.channels];
            const int16_t right = in.channels == 2 ? samples[i * 2 + 1] : left;
            if (!in.hasLast) {
                in.lastLeft = left;
                in.lastRight = right;
                in.hasLast = true;
            }
            // Straight lines between the phone's samples: enough for speech, and 48 kHz goes through untouched.
            for (unsigned k = 1; k <= factor; ++k) {
                in.queue.push_back(between(in.lastLeft, left, k, factor));
                in.queue.push_back(between(in.lastRight, right, k, factor));
            }
            in.lastLeft = left;
            in.lastRight = right;
        }
        stats_.framesIn[s] += frames * factor;
        in.lastPushMs = nowMs;
        in.ended = false;
        const size_t have = queued(in);
        if (have > stats_.mostWaiting[s]) {
            stats_.mostWaiting[s] = have;
        }
        if (have > framesOf(s == kMedia ? kMediaMaxQueueMs : kMaxQueueMs)) {
            const size_t drop = have - framesOf(s == kMedia ? kMediaKeepQueueMs : kKeepQueueMs);
            in.head += drop * kMixChannels;
            in.gone += drop;
            stats_.dropped[s] += drop;
        }
        compact(in);
        return frames * in.channels;
    }

    // How much of what push() took has left the queue since the last call, played or lost to a queue too long, in
    // bytes of the phone's PCM. Whoever acks the phone's sound once it is played asks after every push() and mix().
    size_t takePlayedBytes(Stream s) {
        Input& in = in_[s];
        if (!in.rate) {
            in.gone = 0;
            return 0;
        }
        const unsigned factor = kMixRate / in.rate;
        const size_t frames = in.gone / factor;  // the phone's frames; part of one waits for the next call
        in.gone -= frames * factor;
        return frames * in.channels * sizeof(int16_t);
    }

    // The phone stopped the stream: what waits plays out without waiting for more.
    void end(Stream s) { in_[s].ended = true; }

    // The session ended: nothing waits, nothing plays.
    void clear() {
        for (int s = 0; s < kStreams; ++s) {
            in_[s].queue.clear();
            in_[s].head = 0;
            in_[s].playing = false;
            in_[s].ended = false;
            in_[s].hasLast = false;
            in_[s].gone = 0;  // nobody is left to tell
        }
        duckNowQ15_ = kOne;
        duckHold_ = 0;
    }

    bool pending() const { return queued(in_[kMedia]) || queued(in_[kSpeech]); }
    size_t waiting(Stream s) const { return queued(in_[s]); }  // frames
    bool playing(Stream s) const { return in_[s].playing; }
    uint32_t lastPushMs(Stream s) const { return in_[s].lastPushMs; }  // the `nowMs` of its last push()
    const Stats& stats() const { return stats_; }

    // The next `frames` frames into `out` (2 * frames samples), silence where a stream has nothing. False, and `out`
    // untouched, when no stream plays: the caller writes nothing and asks again a little later.
    bool mix(int16_t* out, size_t frames, uint32_t nowMs) {
        bool any = false;
        for (int s = 0; s < kStreams; ++s) {
            Input& in = in_[s];
            const size_t have = queued(in);
            if (!in.playing && have) {
                const unsigned prebufferMs = s == kMedia ? kMediaPrebufferMs : kSpeechPrebufferMs;
                in.playing = have >= framesOf(prebufferMs) || in.ended || nowMs - in.lastPushMs >= kFlushMs;
            }
            any = any || in.playing;
        }
        if (!any) {
            return false;
        }
        Input& media = in_[kMedia];
        Input& speech = in_[kSpeech];
        const int32_t rampStep = (kOne - duckQ15_) / static_cast<int32_t>(framesOf(kDuckRampMs)) + 1;
        for (size_t i = 0; i < frames; ++i) {
            const bool speaking = speech.playing && queued(speech);
            if (speaking) {
                duckHold_ = framesOf(kDuckHoldMs);
            } else if (duckHold_) {
                --duckHold_;
            }
            const int32_t target = duckHold_ ? duckQ15_ : static_cast<int32_t>(kOne);
            if (duckNowQ15_ > target) {
                duckNowQ15_ = duckNowQ15_ - rampStep < target ? target : duckNowQ15_ - rampStep;
            } else if (duckNowQ15_ < target) {
                duckNowQ15_ = duckNowQ15_ + rampStep > target ? target : duckNowQ15_ + rampStep;
            }
            int32_t left = 0, right = 0;
            if (media.playing && queued(media)) {
                left = (media.queue[media.head] * duckNowQ15_) >> 15;
                right = (media.queue[media.head + 1] * duckNowQ15_) >> 15;
                media.head += kMixChannels;
                ++media.gone;
            }
            if (speaking) {
                left += speech.queue[speech.head];
                right += speech.queue[speech.head + 1];
                speech.head += kMixChannels;
                ++speech.gone;
            }
            out[i * 2] = clamp((left * gainQ15_) >> 15);
            out[i * 2 + 1] = clamp((right * gainQ15_) >> 15);
        }
        stats_.framesOut += frames;
        for (int s = 0; s < kStreams; ++s) {
            Input& in = in_[s];
            if (in.playing && !queued(in)) {
                in.playing = false;
                if (!in.ended) {
                    ++stats_.underruns[s];
                }
                in.ended = false;
            }
            compact(in);
        }
        return true;
    }

private:
    enum : int32_t { kOne = 32768 };

    struct Input {
        unsigned rate, channels;     // 0: not configured
        std::vector<int16_t> queue;  // 48 kHz stereo, read from head
        size_t head;
        size_t gone;  // frames that left the queue and takePlayedBytes() has not reported
        bool playing, ended, hasLast;
        uint32_t lastPushMs;
        int16_t lastLeft, lastRight;
        Input()
            : rate(0),
              channels(0),
              head(0),
              gone(0),
              playing(false),
              ended(false),
              hasLast(false),
              lastPushMs(0),
              lastLeft(0),
              lastRight(0) {}
    };

    static int32_t percentQ15(int percent) { return (percent < 0 ? 0 : percent > 100 ? 100 : percent) * kOne / 100; }
    static size_t framesOf(unsigned ms) { return static_cast<size_t>(kMixRate) * ms / 1000; }
    static size_t queued(const Input& in) { return (in.queue.size() - in.head) / kMixChannels; }
    static int16_t between(int16_t from, int16_t to, unsigned k, unsigned n) {
        return static_cast<int16_t>(from + (static_cast<int32_t>(to) - from) * static_cast<int32_t>(k) /
                                               static_cast<int32_t>(n));
    }
    static int16_t clamp(int32_t v) { return static_cast<int16_t>(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
    // The played part of a queue is given back once it is the larger part.
    static void compact(Input& in) {
        if (in.head && in.head * 2 >= in.queue.size()) {
            in.queue.erase(in.queue.begin(), in.queue.begin() + static_cast<std::ptrdiff_t>(in.head));
            in.head = 0;
        }
    }

    Input in_[kStreams];
    int32_t gainQ15_, duckQ15_;
    int32_t duckNowQ15_;  // media's gain right now, between duckQ15_ and kOne
    size_t duckHold_;     // frames media stays lowered after the last speech frame
    Stats stats_;
};

}  // namespace evo

#endif  // AA_ENTRYNAV_PLATFORM_AUDIO_MIXER_HPP
