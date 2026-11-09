// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Alps SPP UUID and payload encoding, and the hostapd configuration parser, on made-up values.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alps_spp.hpp"
#include "evo_ap.hpp"
#define CHECK(x)                                                 \
    do {                                                         \
        if (!(x)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
            exit(1);                                             \
        }                                                        \
    } while (0)
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
    puts("PASS UUID/payload Alps et configuration hostapd (aucun secret reel)");
    return 0;
}
