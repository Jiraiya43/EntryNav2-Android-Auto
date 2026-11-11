// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The Bluetooth half of a wireless start: each phone link on the SPP slot gets the Wi-Fi handshake, one at a time.
#ifndef ENTRY_AAD_PLATFORM_WIRELESS_LOOP_HPP
#define ENTRY_AAD_PLATFORM_WIRELESS_LOOP_HPP

#include <stdint.h>

#include "wireless_setup.hpp"

namespace evo {

class AlpsSppLink;

// What the loop asks of the service, from the loop's own thread.
class WirelessHost {
public:
    virtual bool stopRequested() = 0;
    // A phone connection is being prepared or its Android Auto session runs: no new handshake meanwhile.
    virtual bool projectionBusy() = 0;
    virtual void sppLinkOpened() = 0;
    // A handshake ended: the phone spoke the wireless protocol, it accepted the Wi-Fi.
    virtual void handshakeEnded(bool spoke, bool accepted) = 0;

protected:
    ~WirelessHost() {}
};

// Runs until a stop is requested or Bluetooth stops. Each link that comes up gets aa::WirelessSetup with the access
// point and TCP port the phone is told, after a short listen (a MyBMW link speaks first: nothing is written into it);
// once the phone accepts, the link is served until it closes. A link that comes up while a projection is busy is
// listened to (a ping is answered, nothing else is sent) until it closes or that projection ends.
void runWirelessLoop(AlpsSppLink& bt, const aa::ApInfo& ap, uint16_t port, WirelessHost& host);

}  // namespace evo

#endif
