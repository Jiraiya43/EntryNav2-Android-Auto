// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The car's touchscreen (UDP 30567) to Android Auto touch events.

// Observed on the car: HmiMain3D (CTouchDataClient) listens on 0.0.0.0:30567; there is no evdev touch device.
// Reading: a read-only AF_PACKET socket with a BPF filter on UDP 30567 copies each datagram, taking nothing away from
// iDrive and never sending. Enough to observe and decode.
// Forward mode: only while the Android Auto picture is on screen, a UDP 30567 socket (SO_REUSEADDR/SO_REUSEPORT) is
// connected to the touch sender. The kernel prefers a connected socket (exact match), so the hidden iDrive stops
// receiving the touches. The socket is closed as soon as the picture goes away, or on stop, and iDrive gets the
// touchscreen back. When the bind is refused, only the copy is used.
#ifndef ENTRY_AAD_PLATFORM_EVO_TOUCH_HPP
#define ENTRY_AAD_PLATFORM_EVO_TOUCH_HPP

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include <atomic>

#include "aa_input.hpp"
#include "evo_touch_frame.hpp"
#include "input_mode.hpp"

namespace evo {

// IPv4 packet starting at the network header (AF_PACKET SOCK_DGRAM). Fragments are refused.
struct UdpView {
    uint32_t src;
    uint16_t sport;
    uint16_t dport;
    const uint8_t* data;
    size_t len;
};
bool parseIpv4Udp(const uint8_t* p, size_t n, UdpView& out);
bool isTouchPdu(const uint8_t* b, size_t n);

// Touch state machine, no I/O.
class TouchTracker {
public:
    enum { NS = aa::kTouchMaxContacts };
    TouchTracker(double sx = 1.0, double sy = 1.0, double ox = 0.0, double oy = 0.0, int maxX = 1279, int maxY = 719);
    // One UDP payload. Returns the number of events written, -1 when it is not a touch frame.
    int feed(const uint8_t* buf, size_t n, aa::TouchEventOut* out, int max);
    // Releases the fingers still down (lost "up" frame, picture taken away).
    int releaseAll(aa::TouchEventOut* out, int max);
    int activeCount() const;
    unsigned messages() const { return messages_; }
    int lastContacts(aa::TouchContact* out, int max) const;
    void reset();
    // False until the Android Auto picture has started (true by default).
    void setPictureStarted(bool started) { pictureStarted_ = started; }
    // The last pixel of the phone's canvas: no coordinate is sent past it.
    void setLimits(int maxX, int maxY) {
        maxX_ = maxX;
        maxY_ = maxY;
    }

private:
    void fillActive(aa::TouchEventOut& e, int which) const;
    void emitUp(aa::TouchEventOut& e, int k, int x, int y);
    double sx_, sy_, ox_, oy_;
    int maxX_, maxY_;
    bool active_[NS], needDown_[NS];
    bool blocked_[NS];  // this finger touched down without a picture: ignored until it lifts
    bool pictureStarted_;
    int cx_[NS], cy_[NS];
    aa::TouchReassembler reasm_;
    bool haveMsg_;
    unsigned char prevSeq0_, prevSeq1_;
    unsigned messages_;
    aa::TouchContact last_[NS];
    int lastN_;
};

class EvoTouch {
public:
    using Callback = void (*)(const aa::TouchEventOut&, void*);
    EvoTouch();
    ~EvoTouch();
    // Before start(): the size of the phone's canvas, 1280x720 by default.
    void setCanvas(uint32_t width, uint32_t height) {
        tracker_.setLimits(static_cast<int>(width) - 1, static_cast<int>(height) - 1);
    }
    bool start(InputMode mode, Callback cb, void* user);
    // Forward mode only: true while the Android Auto picture is on screen and the session is up.
    void setOwned(bool owned);
    // Main loop: whether the Android Auto picture has started. A gesture begun before stays ignored until it lifts.
    void setPictureStarted(bool started) { pictureStarted_ = started; }
    void stop();
    void summary() const;  // one summary log line
private:
    static void* threadMain(void* self);
    void run();
    bool openTap();
    void updateSteal(bool want);
    void handle(const UdpView& u, uint32_t now);
    void emit(const aa::TouchEventOut* ev, int n);
    InputMode mode_;
    Callback cb_;
    void* user_;
    pthread_t thread_;
    bool started_;
    std::atomic<bool> running_, owned_, pictureStarted_;
    int tap_, steal_, ifindex_;
    uint32_t peerIp_;
    uint16_t peerPort_;
    bool havePeer_, stealRefused_;
    uint32_t lastFrameMs_;
    TouchTracker tracker_;
    unsigned tapped_, touchPdus_, otherPdus_, events_, sent_, stolen_, rawLogged_, eventsLogged_;
    bool fingerSent_;  // a finger went down on the phone and has not been lifted there yet (touch thread)
};

}  // namespace evo

#endif
