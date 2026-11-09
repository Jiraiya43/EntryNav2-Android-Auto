// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Control channel between the iDrive app (PluginApp EntryAad, in HmiMain3D) and the Android Auto service.

// Protocol: UDP on 127.0.0.1 (evo::contract::kControlPort in the service), one text command per datagram:
//   ping (answered "pong"), show, hide, quit, gear <n>, daynight <n>, call <0|1>, popup <0|1>, drivemode [<n>],
//   pdc <n> (accepted, not used by the service).
// State file (evo::contract::kStateFile), read every second by the app: "<connecting|connected> <pid> <overlay>",
// replaced atomically and deleted on stop. overlay 1 = the Android Auto picture is on screen (inputs go to Android
// Auto).
#ifndef ENTRY_AAD_PLATFORM_EVO_CONTROL_HPP
#define ENTRY_AAD_PLATFORM_EVO_CONTROL_HPP

#include <pthread.h>
#include <stdint.h>

#include <atomic>
#include <string>

namespace evo {

struct ControlCommand {
    std::string verb;
    int arg;
    bool hasArg;
};
// Pure parser. False for an unknown or malformed command.
bool parseControlCommand(const char* data, size_t len, ControlCommand& out);

class EvoControl {
public:
    using Callback = void (*)(const ControlCommand&, void*);
    EvoControl();
    ~EvoControl();
    // Binds 127.0.0.1:port. False when the port is taken (another service is running).
    bool start(uint16_t port, const char* stateFile, Callback cb, void* user);
    void stop();
    // Writes only when the content changes, or every 5 s to refresh the file's timestamp.
    void writeState(const char* state, int overlay);

private:
    static void* threadMain(void* self);
    void run();
    int fd_;
    pthread_t thread_;
    bool started_;
    std::atomic<bool> running_;
    std::string stateFile_, last_;
    uint32_t lastWriteMs_;
    Callback cb_;
    void* user_;
};

}  // namespace evo

#endif
