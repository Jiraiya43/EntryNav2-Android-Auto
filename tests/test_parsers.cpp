// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Alps SPP UUID and payload encoding, the hostapd configuration parser and the access point's U-APSD steps, on made-up
// values.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "alps_spp.hpp"
#include "ap_uapsd.hpp"
#include "evo_ap.hpp"
#define CHECK(x)                                                 \
    do {                                                         \
        if (!(x)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
            exit(1);                                             \
        }                                                        \
    } while (0)

// hostapd as hostapd_cli shows it: one reply per command, given in order; the commands sent are kept.
struct FakeHostapd {
    std::vector<std::string> replies;  // "!" : the command could not run
    std::vector<std::string> sent;
    bool operator()(const std::vector<std::string>& command, std::string& reply) {
        std::string line;
        for (size_t i = 0; i < command.size(); ++i) {
            line += (i ? " " : "") + command[i];
        }
        sent.push_back(line);
        const std::string next = sent.size() <= replies.size() ? replies[sent.size() - 1] : "!";
        reply = next == "!" ? "" : next;
        return next != "!";
    }
};

// The service's stop, asked once this many hostapd commands were sent (0: before any; never: not asked).
struct StopAfter {
    const FakeHostapd* hostapd;
    size_t commands;
    bool operator()() const { return hostapd->sent.size() >= commands; }
};
const size_t never = static_cast<size_t>(-1);

evo::UapsdOffer withdraw(FakeHostapd& hostapd, std::initializer_list<const char*> replies, size_t stopAfter = never,
                         bool startKnown = true) {
    hostapd.replies.assign(replies.begin(), replies.end());
    hostapd.sent.clear();
    const StopAfter stopping = {&hostapd, stopAfter};
    return evo::withdrawUapsd(std::ref(hostapd), stopping, false, startKnown);
}
// The hostapd start could not be told: a RELOAD could not be kept to once per start.
evo::UapsdOffer withdrawStartUnknown(FakeHostapd& hostapd, std::initializer_list<const char*> replies) {
    return withdraw(hostapd, replies, never, false);
}

