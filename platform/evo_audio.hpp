// Copyright (c) 2026 CallMeLewis
// Sound over Wi-Fi: the phone's media (AA channel 9) and its speech and guidance (channel 8), played on the car.
//
// Off unless the settings file exists (service_contract.hpp, kAudioWifi): without it nothing here runs, the phone is
// told about no media sink and its sound stays on the car's Bluetooth, as before. With it, both streams are mixed
// (platform/audio_mixer.hpp) and written to one playback PCM of the car (platform/evo_alsa_out.hpp), opened at the
// first sound and closed after a few seconds of silence. Before the first sound, the car's audio manager is asked to
// connect the PCM to the speakers (platform/audio_route.hpp), and the route is given back after five seconds without mixed sound or when the session ends.
// While the phone's music isn't playing on the car, a prompt plays on a PCM of its own under a route of its own, so the
// car lowers the radio instead of giving its source to the phone (platform/prompt_path.hpp).
//
// The settings file, one setting per line, each optional:
//   pcm=<name>   the playback PCM, a name of the car's /etc/asound.conf ("entertainment" when absent)
//   route=<hex>  the route asked of the car's audio manager, 0 for none (0x315 with "entertainment", else none)
//   gain=<0-100> the whole mix, in percent (100 when absent)
//   duck=<0-100> what is left of media while a voice prompt plays, in percent (30 when absent)
//   prompt-pcm=<name>   the prompts' own PCM ("carplay_alt" when absent)
//   prompt-route=<hex>  their route, 0 to keep every prompt in the mix (0x31A, or 0 when pcm or route is changed)
#ifndef AA_ENTRYNAV_PLATFORM_EVO_AUDIO_HPP
#define AA_ENTRYNAV_PLATFORM_EVO_AUDIO_HPP

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "audio_mixer.hpp"
#include "evo_alsa_out.hpp"
#include "prompt_path.hpp"

namespace evo {

struct AudioSettings {
    std::string pcm;
    int gainPercent;
    int duckPercent;
    uint16_t route;  // asked of the car's audio manager before the first sound, 0 for none
    std::string promptPcm;
    uint16_t promptRoute;  // 0: prompts stay in the mix
    AudioSettings()
        : pcm("entertainment"),
          gainPercent(100),
          duckPercent(30),
          route(0x315),
          promptPcm("carplay_alt"),
          promptRoute(0x31A) {}
};

// A PCM name as accepted from the file: 1 to 63 of A-Z a-z 0-9 _ - . : , =
bool validPcmName(const std::string& name);
// False when there is no file: sound stays on Bluetooth. An invalid line keeps that setting's default.
bool loadAudioSettings(const char* path, AudioSettings& settings);

class EvoAudio {
public:
    EvoAudio();
    ~EvoAudio();

    // Told of every byte of media handed to data() once it is played, or will never be: the session acks the phone's
    // music by it (AASession::setAckMediaWhenPlayed), so the phone sends as fast as the car plays. Called from the
    // session thread and the playback thread, with no lock held. Set before start().
    using MediaPlayed = void (*)(size_t bytes, void* user);
    void setMediaPlayed(MediaPlayed fn, void* user) {
        mediaPlayed_ = fn;
        mediaPlayedUser_ = user;
    }

    // False when the car's libasound cannot be loaded or the thread cannot start: sound must stay on Bluetooth.
    bool start(const AudioSettings& settings);
    void stop();

    // From the session thread; none blocks longer than a copy of the sound.
    // A stream starts (config: the index in the channel's list of configurations) or stops.
    void stream(uint8_t channel, bool started, uint32_t config);
    void data(uint8_t channel, const uint8_t* pcm, size_t len);
    void sessionEnded();
    AudioMixer::Stats stats();  // what came in, what was lost, what was played
    // Our media route is asked of the car for this session: iDrive then takes CarPlay for the playing source.
    bool holdsRoute();

    // The session's callbacks (core/aa_session.hpp), user = the EvoAudio.
    static void onData(uint8_t channel, const uint8_t* pcm, size_t len, uint64_t pts, void* user);
    static void onStream(uint8_t channel, bool started, uint32_t config, void* user);

private:
    static void* threadStub(void* arg);
    static void* promptStub(void* arg);
    void loop();
    void promptLoop();
    static bool prime(AlsaOut& out, unsigned channels);
    void logStats(const char* when);
    bool musicPlaying(uint32_t nowMs) const;  // under mu_
    void startPrompt(unsigned rate, uint32_t nowMs);
    void askPromptRoute(bool build);  // under mu_

    AudioSettings settings_;
    AlsaOut out_;  // the playback thread only, once started
    pthread_t thread_;
    pthread_mutex_t mu_;
    pthread_cond_t cv_;
    bool started_, quit_;
    AudioMixer mixer_;  // under mu_
    // A separate result per build keeps late answers from earlier sessions out of this one.
    std::shared_ptr<std::atomic<int> > mediaRoute_, promptRoute_;
    uint32_t lastMixMs_, mediaRefusedMs_;
    bool mediaBackoff_;
    MediaPlayed mediaPlayed_;
    void* mediaPlayedUser_;
    // Under mu_: the music ran dry while the phone had not stopped it, at that time (0: it plays or is stopped), and
    // how many times in this session.
    uint32_t mediaDryMs_;
    unsigned mediaGaps_, mediaTrims_;
    void releaseRoute(bool restore = true);  // under mu_; restore: the source our route replaced plays again
    void updateMediaRoute(bool have, uint32_t now);  // under mu_

    // The prompts' own path: under mu_ but promptOut_, which only the prompt thread uses.
    AlsaOut promptOut_;
    pthread_t promptThread_;
    bool promptThreadRunning_;
    pthread_cond_t promptCv_;
    PromptPath path_;
    std::vector<int16_t> prompt_;  // the current prompt as the phone sends it (mono), read from promptHead_
    size_t promptHead_;
    unsigned promptRate_;
    bool promptAlt_;      // the current prompt goes this way, not into the mix
    bool promptEnded_;    // the phone stopped its stream
    bool promptPlaying_;  // its sound has started going to the PCM
    uint32_t promptPushMs_;
    bool musicOpen_;         // the phone's media stream is started
    bool mediaStopped_;      // media STOP since last start, even if the PCM is still open
    uint32_t musicSoundMs_;  // its last sound, 0 for none yet
};

}  // namespace evo

#endif  // AA_ENTRYNAV_PLATFORM_EVO_AUDIO_HPP
