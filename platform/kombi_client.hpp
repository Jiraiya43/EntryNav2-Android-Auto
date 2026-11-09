// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// TCP client of the cluster's route-guidance service: graph updates, the cancel, the cancel from a fatal signal.

// - The updates share one connection. A connection error (end of stream, reset, timeout) gets one reconnection and the
//   request again: updates can be repeated. An answer that isn't an acknowledgement is never retried.
// - Magic cookies that came in while idle are read away before each request, and up to four are skipped while waiting
//   for an answer.
// - The cancel always opens its own connection, then falls back to the guidance-state-0 requests alone.
// - Only allowed methods leave (isAllowedMethod): never create or dispose.
// - emergencyCancel() is async-signal-safe: system calls only, on bytes armed beforehand.
#ifndef ENTRY_AAD_PLATFORM_KOMBI_CLIENT_HPP
#define ENTRY_AAD_PLATFORM_KOMBI_CLIENT_HPP

#include <stdint.h>

#include <string>
#include <vector>

#include "kombi_graph.hpp"
#include "kombi_someip.hpp"

namespace evo {
namespace kombi {

enum SendOutcome { kSent = 0, kLinkDown = 1, kRefused = 2 };

class ClusterLink {
public:
    static const int kTimeoutMs = 3000;  // to connect, and to wait for each answer

    ClusterLink(const std::string& address, uint16_t port, int timeoutMs = kTimeoutMs);
    ~ClusterLink();

    // The requests in order, on the shared connection. why: a short reason when it fails.
    SendOutcome send(const std::vector<Request>& requests, std::string* why = nullptr);

    // The factory cancel on a new connection, else its guidance-state-0 requests. When both fail, again every pauseS
    // until retryForS has passed or giveUp(user) says so. True: the cluster acknowledged it.
    using GiveUp = bool (*)(void* user);
    bool cancel(const RouteGraph& graph, double retryForS = 0.0, double pauseS = 5.0, GiveUp giveUp = nullptr,
                void* user = nullptr, std::string* why = nullptr);

    void disconnect();
    void setTimeoutMs(int ms) { timeoutMs_ = ms; }

    // Writes the bytes emergencyCancel() sends; once the graph is known, never at the same time as itself.
    void armEmergencyCancel(const RouteGraph& graph);
    // For a fatal-signal handler: the cancel once, 1 s to connect and per answer. Nothing before it is armed.
    static void emergencyCancel();

private:
    int open() const;
    bool exchange(int fd, const Request& request, uint16_t session, std::string* why, bool* refused) const;
    bool readQueued(std::string* why);
    bool sendAll(const std::vector<Request>& requests, uint16_t firstSession, std::string* why) const;

    std::string address_;
    uint16_t port_;
    int timeoutMs_;
    int fd_;            // the shared connection, -1 when closed
    uint16_t session_;  // the last session number used on it, 1 to 0xFFFE
};

}  // namespace kombi
}  // namespace evo

#endif
