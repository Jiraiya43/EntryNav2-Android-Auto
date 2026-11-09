// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Parser for hostapd get_config output and ap.conf, without I/O (shared with the PC tests).
#include <sstream>

#include "evo_ap.hpp"
namespace evo {
bool parseHostapdConfig(const std::string& text, aa::ApInfo& out) {
    if (text.size() > 65536 || text.find('\0') != std::string::npos) {
        return false;
    }
    aa::ApInfo ap;
    std::string line, keyMgmt, wpa;
    std::istringstream in(text);
    while (std::getline(in, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string name = line.substr(0, eq), value = line.substr(eq + 1);
        if (name == "ssid") {
            ap.ssid = value;
        } else if (name == "passphrase" || name == "wpa_passphrase") {
            ap.key = value;
        } else if (name == "bssid") {
            ap.bssid = value;
        } else if (name == "key_mgmt" || name == "wpa_key_mgmt") {
            keyMgmt = value;
        } else if (name == "wpa") {
            wpa = value;
        }
    }
    // A raw 64-hex PSK cannot be advertised as a WPP passphrase.
    if (ap.ssid.empty() || ap.ssid.size() > 32 || ap.key.size() < 8 || ap.key.size() > 63) {
        return false;
    }
    if (!wpa.empty() && wpa != "2" && wpa != "3") {
        return false;
    }
    if (!keyMgmt.empty() && keyMgmt != "WPA-PSK") {
        return false;
    }
    for (size_t i = 0; i < ap.key.size(); ++i) {
        if (static_cast<unsigned char>(ap.key[i]) < 32 || static_cast<unsigned char>(ap.key[i]) > 126) {
            return false;
        }
    }
    out = ap;
    return true;
}
}  // namespace evo
