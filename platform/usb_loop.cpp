// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The USB phone loop (see usb_loop.hpp).
#include "usb_loop.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <map>
#include <set>
#include <vector>

#include "evo_log.hpp"
#include "usb_accessory.hpp"
#include "usb_bridge.hpp"

namespace evo {
namespace {

const unsigned kMaxResets = 3;        // per phone, until a session held or the phone is plugged in again
const uint32_t kHeldMs = 30000;       // a session that lasted this long worked
const unsigned kMaxSilentTries = 24;  // sessions in a row the phone said nothing in (5 s each) before a reset
const unsigned kMaxClaimTries = 10;   // turns in a row its interface was held or not ready
const unsigned kMaxAskFailures = 3;   // switch requests that failed half-way
const unsigned kMaxUnanswered = 5;    // per phone: switch requests taken, with no accessory back
const unsigned kMaxProbeRetries = 3;  // retries after a temporary failure of the initial AOA request
const uint32_t kUnpluggedMs = 6000;   // no phone on the bus for this long: it was unplugged
const unsigned kReturnStepUs = 20000;

class Loop {
public:
    Loop(const UsbLoopSetup& setup, UsbHost& host)
        : setup_(setup),
          host_(host),
          bridge_(nullptr),
          bridgeStart_(0),
          quietSince_(0),
          quiet_(false),
          emptySince_(0),
          empty_(false) {}

    void run() {
        ELOG("UsbLoop", "watching %s%s for an Android phone", setup_.port.empty() ? "every USB port" : "USB port ",
             setup_.port.c_str());
        bool atOnce = false;
        while (!host_.stopRequested()) {
            if (!atOnce) {
                rest(setup_.turnMs);
            }
            atOnce = turn();
        }
        leave();
    }

private:
    struct ProbeRetry {
        unsigned address, failures;
        ProbeRetry() : address(0), failures(0) {}
    };

    // Sleeps, but not past a stop request.
    void rest(unsigned ms) {
        for (unsigned waited = 0; waited < ms && !host_.stopRequested(); waited += 50) {
            usleep(50000);
        }
    }

    void forgetProbeRetries() {
        for (std::map<std::string, ProbeRetry>::iterator it = probeRetries_.begin(); it != probeRetries_.end();) {
            UsbPhone now;
            if (!readUsbDevice(setup_.sysfs, it->first, now) || now.address != it->second.address) {
                probeRetries_.erase(it++);
            } else {
                ++it;
            }
        }
    }

    // One look at the bus. True when the next one must follow at once.
    bool turn() {
        if (bridge_) {
            if (bridge_->finished()) {
                bridgeEnded();
            }
            return false;
        }
        if (host_.projectionBusy() || (quiet_ && monoMs() - quietSince_ < setup_.afterResetMs)) {
            return false;
        }
        quiet_ = false;
        std::vector<UsbPhone> phones;
        listUsbPhones(setup_.sysfs, setup_.port, phones);
        forgetGone();
        forgetProbeRetries();
        for (size_t i = 0; i < phones.size(); ++i) {
            if (!leftAlone(phones[i])) {
                noteEmpty(false);
                return phones[i].accessory ? useAccessory(phones[i]) : askPhone(phones[i]);
            }
        }
        noteEmpty(true);
        return false;
    }

    bool leftAlone(const UsbPhone& phone) const {
        const std::map<std::string, unsigned>::const_iterator it = leftAlone_.find(phone.port);
        return it != leftAlone_.end() && it->second == phone.address;
    }

    void leaveAlone(const UsbPhone& phone) { leftAlone_[phone.port] = phone.address; }

    // No phone for longer than a phone takes to come back after a switch or a reset: it was unplugged, and the
    // next one starts with every try allowed.
    void noteEmpty(bool empty) {
        if (!empty) {
            empty_ = false;
        } else if (!empty_) {
            empty_ = true;
            emptySince_ = monoMs();
        } else if (monoMs() - emptySince_ >= kUnpluggedMs) {
            ours_.clear();
            unanswered_.clear();
            counts_.clear();
        }
    }

    // A device that was unplugged, or came back as another one, may be looked at again.
    void forgetGone() {
        for (std::map<std::string, unsigned>::iterator it = leftAlone_.begin(); it != leftAlone_.end();) {
            UsbPhone now;
            if (!readUsbDevice(setup_.sysfs, it->first, now) || now.address != it->second) {
                counts_.erase(it->first);
                leftAlone_.erase(it++);
            } else {
                ++it;
            }
        }
    }

