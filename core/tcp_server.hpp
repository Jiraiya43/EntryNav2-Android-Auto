// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// TCP listener that waits for the phone's Android Auto connection on the car's hotspot.
#ifndef ENTRY_AAD_CORE_TCP_SERVER_HPP
#define ENTRY_AAD_CORE_TCP_SERVER_HPP

#include <pthread.h>
#include <stdint.h>

#include <atomic>
#include <string>

namespace aa {

// The accepted socket, handed over: the callback owns it from then on. Called on the listener's thread.
using ClientConnectedCallback = void (*)(int, void*);

// One phone at a time: a connection is turned away until the session it was handed to has ended. A listening socket
// that keeps failing (the car's Wi-Fi switched off under it) is made again.
class TcpServer {
public:
    explicit TcpServer(uint16_t port);
    ~TcpServer();

    bool start();  // true when already listening
    void stop();   // joins the listener's thread, within a second

    // Set before start().
    void setConnectionCallback(ClientConnectedCallback callback, void* user_data = nullptr);
    // The car's hotspot address: listened on when it is up, and the /24 a phone must come from. Empty: every
    // interface, anyone.
    void setHotspotAddress(const std::string& ip) { hotspot_ = ip; }

    void notifySessionEnded() { busy_ = false; }  // the next phone can come in
    void markSessionActive() { busy_ = true; }    // again, once the session started

private:
    static void* threadMain(void* self);
    void run();
    int bindListener();  // a listening socket, or -1
    void relisten(const char* why);
    bool fromHotspot(uint32_t address) const;  // network order

    uint16_t port_;
    std::string hotspot_;
    ClientConnectedCallback callback_;
    void* user_;
    pthread_t thread_;
    std::atomic<bool> running_;
    std::atomic<bool> busy_;
    // Listener thread only, while running.
    int listener_;
    unsigned failures_;  // select() or accept() failures in a row
    int lastError_;
    unsigned accepted_;
};

}  // namespace aa

#endif  // ENTRY_AAD_CORE_TCP_SERVER_HPP
