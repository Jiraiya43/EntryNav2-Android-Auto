// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The car's sound route for music over Wi-Fi: asked of the car's audio manager, as BMW's CarPlay asks for its own.
//
// What is written to a playback PCM is only heard once the car's audio manager has connected that PCM to the
// amplifier. BMW's CARPLAY asks for it on vMOST: ConnectionMaster (FBlock 0x03, instance 0x00) function 0x053
// BuildHighSyncConnection, StartResultAck (op 6) with [sender handle u16][connection id u16], big-endian; 0x054
// RemoveHighSyncConnection the same way gives it back. The car answers ProcessingAck (op 0xA) about once a second,
// then ResultAck (op 0xD), or ErrorAck (op 9). Connection 0x315 is CarPlay's media route, to pcm.entertainment.
// Tried on the reference car on 14 November 2026 with no iPhone connected: the car took 0x315, iDrive left its
// Bluetooth audio unselected, the music played on the speakers and the volume knob set its level.
//
// The request runs in a short process of its own ("aa-entrynav --audio-route build|remove <hex>"): libvmostclient
// keeps one connection per process, and the service's own belongs to the iDrive controller reader.
#ifndef AA_ENTRYNAV_PLATFORM_AUDIO_ROUTE_HPP
#define AA_ENTRYNAV_PLATFORM_AUDIO_ROUTE_HPP

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace evo {

const uint16_t kMediaRoute = 0x315;   // CarPlay's media route, to pcm.entertainment
const uint16_t kPromptRoute = 0x31A;  // CarPlay's alternate audio (Maps prompts), to pcm.carplay_alt
const uint8_t kConnectionMasterFblock = 0x03, kConnectionMasterInst = 0x00;
const uint16_t kBuildHighSyncConnection = 0x053, kRemoveHighSyncConnection = 0x054;
// Property read with Get [PosX][PosY] (CPxyConnectionMaster::init, libauxin-generic-proxies, 0xC03): the connections.
const uint16_t kHighSyncConnectionTableExt = 0xC03;
const unsigned kBuildTries = 5, kBuildRetryPauseMs = 200;  // a refused build is sent again (audioRouteMain)
const unsigned kReleaseWaitMs = 6000;      // a release waits at most this long for the car's answer
const uint16_t kRouteHandleBase = 0xA500;  // ours: apart from CARPLAY's 0xC3nn, whose replies it matches by handle

// A row of HighSyncConnectionTableExt; kConnectionPlaying = connected to the speakers (the radio's 0x0001 with FM,
// 0x315 with our music; 0x84/0x85 stay at 2). Read on the reference car on 21 November 2026.
struct RouteEntry {
    uint16_t id;
    uint8_t status;
};
const uint8_t kConnectionPlaying = 0;
// The table's Status payload: [PosX][PosY], then [id u16][status u8] per connection.
std::vector<RouteEntry> parseConnectionTable(const uint8_t* data, size_t len);
// Connections that played before `route` was built and are gone after it: what the car removed to make room.
std::vector<uint16_t> replacedConnections(const std::vector<RouteEntry>& before, const std::vector<RouteEntry>& after,
                                          uint16_t route);
bool somethingPlays(const std::vector<RouteEntry>& table);

// [handle][route], each big-endian.
void buildRoutePayload(uint8_t out[4], uint16_t handle, uint16_t route);

// The helper process: "--audio-route build|remove|take|give <hex route>" or "table 0" (take/give: see the .cpp). Sends the request, waits up to waitMs for the car's
// answer and logs it. 0 when the car answered ResultAck, 1 on ErrorAck or no answer, 2 on bad arguments.
int audioRouteMain(const char* verb, const char* route, unsigned waitMs = 15000);

// The outcome of a request: granted when the car answered ResultAck.
using RouteDone = void (*)(bool granted, void* user);

// From the service: runs the helper and logs the outcome, in the background (the car takes several seconds to answer)
// unless wait, as the service stops. All requests run one at a time, in the order they were made, so a remove
// never overtakes the build before it. done, when given, gets the outcome on the
// request's thread.
// restore: a build remembers the source the car removed for it, a remove plays that source again (the radio after our
// music), unless something else plays by then.
void requestAudioRoute(bool build, uint16_t route, bool wait = false, RouteDone done = nullptr, void* user = nullptr,
                       bool restore = false);

void waitForAudioRoutes();  // finish earlier releases before service exit

// Tests: requests go to `requester` instead of the helper process; null restores the helper.
using RouteRequester = void (*)(bool build, uint16_t route, RouteDone done, void* user);
void setAudioRouteRequesterForTest(RouteRequester requester);

}  // namespace evo

#endif  // AA_ENTRYNAV_PLATFORM_AUDIO_ROUTE_HPP