void uapsdCases() {
    FakeHostapd hostapd;
    // No station: the setting, then RELOAD to rebuild the beacon and the chip's probe response.
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "", "OK\n"}) == evo::UapsdOffer::kWithdrawn);
    CHECK(hostapd.sent.size() == 4 && hostapd.sent[0] == "set uapsd_advertisement_enabled 0" &&
          hostapd.sent[1] == "set ieee80211n 0" && hostapd.sent[2] == "all_sta" && hostapd.sent[3] == "raw RELOAD");
    CHECK(withdraw(hostapd, {"OK", "OK", " \n", "OK\r\n"}) == evo::UapsdOffer::kWithdrawn);
    // A station on the access point (its MIB lines), a phone that joined by itself: RELOAD all the same. It keeps
    // U-APSD for as long as it stays associated, so it is disconnected once and joins again without it (#203).
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "02:00:00:00:00:01\nflags=[AUTH][ASSOC]\naid=1\n", "OK\n"}) ==
          evo::UapsdOffer::kRejoined);
    CHECK(hostapd.sent.size() == 4 && hostapd.sent[3] == "raw RELOAD");
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "02:00:00:00:00:01\n", "FAIL\n"}) == evo::UapsdOffer::kReloadRefused);
    // RELOAD not answered (hostapd_cli timed out or failed): it may have run, told apart from a refusal.
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "02:00:00:00:00:01\n", "!"}) == evo::UapsdOffer::kReloadUnanswered);
    // A station list that cannot be read is no proof of an empty one: RELOAD all the same, but no disconnection is
    // claimed, since maybe nobody was there.
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "!", "OK\n"}) == evo::UapsdOffer::kWithdrawn && hostapd.sent.size() == 4);
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "'STA-FIRST' command failed.\n", "OK\n"}) == evo::UapsdOffer::kWithdrawn);
    // The hostapd start unknown: a RELOAD could not be kept to once per start, so nobody is disconnected, as before.
    CHECK(withdrawStartUnknown(hostapd, {"OK\n", "OK\n", "02:00:00:00:00:01\nflags=[AUTH][ASSOC]\n"}) ==
          evo::UapsdOffer::kBeaconLeft);
    CHECK(hostapd.sent.size() == 3);
    CHECK(withdrawStartUnknown(hostapd, {"OK\n", "OK\n", "!"}) == evo::UapsdOffer::kBeaconLeft &&
          hostapd.sent.size() == 3);
    CHECK(withdrawStartUnknown(hostapd, {"OK\n", "OK\n", "", "OK\n"}) == evo::UapsdOffer::kWithdrawn);
    // The station count of an all_sta reply, MIB lines skipped; -1 for anything that is no station list.
    CHECK(evo::uapsd::stations("") == 0 && evo::uapsd::stations(" \r\n") == 0);
    CHECK(evo::uapsd::stations("02:00:00:00:00:01\nflags=[AUTH]\n0A:0b:00:00:00:02\r\naid=2\r\n") == 2);
    CHECK(evo::uapsd::stations("'STA-FIRST' command failed.\n") == -1);
    CHECK(evo::uapsd::stations("02:00:00:00:00:0Z\n") == -1 && evo::uapsd::stations("02-00-00-00-00-01\n") == -1);
    CHECK(strstr(evo::describeUapsdOffer(evo::UapsdOffer::kRejoined), "disconnected once"));
    // A setting refused or not answered: nothing else is sent.
    CHECK(withdraw(hostapd, {"OK\n", "FAIL\n"}) == evo::UapsdOffer::kSetRefused && hostapd.sent.size() == 2);
    CHECK(withdraw(hostapd, {"FAIL\n"}) == evo::UapsdOffer::kSetRefused && hostapd.sent.size() == 1);
    CHECK(withdraw(hostapd, {"!"}) == evo::UapsdOffer::kSetRefused && hostapd.sent.size() == 1);
    CHECK(withdraw(hostapd, {"OKAY\n"}) == evo::UapsdOffer::kSetRefused);
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "", "FAIL\n"}) == evo::UapsdOffer::kReloadRefused);
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "", "!"}) == evo::UapsdOffer::kReloadUnanswered);
    // The beacon already rebuilt for this hostapd start: the setting again, never a second RELOAD (a phone that comes
    // back reopens its Bluetooth link every 400 ms or so while it joins).
    const StopAfter running = {&hostapd, never};
    hostapd.replies.assign({"OK\n", "OK\n", "", "OK\n"});
    hostapd.sent.clear();
    CHECK(evo::withdrawUapsd(std::ref(hostapd), running, true) == evo::UapsdOffer::kAlreadyDone);
    CHECK(hostapd.sent.size() == 2 && hostapd.sent[1] == "set ieee80211n 0");
    hostapd.replies.assign({"FAIL\n"});
    hostapd.sent.clear();
    CHECK(evo::withdrawUapsd(std::ref(hostapd), running, true) == evo::UapsdOffer::kSetRefused);
    // The service asked to stop: no further hostapd command, whose 5 s each kept the stop waiting (15 s in all, past
    // the supervisor's 10 s before it kills the service).
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "", "OK\n"}, 0) == evo::UapsdOffer::kStopped && hostapd.sent.empty());
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "", "OK\n"}, 1) == evo::UapsdOffer::kStopped && hostapd.sent.size() == 1);
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "", "OK\n"}, 2) == evo::UapsdOffer::kStopped && hostapd.sent.size() == 2);
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "", "OK\n"}, 3) == evo::UapsdOffer::kStopped && hostapd.sent.size() == 3);
    CHECK(withdraw(hostapd, {"OK\n", "OK\n", "", "OK\n"}, 4) == evo::UapsdOffer::kWithdrawn &&
          hostapd.sent.size() == 4);
    CHECK(withdraw(hostapd, {"FAIL\n"}, 1) == evo::UapsdOffer::kSetRefused);  // a refusal stays a refusal
    CHECK(strstr(evo::describeUapsdOffer(evo::UapsdOffer::kStopped), "stopping"));
    // Which hostapd start: its control socket's identity; unknown is never the same start.
    evo::HostapdStart a, b, unknown;
    a.device = b.device = 1;
    a.inode = b.inode = 42;
    a.known = b.known = true;
    CHECK(a.same(b) && !a.same(unknown) && !unknown.same(unknown));
    b.inode = 43;  // hostapd started again: a new socket
    CHECK(!a.same(b));
    // Kept in its file across the service's processes: what is written reads back as the same start.
    evo::HostapdStart kept;
    CHECK(evo::formatHostapdStart(a) == "1 42\n" && evo::parseHostapdStart("1 42\n", kept) && kept.same(a));
    CHECK(evo::formatHostapdStart(unknown).empty());
    CHECK(!evo::parseHostapdStart("", kept) && !kept.known);
    CHECK(!evo::parseHostapdStart("1 42", kept) && !evo::parseHostapdStart("1 42\n7 8\n", kept));
    CHECK(!evo::parseHostapdStart("-1 42\n", kept) && !evo::parseHostapdStart("x 42\n", kept) && !kept.known);
    CHECK(!evo::parseHostapdStart("1 -42\n", kept) && !kept.known);
    CHECK(strstr(evo::describeUapsdOffer(evo::UapsdOffer::kReloadUnanswered), "not asked again"));
    CHECK(strstr(evo::describeUapsdOffer(evo::UapsdOffer::kSetRefused), "left on"));
}

