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
        int fd = bt.waitLink(250);
        if (fd < 0) {
            continue;
        }
        // Credentials are captured from the live AP at startup, never from the saved car report.
        aa::WirelessSetup setup(fd, ap, port, kPeekMs);
        // Lifecycle guard: a link that comes up while a phone connection is prepared or its session runs must not
        // restart that projection: no WifiStartRequest goes into it. On this car the phone opens one about every
        // 12 s during a session; it is listened to, so the log says what the phone expects there.
        if (host.projectionBusy()) {
            const uint32_t sinceMs = monoMs();
            bool open = true;
            while (open && host.projectionBusy() && !host.stopRequested() && bt.linkUp()) {
                open = setup.listen(100);
            }
            ELOG("WirelessLoop", "Bluetooth link back while Android Auto runs: %u message(s) from the phone in %u ms",
                 setup.heard(), monoMs() - sinceMs);
        }
        if (host.stopRequested() || !bt.linkUp()) {
            bt.closeLink();
            continue;
        }
        host.sppLinkOpened();
        ELOG("WirelessLoop", "SPP link open; starting the WPP exchange");
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