    void bridgeEnded() {
        const uint32_t lasted = monoMs() - bridgeStart_;
        const bool spoke = bridge_->bytesFromPhone() > 0;
        const bool left = bridge_->phoneLeft();
        delete bridge_;
        bridge_ = nullptr;
        ELOG("UsbLoop", "USB session over after %u s (%s)", lasted / 1000,
             left    ? "the phone left"
             : spoke ? "session ended"
                     : "the phone said nothing");
        PortCounts& c = counts_[bridgePort_];
        if (lasted >= kHeldMs) {
            c.resets = 0;
        }
        if (spoke) {
            c.silent = 0;
            ours_.erase(bridgePort_);  // its Android Auto is over: only a fresh accessory starts a new one
        } else if (++c.silent >= kMaxSilentTries) {
            c.silent = 0;
            ours_.erase(bridgePort_);
        }
    }

    // An accessory on the bus: used when this loop just made it, else reset so that the phone can be asked again.
    bool useAccessory(const UsbPhone& phone) {
        if (!ours_.count(phone.port)) {
            return resetPhone(phone);
        }
        AccessoryEndpoints endpoints;
        UsbDevice* device = nullptr;
        const bool described = readAccessoryEndpoints(setup_.sysfs, phone.port, endpoints);
        if (described) {
            device = setup_.open(phone.bus, phone.address);
        }
        if (!device || !device->claimInterface(0)) {
            const int why = described ? errno : 0;
            delete device;
            PortCounts& c = counts_[phone.port];
            if (++c.claimTries >= kMaxClaimTries) {
                c.claimTries = 0;
                // Held by another program (EBUSY): most likely the car's own USB service, which also opens a new
                // accessory, took the one our request made. The phone is reset and asked again, at most 3 times
                // (resetPhone), so Android Auto gets it back; never taken from under its holder while it holds it.
                ELOG("UsbLoop", "accessory on %s not available (%s): %s", phone.port.c_str(),
                     !described ? "not set up by the kernel" : strerror(why),
                     described && why == EBUSY ? "another program holds it: the phone is reset, then asked again, "
                                                 "within its limit"
                                               : "giving this one up");
                ours_.erase(phone.port);  // the next look resets the phone, within its limit
            }
            return false;
        }
        counts_[phone.port].claimTries = 0;
        int fds[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) != 0) {
            device->releaseInterface(0);
            delete device;
            return false;
        }
        bridge_ = new UsbBridge(device, endpoints, fds[0]);
        if (!bridge_->start()) {
            delete bridge_;
            bridge_ = nullptr;
            close(fds[1]);
            return false;
        }
        bridgePort_ = phone.port;
        bridgeStart_ = monoMs();
        ELOG("UsbLoop", "phone on %s (bus %u, address %u) in accessory mode: starting Android Auto", phone.port.c_str(),
             phone.bus, phone.address);
        host_.usbPhoneReady(fds[1]);
        return false;
    }

    bool resetPhone(const UsbPhone& phone) {
        PortCounts& c = counts_[phone.port];
        if (c.resets >= kMaxResets) {
            ELOG("UsbLoop", "phone on %s still not starting Android Auto after %u resets: unplug it and plug it again",
                 phone.port.c_str(), c.resets);
            leaveAlone(phone);
            return false;
        }
        ++c.resets;
        UsbDevice* device = setup_.open(phone.bus, phone.address);
        const bool done = device && device->reset();
        const int why = errno;
        delete device;
        if (!done) {
            ELOG("UsbLoop", "phone on %s cannot be reset (%s): unplug it and plug it again", phone.port.c_str(),
                 strerror(why));
            leaveAlone(phone);
            return false;
        }
        ELOG("UsbLoop", "phone on %s was left in accessory mode: reset %u of %u, then it is asked again",
             phone.port.c_str(), c.resets, kMaxResets);
        ours_.insert(phone.port);  // whatever comes back on this port is fresh
        quiet_ = true;
        quietSince_ = monoMs();
        return false;
    }