int main() {
    uint8_t uuid[16];
    const uint8_t expected[] = {0x4d, 0xe1, 0x7a, 0x00, 0x52, 0xcb, 0x11, 0xe6,
                                0xbd, 0xf4, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66};
    CHECK(evo::parseUuid128(evo::kAaUuid, uuid));
    CHECK(!memcmp(uuid, expected, 16));
    CHECK(evo::parseUuid128("4DE17A00-52CB-11E6-BDF4-0800200C9A66", uuid));
    CHECK(!memcmp(uuid, expected, 16));
    const char* bad[] = {"", "4de17a00-52cb-11e6-bdf4-0800200c9a6", "4de17a00-52cb-11e6-bdf4-0800200c9a660",
                         "4de17a00_52cb-11e6-bdf4-0800200c9a66", "4de17a00-52cb-11e6-bdf4-0800200c9a6g"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        memset(uuid, 0xaa, sizeof(uuid));
        CHECK(!evo::parseUuid128(bad[i], uuid));
        CHECK(uuid[0] == 0xaa);
    }
    CHECK(!evo::parseUuid128(0, uuid));
    CHECK(!evo::parseUuid128(evo::kAaUuid, 0));
    uint8_t local[16];
    memset(local, 0x42, sizeof(local));
    std::vector<uint8_t> payload = evo::buildSetUuidPayload(expected, local);
    CHECK(payload.size() == 34 && payload[0] == 3 && payload[17] == 3);
    CHECK(!memcmp(&payload[1], expected, 16) && !memcmp(&payload[18], local, 16));
    CHECK(evo::buildSetUuidPayload(0, local).empty());
    aa::ApInfo ap;
    ap.ssid = "unchanged";
    CHECK(
        evo::parseHostapdConfig("Selected interface 'wlan0'\nssid=BMW test\npassphrase=demo#= pass "
                                "\nkey_mgmt=WPA-PSK\nbssid=12:34:56:78:9a:bc\n",
                                ap));
    CHECK(ap.ssid == "BMW test" && ap.key == "demo#= pass " && ap.security == 8);
    CHECK(evo::parseHostapdConfig("# comment\r\nssid=BMW\r\nwpa_passphrase=demo1234\r\nwpa=2\r\n", ap));
    CHECK(ap.key == "demo1234" && ap.bssid.empty());
    CHECK(!evo::parseHostapdConfig("ssid=bad\nwpa_passphrase=short\n", ap));
    CHECK(ap.ssid == "BMW");
    CHECK(!evo::parseHostapdConfig("ssid=bad\npsk=" + std::string(64, 'a') + "\n", ap));
    CHECK(!evo::parseHostapdConfig("ssid=bad\npassphrase=demo1234\nkey_mgmt=WPA-EAP\n", ap));
    CHECK(!evo::parseHostapdConfig("ssid=bad\npassphrase=demo1234\nwpa=0\n", ap));
    CHECK(!evo::parseHostapdConfig("ssid=" + std::string(33, 's') + "\npassphrase=demo1234\n", ap));
    CHECK(!evo::parseHostapdConfig("ssid=ok\npassphrase=" + std::string(64, 'k') + "\n", ap));
    CHECK(!evo::parseHostapdConfig("ssid=ok\npassphrase=demo1234" + std::string(1, '\0'), ap));
    CHECK(!evo::parseHostapdConfig(std::string(65537, 'x'), ap));
    uapsdCases();
    puts("PASS UUID/payload Alps et configuration hostapd (aucun secret reel), U-APSD du point d'acces");
    return 0;
}
