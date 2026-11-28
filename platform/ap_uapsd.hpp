// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The car's access point offers no U-APSD to the phone that joins it (#138).
//
// wland writes uapsd_advertisement_enabled=1 into hostapd's configuration. On an X1 F48 the WL18xx then held a phone's
// frames back for seconds while it saved power (firmware counter ps_poll_upsd_timeouts: 1700 in 12 minutes), and the
// picture and the sound stalled; with U-APSD off, the stalls were gone.
//
// 802.11n is turned off the same way (#203). During stalls on an X1 F48, a decrypted capture showed the car
// acknowledging the phone's frames on air and about a quarter of them never reaching its TCP; without 802.11n,
// 0.09 % in one run of 6 re-joins. Not told apart: chip firmware, driver, or mac80211's A-MPDU reordering. It holds
// for everything on the car's Wi-Fi and caps it at 54 Mbit/s.
//
// hostapd on the car is v2.1-devel. SET changes the setting in memory only (hostapd_set_iface): association responses
// follow it at once, but the beacon and the probe responses, which the chip sends from a template, go on offering
// U-APSD until hostapd rebuilds them. A phone asks for U-APSD from them in its association request, and hostapd keeps
// what it asked (check_wmm) for as long as that association lasts. RELOAD rebuilds them from the setting in memory, not
// from the file (hostapd_reload_iface), and disconnects every station first (hostapd_clear_old).
//
// RELOAD is sent even when a station is already associated. Leaving such a station alone kept U-APSD for its whole
// association: phones that join the car's Wi-Fi by themselves before Android Auto starts (a known network, or a phone
// back after a lost session) kept it for every session of a drive (an F52, 4 sessions of 4, #203). Disconnected once,
// they join again within a second or two, without U-APSD, and the phone at hand gets its Wi-Fi start request right
// after. Anything else on the car's Wi-Fi (a laptop, an iPhone on CarPlay) is disconnected once too.
//
// RELOAD is sent once per hostapd start: a phone that comes back reopens its Bluetooth link every 400 ms or so while it
// joins the Wi-Fi again, and a RELOAD each time slowed its return from 2 s to 3-7 s on an F45 (one of them may well
// have caught it joining). The start it was sent for is kept in a file of /tmp (evo_ap.cpp), so that a new service
// process (Stop then Start, a switch between USB and wireless) does not disconnect everyone again. A RELOAD that got no
// answer may have run: it is not sent again for that start; a refused one (FAIL, before hostapd disconnects anybody)
// is. When the hostapd start cannot be told (its control socket unreadable), a RELOAD could not be kept to once: it is
// then sent only with no station associated, as before. wland can start hostapd again from its own file: the setting
// is made before each Wi-Fi exchange with a phone, not once.
//
// Pure: the hostapd_cli calls are made by the caller (platform/evo_ap.cpp). The station list is only counted, never
// logged (MAC addresses). tests/test_parsers.cpp.
#ifndef ENTRY_AAD_PLATFORM_AP_UAPSD_HPP
#define ENTRY_AAD_PLATFORM_AP_UAPSD_HPP

#include <stdio.h>

#include <string>
#include <vector>

namespace evo {

enum class UapsdOffer {
    kWithdrawn,         // off, and the beacon rebuilt: the next phone joins without U-APSD
    kRejoined,          // off, the beacon rebuilt; who was on the Wi-Fi was disconnected once and joins without it
    kAlreadyDone,       // off, the beacon rebuilt earlier for this hostapd start: no RELOAD again
    kBeaconLeft,        // off for new associations; hostapd start unknown and a station associated: no RELOAD
    kReloadRefused,     // off in hostapd, but RELOAD refused (FAIL): the beacon still offers it, nobody disconnected
    kReloadUnanswered,  // off in hostapd, RELOAD not answered: it may have run; not sent again for this hostapd start
    kSetRefused,        // hostapd refused the setting or did not answer: left on
    kStopped,           // the service is stopping: nothing more asked of hostapd
};

inline const char* describeUapsdOffer(UapsdOffer offer) {
    switch (offer) {
        case UapsdOffer::kWithdrawn:
            return "U-APSD and 802.11n off, beacon rebuilt: the phone joins without them";
        case UapsdOffer::kRejoined:
            return "U-APSD off, beacon rebuilt: what was already on the car's Wi-Fi was disconnected once and joins "
                   "again without it";
        case UapsdOffer::kAlreadyDone:
            return "U-APSD off, beacon already rebuilt for this hostapd start";
        case UapsdOffer::kBeaconLeft:
            return "U-APSD off for new associations; a station is associated and the hostapd start is unknown, beacon "
                   "left, nobody disconnected";
        case UapsdOffer::kReloadRefused:
            return "U-APSD off in hostapd, RELOAD refused: the beacon may still offer it";
        case UapsdOffer::kReloadUnanswered:
            return "U-APSD off in hostapd, RELOAD not answered: it may have run, not asked again for this hostapd "
                   "start";
        case UapsdOffer::kStopped:
            return "service stopping: hostapd asked nothing more";
        case UapsdOffer::kSetRefused:
            break;
    }
    return "hostapd refused a setting: U-APSD or 802.11n left on";
}

namespace uapsd {
inline bool blank(const std::string& reply) {
    return reply.find_first_not_of(" \t\r\n") == std::string::npos;
}
// hostapd_cli prints the reply as it is: "OK" or "FAIL", an end of line after it.
inline bool ok(const std::string& reply) {
    const size_t end = reply.find_last_not_of(" \t\r\n");
    return end != std::string::npos && reply.compare(0, end + 1, "OK") == 0;
}
// A line of all_sta that opens a station: its address alone ("02:00:00:00:00:01"); its MIB lines hold a '='.
inline bool stationLine(const std::string& line) {
    if (line.size() != 17) {
        return false;
    }
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (i % 3 == 2 ? c != ':' : !hex) {
            return false;
        }
    }
    return true;
}
// The stations in an all_sta reply; -1 when the reply is not a station list (an error, an unknown command).
inline int stations(const std::string& reply) {
    int count = 0;
    size_t start = 0;
    while (start < reply.size()) {
        size_t end = reply.find('\n', start);
        if (end == std::string::npos) {
            end = reply.size();
        }
        std::string line = reply.substr(start, end - start);
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        if (stationLine(line)) {
            ++count;
        } else if (!blank(line) && line.find('=') == std::string::npos) {
            return -1;
        }
        start = end + 1;
    }
    return count;
}
}  // namespace uapsd