    bool askPhone(const UsbPhone& phone) {
        UsbDevice* device = setup_.open(phone.bus, phone.address);
        if (!device) {
            ELOG("UsbLoop", "device %04x:%04x on %s cannot be opened (%s): left alone", phone.vendor, phone.product,
                 phone.port.c_str(), strerror(errno));
            leaveAlone(phone);
            return false;
        }
        unsigned protocol = 0;
        int probeResult = 0;
        const AccessorySwitch result = askAccessoryMode(*device, protocol, probeResult);
        delete device;
        if (result == kSwitchProbeFailed) {
            ProbeRetry& retry = probeRetries_[phone.port];
            if (retry.address != phone.address) {
                retry.address = phone.address;
                retry.failures = 0;
            }
            ++retry.failures;
            const bool exhausted = retry.failures > kMaxProbeRetries;
            if (probeResult < 0) {
                ELOG("UsbLoop", "AOA protocol request on %s failed (%s): %s", phone.port.c_str(),
                     strerror(-probeResult), exhausted ? "retry limit reached, left alone" : "will retry");
            } else {
                ELOG("UsbLoop", "AOA protocol request on %s returned %d of 2 bytes: %s", phone.port.c_str(),
                     probeResult, exhausted ? "retry limit reached, left alone" : "will retry");
            }
            if (exhausted) {
                probeRetries_.erase(phone.port);
                leaveAlone(phone);
            }
            return false;
        }
        probeRetries_.erase(phone.port);
        if (result == kSwitchNotAndroid) {
            ELOG("UsbLoop", "device %04x:%04x on %s does not support AOA: left alone", phone.vendor, phone.product,
                 phone.port.c_str());
            leaveAlone(phone);
            return false;
        }
        PortCounts& c = counts_[phone.port];
        if (result == kSwitchFailed) {
            if (++c.askFailures >= kMaxAskFailures) {
                c.askFailures = 0;
                ELOG("UsbLoop", "phone %04x:%04x on %s refused the accessory requests: left alone", phone.vendor,
                     phone.product, phone.port.c_str());
                leaveAlone(phone);
            }
            return false;
        }
        c.askFailures = 0;
        ELOG("UsbLoop", "phone %04x:%04x on %s asked to start Android Auto (accessory protocol %u)", phone.vendor,
             phone.product, phone.port.c_str(), protocol);
        ours_.insert(phone.port);
        // The car's own USB service opens new accessories too: be there when it comes back.
        const uint32_t begin = monoMs();
        while (!host_.stopRequested() && monoMs() - begin < setup_.returnMs) {
            UsbPhone back;
            AccessoryEndpoints endpoints;
            if (readUsbDevice(setup_.sysfs, phone.port, back) && back.accessory && back.address != phone.address &&
                readAccessoryEndpoints(setup_.sysfs, phone.port, endpoints)) {
                unanswered_.erase(phone.port);
                return true;
            }
            usleep(kReturnStepUs);
        }
        // Not back yet. A slow phone that comes back later is used at the next look; one that never does is asked
        // again, a few times only: it does not do what it says.
        Unanswered& count = unanswered_[phone.port];
        if (count.address != phone.address) {
            count.address = phone.address;
            count.asks = 0;
        }
        if (++count.asks >= kMaxUnanswered) {
            ELOG("UsbLoop",
                 "phone %04x:%04x on %s took the accessory request %u times without coming back as an accessory: left "
                 "alone, unplug it and plug it again",
                 phone.vendor, phone.product, phone.port.c_str(), count.asks);
            unanswered_.erase(phone.port);
            leaveAlone(phone);
        }
        return false;
    }

    // The service stops: the bridge ends, and the phone is taken out of accessory mode so that it is not left
    // waiting for a head unit. Its next start then begins as a first one.
    void leave() {
        if (!bridge_) {
            return;
        }
        delete bridge_;
        bridge_ = nullptr;
        UsbPhone phone;
        if (readUsbDevice(setup_.sysfs, bridgePort_, phone) && phone.accessory) {
            UsbDevice* device = setup_.open(phone.bus, phone.address);
            const bool done = device && device->reset();
            delete device;
            ELOG("UsbLoop", "phone on %s taken out of accessory mode: %s", bridgePort_.c_str(),
                 done ? "done" : "not done, it leaves it when unplugged");
        }
    }

    const UsbLoopSetup& setup_;
    UsbHost& host_;
    std::map<std::string, ProbeRetry> probeRetries_;
    UsbBridge* bridge_;
    std::string bridgePort_;
    uint32_t bridgeStart_;
    // A phone's switch requests taken with no accessory back, by port: for the phone at that address.
    struct Unanswered {
        unsigned address, asks;
        Unanswered() : address(0), asks(0) {}
    };

    struct PortCounts {
        unsigned resets, silent, claimTries, askFailures;
        PortCounts() : resets(0), silent(0), claimTries(0), askFailures(0) {}
    };

    std::set<std::string> ours_;                 // ports whose accessory this loop made, or reset
    std::map<std::string, unsigned> leftAlone_;  // port and address of the devices not to touch again
    std::map<std::string, Unanswered> unanswered_;
    std::map<std::string, PortCounts> counts_;
    uint32_t quietSince_;
    bool quiet_;  // just after a reset: the bus is not looked at
    uint32_t emptySince_;
    bool empty_;  // no phone at the last look
};

}  // namespace

void runUsbLoop(const UsbLoopSetup& setup, UsbHost& host) {
    Loop(setup, host).run();
}

}  // namespace evo
