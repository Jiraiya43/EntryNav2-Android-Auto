// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Switches the car's Wi-Fi hotspot on the way the BMW CarPlay service does when CarPlay is enabled in iDrive.

// How the BMW CarPlay service (CARPLAY) drives the hotspot through its WLAN proxy (CPxyWLAN):
//   - MOST FBlock WLAN 0xA0, instance 0x01 (wland's access point; wland also registers a P2P instance).
//   - Functions 0xC10 WLANState3 (property) and 0xC11 SelectWLANState3 (method).
//   - SelectWLANState3.StartResultAck: op 6, UWord sender handle + UByte state (0..1). The CarPlay activation switch
//     sends state 1 when CarPlay is switched on, 0 when it is switched off.
//   - WLANState3.Get: op 1; the Status reply (op 0xC) carries one UByte, the current state.
// Observed on the car: with the hotspot off, enabling "Apple CarPlay" in iDrive switched it back on, and it stayed on
// after a reboot.
#ifndef ENTRY_AAD_PLATFORM_EVO_WLAN_HPP
#define ENTRY_AAD_PLATFORM_EVO_WLAN_HPP

#include <stddef.h>
#include <stdint.h>

#include "vmost_client.hpp"

namespace evo {

const uint8_t kWlanFblock = 0xA0, kWlanApInstance = 0x01;
const uint16_t kWlanState3 = 0xC10, kSelectWlanState3 = 0xC11;

// SelectWLANState3.StartResultAck payload, MOST big-endian: handle (2 bytes), state (1 = on, 0 = off).
size_t buildSelectWlanState3(uint8_t out[3], uint16_t senderHandle, bool on);

struct WlanRequestResult {
    bool connected;     // vmostd session opened and WLAN.01 proxy registered
    int stateBefore;    // WLANState3 read before (-1 = no answer)
    bool sent;          // SelectWLANState3(on) sent
    int stateAfter;     // last WLANState3 seen after the request (-1 = none)
    int replyOp;        // last SelectWLANState3 reply op (-1 = none; 0xD ResultAck, 0x9 ErrorAck, 0xA ProcessingAck...)
    unsigned messages;  // WLAN.01 messages received
};

// Short vMOST session (libvmostclient, like CarPlay's WLAN proxy): register WLAN.01, Get WLANState3, and only when
// it is not 1, send SelectWLANState3(1); wait up to waitMs for wland's replies, log them, unregister, disconnect.
// Never sends "off". Returns true when the hotspot reported on or the request was sent. Host tests replace vmostd and
// its library with setVmostForTest (vmost_client.hpp).
bool requestWlanAccessPoint(unsigned waitMs, WlanRequestResult* result);

}  // namespace evo

#endif