// Which hostapd start this is: its control socket, created anew each time hostapd starts (unlinked, then bound).
struct HostapdStart {
    unsigned long long device, inode;
    bool known;
    HostapdStart() : device(0), inode(0), known(false) {}
    bool same(const HostapdStart& other) const {
        return known && other.known && device == other.device && inode == other.inode;
    }
};

// A hostapd start as it is kept in its file: "<device> <inode>\n". Empty for an unknown one.
inline std::string formatHostapdStart(const HostapdStart& start) {
    if (!start.known) {
        return std::string();
    }
    char text[48];
    snprintf(text, sizeof(text), "%llu %llu\n", start.device, start.inode);
    return text;
}
// False, and an unknown start, for anything but what formatHostapdStart writes.
inline bool parseHostapdStart(const std::string& text, HostapdStart& out) {
    out = HostapdStart();
    unsigned long long device = 0, inode = 0;
    char end = 0;
    if (text.empty() || text[0] < '0' || text[0] > '9' || text.find('-') != std::string::npos ||
        sscanf(text.c_str(), "%llu %llu%c", &device, &inode, &end) != 3 || end != '\n' ||
        text.find('\n') != text.size() - 1) {
        return false;
    }
    out.device = device;
    out.inode = inode;
    out.known = true;
    return true;
}

// call(command, reply): runs one hostapd_cli command on wlan0 ({"set", ...}) and gives its reply; false when it could
// not run. At most four calls: the two settings, the station list, RELOAD. beaconDone: the beacon was rebuilt for this
// hostapd start already (the caller knows it from HostapdStart): the setting only. startKnown: the hostapd start could
// be told, so a RELOAD stays once per start and may disconnect who is there. stopping(): true once the service is
// asked to stop, checked before each call: a stop waits for the call under way only, never for several of up to 5 s
// each (the supervisor kills the service 10 s after asking it to stop).
template <typename Call, typename Stopping>
UapsdOffer withdrawUapsd(Call call, Stopping stopping, bool beaconDone = false, bool startKnown = true) {
    std::string reply;
    // Both settings first, then one RELOAD for the two.
    for (const char* setting : {"uapsd_advertisement_enabled", "ieee80211n"}) {
        if (stopping()) {
            return UapsdOffer::kStopped;
        }
        if (!call(std::vector<std::string>{"set", setting, "0"}, reply) || !uapsd::ok(reply)) {
            return UapsdOffer::kSetRefused;
        }
    }
    if (beaconDone) {
        return UapsdOffer::kAlreadyDone;
    }
    if (stopping()) {
        return UapsdOffer::kStopped;
    }
    // Unreadable: maybe someone is there, so the same as a station.
    const int there = call(std::vector<std::string>{"all_sta"}, reply) ? uapsd::stations(reply) : -1;
    if (there != 0 && !startKnown) {
        return UapsdOffer::kBeaconLeft;
    }
    if (stopping()) {
        return UapsdOffer::kStopped;
    }
    // hostapd_cli prints FAIL and ends well when hostapd refuses, before it disconnects anybody; it fails or is stopped
    // when hostapd does not answer in time, and then the RELOAD may have run all the same.
    if (!call(std::vector<std::string>{"raw", "RELOAD"}, reply)) {
        return UapsdOffer::kReloadUnanswered;
    }
    if (!uapsd::ok(reply)) {
        return UapsdOffer::kReloadRefused;
    }
    // A list that could not be read: maybe nobody was there, so no disconnection is claimed.
    return there > 0 ? UapsdOffer::kRejoined : UapsdOffer::kWithdrawn;
}

}  // namespace evo

#endif  // ENTRY_AAD_PLATFORM_AP_UAPSD_HPP
