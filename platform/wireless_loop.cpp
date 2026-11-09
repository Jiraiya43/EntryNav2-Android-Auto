// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The Bluetooth half of a wireless start (see wireless_loop.hpp).
#include "wireless_loop.hpp"

#include <unistd.h>

#include "alps_spp.hpp"
#include "evo_log.hpp"

namespace evo {

void runWirelessLoop(AlpsSppLink& bt, const aa::ApInfo& ap, uint16_t port, WirelessHost& host) {
    const int kPeekMs = 300;  // a MyBMW link speaks first: nothing is written into it
    while (!host.stopRequested() && bt.running()) {
        // Lifecycle guard: a returning SPP must not restart an active TCP projection.
        if (host.projectionBusy()) {
            usleep(100000);
            continue;
        }
        int fd = bt.waitLink(250);
        if (fd < 0) {
            continue;
        }
        // TCP may have arrived while waitLink was sleeping. Hold this local link without a new
        // WifiStartRequest until that projection ends (or Bluetooth/stop interrupts us).
        while (host.projectionBusy() && !host.stopRequested() && bt.linkUp()) {
            usleep(100000);
        }
        if (host.stopRequested() || !bt.linkUp()) {
            bt.closeLink();
            continue;
        }
        host.sppLinkOpened();
        ELOG("WirelessLoop", "SPP link open; starting the WPP exchange");
        // Credentials are captured from the live AP at startup, never from the saved car report.
        aa::WirelessSetup setup(fd, ap, port, kPeekMs);
        bool ok = !host.stopRequested() && setup.handshake(10000);
        host.handshakeEnded(setup.spokeWireless(), ok);
        if (ok) {
            ELOG("WirelessLoop", "WPP accepted; waiting for TCP on port %u", port);
            while (!host.stopRequested() && bt.linkUp() && setup.serve(1000)) {
            }
        }
        bt.closeLink();  // owns fd; no close(fd) in the caller
    }
}

}  // namespace evo
