// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// evo-capture : passive SOME/IP recorder for EntryNav2 / EntryEVO. RECEIVE ONLY: one AF_PACKET socket that
// reads copies of the IPv4 traffic the HU already sends/receives (needed to find the G20 cluster guidance service,
// BMW.INFOTAINMENT.REMOTEINSTRUMENTCLUSTER.RICServiceGuidance, served by the HU). Nothing is ever sent, no interface
// flag is changed (no promiscuous mode), no BMW file is touched. Bounded in time (default 240 s, max 900 s) and size
// (pcap closed at 24 MB, the summary goes on). Writes a pcap (raw IPv4, Wireshark) under /tmp/entry-aa-releve and
// prints a text summary: SOME/IP (service, method, type) pairs with peers and sample payloads, SOME/IP-SD
// offers/subscriptions. A developer tool, not part of a release (make dev-tools).
//   evo-capture --someip [seconds]     record then summarize
//   evo-capture --selftest             parser check on synthetic packets (PC/QEMU)
#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

const char* const kOutDir = "/tmp/entry-aa-releve";
const unsigned kSnap = 2048;
const long kDefaultSeconds = 240, kMaxSeconds = 900;
const long kMaxBytes = 24L * 1024 * 1024;
volatile sig_atomic_t g_stop = 0;
void onSignal(int) {
    g_stop = 1;
}

unsigned be16(const uint8_t* p) {
    return static_cast<unsigned>(p[0]) << 8 | p[1];
}
uint32_t be32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 |
           p[3];
}
std::string ipText(const uint8_t* p) {
    char b[20];
    snprintf(b, sizeof(b), "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
    return b;
}
std::string hex(const uint8_t* p, size_t n, size_t max) {
    std::string s;
    char b[4];
    for (size_t i = 0; i < n && i < max; ++i) {
        snprintf(b, sizeof(b), "%02x", p[i]);
        s += b;
    }
    if (n > max) {
        s += "..";
    }
    return s;
}
const char* typeName(unsigned t) {
    switch (t & ~0x20u) {  // 0x20 = TP flag
        case 0x00:
            return "REQUEST";
        case 0x01:
            return "REQUEST_NO_RETURN";
        case 0x02:
            return "NOTIFICATION";
        case 0x80:
            return "RESPONSE";
        case 0x81:
            return "ERROR";
        default:
            return "?";
    }
}
const char* sdEntryName(unsigned t, uint32_t ttl) {
    switch (t) {
        case 0x00:
            return "FIND";
        case 0x01:
            return ttl ? "OFFER" : "STOP_OFFER";
        case 0x06:
            return ttl ? "SUBSCRIBE" : "STOP_SUBSCRIBE";
        case 0x07:
            return ttl ? "SUBSCRIBE_ACK" : "SUBSCRIBE_NACK";
        default:
            return "?";
    }
}

struct Pair {
    unsigned long count = 0, bytes = 0;
    std::set<std::string> flows;
    std::vector<std::string> samples;
};

struct Summary {
    std::map<std::string, unsigned long> perIf;
    std::map<std::string, Pair> pairs;        // "svc 0x.... method 0x.... TYPE"
    std::map<std::string, unsigned long> sd;  // one line per distinct SD entry (+endpoint)
    unsigned long ipv4 = 0, someip = 0, skipped = 0;

    void addSomeIp(const std::string& flow, const uint8_t* p, size_t n, bool tcp) {
        while (n >= 16) {
            unsigned svc = be16(p), method = be16(p + 2);
            uint32_t len = be32(p + 4);
            if (p[12] != 0x01 || len < 8 || len > 0x100000) {
                return;  // protocol version 1 only
            }
            size_t total = 8 + static_cast<size_t>(len);
            if (!tcp && total > n) {
                return;
            }
            size_t have = total > n ? n : total;
            unsigned type = p[14];
            ++someip;
            if (svc == 0xffff && method == 0x8100) {
                addSd(flow, p + 16, have - 16);
            }
            char key[96];
            snprintf(key, sizeof(key), "svc 0x%04x method 0x%04x %s (iface v%u)", svc, method, typeName(type), p[13]);
            Pair& e = pairs[key];
            ++e.count;
            e.bytes += total;
            if (e.flows.size() < 6) {
                e.flows.insert(flow);
            }
            if (e.samples.size() < 3) {
                e.samples.push_back(hex(p + 16, have - 16, 96));
            }
            if (have < total) {
                return;
            }
            p += total;
            n -= total;
        }
    }

    void addSd(const std::string& flow, const uint8_t* p, size_t n) {
        if (n < 8) {
            return;
        }
        // Lengths come from the network: compared by subtraction, so that a huge value cannot wrap a 32-bit size_t.
        uint32_t elen = be32(p + 4);
        if (n < 12 || elen > n - 12) {
            return;
        }
        const uint8_t* entries = p + 8;
        const uint8_t* opts = entries + elen + 4;
        uint32_t olen = be32(entries + elen);
        if (olen > n - static_cast<size_t>(opts - p)) {
            olen = 0;
        }
        std::vector<std::string> endpoints;
        for (size_t o = 0; o + 3 <= olen;) {
            unsigned l = be16(opts + o), t = opts[o + 2];
            if (o + 3 + l > olen) {
                break;
            }
            if ((t == 0x04 || t == 0x14) && l == 9) {  // IPv4 endpoint / multicast
                const uint8_t* q = opts + o + 4;       // IPv4[4] reserved[1] L4-proto[1] port[2]
                char b[48];
                snprintf(b, sizeof(b), "%s %s:%u",
                         q[5] == 6    ? "tcp"
                         : q[5] == 17 ? "udp"
                                      : "?",
                         ipText(q).c_str(), be16(q + 6));
                endpoints.push_back(b);
            } else {
                endpoints.push_back("opt-type-0x" + hex(opts + o + 2, 1, 1));
            }
            o += 3 + l;
        }
        for (size_t e = 0; e + 16 <= elen; e += 16) {
            const uint8_t* q = entries + e;
            unsigned t = q[0], i1 = q[1], i2 = q[2], n1 = q[3] >> 4, n2 = q[3] & 15;
            uint32_t ttl = be32(q + 8) & 0xffffff;
            char b[160];
            if (t == 0x06 || t == 0x07) {
                snprintf(b, sizeof(b), "%-14s svc 0x%04x inst 0x%04x major %u eventgroup 0x%04x", sdEntryName(t, ttl),
                         be16(q + 4), be16(q + 6), q[8], be16(q + 14));
            } else {
                snprintf(b, sizeof(b), "%-14s svc 0x%04x inst 0x%04x major %u minor %u", sdEntryName(t, ttl),
                         be16(q + 4), be16(q + 6), q[8], be32(q + 12));
            }
            std::string line = std::string(b) + "  [" + flow + "]";
            for (unsigned k = 0; k < n1 && i1 + k < endpoints.size(); ++k) {
                line += " -> " + endpoints[i1 + k];
            }
            for (unsigned k = 0; k < n2 && i2 + k < endpoints.size(); ++k) {
                line += " -> " + endpoints[i2 + k];
            }
            ++sd[line];
        }
    }

    // One IPv4 datagram (network header first), as delivered by a SOCK_DGRAM packet socket.
    void addIpv4(const std::string& ifname, const uint8_t* p, size_t n) {
        ++perIf[ifname];
        if (n < 20 || (p[0] >> 4) != 4) {
            return;
        }
        ++ipv4;
        size_t ihl = (p[0] & 15) * 4u;
        size_t tot = be16(p + 2);
        if (ihl < 20 || tot < ihl) {
            return;
        }
        if (tot < n) {
            n = tot;
        }
        if (be16(p + 6) & 0x1fff) {
            return;  // non-first fragment
        }
        const uint8_t* l4 = p + ihl;
        size_t l4n = n > ihl ? n - ihl : 0;
        unsigned proto = p[9];
        std::string src = ipText(p + 12), dst = ipText(p + 16);
        if (proto == 17 && l4n >= 8) {
            unsigned sp = be16(l4), dp = be16(l4 + 2);
            char flow[96];
            snprintf(flow, sizeof(flow), "udp %s:%u > %s:%u %s", src.c_str(), sp, dst.c_str(), dp, ifname.c_str());
            addSomeIp(flow, l4 + 8, l4n - 8, false);
        } else if (proto == 6 && l4n >= 20) {
            unsigned sp = be16(l4), dp = be16(l4 + 2);
            if (sp == 22 || dp == 22) {
                ++skipped;
                return;
            }  // our own SSH session
            size_t off = (l4[12] >> 4) * 4u;
            if (off < 20 || off > l4n) {
                return;
            }
            char flow[96];
            snprintf(flow, sizeof(flow), "tcp %s:%u > %s:%u %s", src.c_str(), sp, dst.c_str(), dp, ifname.c_str());
            addSomeIp(flow, l4 + off, l4n - off, true);
        }
    }

    void print(FILE* f) const {
        fprintf(f, "\n=== Interfaces (IPv4 packets seen)\n");
        for (std::map<std::string, unsigned long>::const_iterator i = perIf.begin(); i != perIf.end(); ++i) {
            fprintf(f, "  %-12s %lu\n", i->first.c_str(), i->second);
        }
        fprintf(f, "IPv4 %lu, SOME/IP messages %lu, SSH ignored %lu\n", ipv4, someip, skipped);
        fprintf(f, "\n=== SOME/IP-SD (distinct offers and subscriptions)\n");
        for (std::map<std::string, unsigned long>::const_iterator i = sd.begin(); i != sd.end(); ++i) {
            fprintf(f, "  x%-5lu %s\n", i->second, i->first.c_str());
        }
        fprintf(f, "\n=== SOME/IP (service, method or event)\n");
        for (std::map<std::string, Pair>::const_iterator i = pairs.begin(); i != pairs.end(); ++i) {
            fprintf(f, "%s: %lu messages, %lu bytes\n", i->first.c_str(), i->second.count, i->second.bytes);
            for (std::set<std::string>::const_iterator j = i->second.flows.begin(); j != i->second.flows.end(); ++j) {
                fprintf(f, "    flow %s\n", j->c_str());
            }
            for (size_t k = 0; k < i->second.samples.size(); ++k) {
                fprintf(f, "    sample %s\n", i->second.samples[k].c_str());
            }
        }
    }
};

struct Pcap {
    FILE* f = nullptr;
    long bytes = 0;
    bool open(const std::string& path) {
        f = fopen(path.c_str(), "wbx");
        if (!f) {
            return false;
        }
        uint32_t h[6] = {0xa1b2c3d4u, 0x00040002u, 0, 0, 65535, 101};  // v2.4, LINKTYPE_RAW (IPv4)
        fwrite(h, sizeof(h), 1, f);
        bytes = sizeof(h);
        return true;
    }
    void add(const struct timeval& tv, const uint8_t* p, size_t caplen, size_t len) {
        uint32_t r[4] = {static_cast<uint32_t>(tv.tv_sec), static_cast<uint32_t>(tv.tv_usec),
                         static_cast<uint32_t>(caplen), static_cast<uint32_t>(len)};
        fwrite(r, sizeof(r), 1, f);
        fwrite(p, 1, caplen, f);
        bytes += sizeof(r) + caplen;
    }
    void close() {
        if (f) {
            fclose(f);
            f = nullptr;
        }
    }
};

int record(long seconds) {
    if (geteuid() != 0) {
        fprintf(stderr, "REFUSED: root required (read-only AF_PACKET socket)\n");
        return 2;
    }
    if (mkdir(kOutDir, 0700) != 0 && errno != EEXIST) {
        fprintf(stderr, "REFUSED: cannot create %s\n", kOutDir);
        return 2;
    }
    struct stat st;
    if (lstat(kOutDir, &st) || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "REFUSED: %s invalid\n", kOutDir);
        return 2;
    }
    int s = socket(PF_PACKET, SOCK_DGRAM | SOCK_CLOEXEC, htons(ETH_P_ALL));  // ALL: also the HU's outgoing copies
    if (s < 0) {
        fprintf(stderr, "REFUSED: AF_PACKET socket: %s\n", strerror(errno));
        return 2;
    }
    int rcv = 4 * 1024 * 1024;
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv));
    char name[96];
    time_t now = time(nullptr);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(name, sizeof(name), "/someip-%Y%m%d-%H%M%S.pcap", &tmv);
    std::string path = std::string(kOutDir) + name;
    Pcap pcap;
    if (!pcap.open(path)) {
        fprintf(stderr, "REFUSED: %s: %s\n", path.c_str(), strerror(errno));
        close(s);
        return 2;
    }
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    signal(SIGHUP, onSignal);
    signal(SIGPIPE, SIG_IGN);
    printf("Passive recording %ld s -> %s (read only, nothing sent, at most %ld MB)\n", seconds, path.c_str(),
           kMaxBytes >> 20);
    fflush(stdout);
    Summary sum;
    std::map<int, std::string> names;
    uint8_t buf[65536];
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    long lastTick = 0;
    unsigned long kept = 0;
    bool full = false;
    while (!g_stop) {
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        long el = t.tv_sec - t0.tv_sec;
        if (el >= seconds) {
            break;
        }
        if (el / 30 != lastTick) {
            lastTick = el / 30;
            printf("  ... %ld s, %lu IPv4 packets kept, %ld KB, %lu SOME/IP messages\n", el, kept, pcap.bytes >> 10,
                   sum.someip);
            fflush(stdout);
        }
        struct pollfd pfd = {s, POLLIN, 0};
        if (poll(&pfd, 1, 500) <= 0) {
            continue;
        }
        struct sockaddr_ll from;
        socklen_t fl = sizeof(from);
        ssize_t n = recvfrom(s, buf, sizeof(buf), MSG_TRUNC, reinterpret_cast<sockaddr*>(&from), &fl);
        if (n <= 0) {
            continue;
        }
        if (from.sll_protocol != htons(ETH_P_IP)) {
            continue;
        }
        std::map<int, std::string>::iterator it = names.find(from.sll_ifindex);
        if (it == names.end()) {
            char ifn[IF_NAMESIZE + 1] = {0};
            if (!if_indextoname(from.sll_ifindex, ifn)) {
                snprintf(ifn, sizeof(ifn), "if%d", from.sll_ifindex);
            }
            it = names.insert(std::make_pair(from.sll_ifindex, std::string(ifn))).first;
        }
        if (it->second == "lo") {
            continue;  // local control traffic (entry-aad, HMI)
        }
        size_t len = static_cast<size_t>(n), cap = len > sizeof(buf) ? sizeof(buf) : len;
        if (cap >= 40 && buf[9] == 6 &&
            (be16(buf + (buf[0] & 15) * 4) == 22 || be16(buf + (buf[0] & 15) * 4 + 2) == 22)) {
            ++sum.skipped;
            continue;  // never record the SSH session itself
        }
        sum.addIpv4(it->second, buf, cap);
        if (!full) {
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            size_t snap = cap > kSnap ? kSnap : cap;
            if (pcap.bytes + static_cast<long>(snap) + 16 > kMaxBytes) {
                full = true;
                printf("  size limit reached: pcap closed, the summary goes on\n");
            } else {
                pcap.add(tv, buf, snap, len);
                ++kept;
            }
        }
    }
    pcap.close();
    close(s);
    printf("Done: %lu packets in %s (%ld KB)\n", kept, path.c_str(), pcap.bytes >> 10);
    sum.print(stdout);
    fflush(stdout);
    return 0;
}

// ---- self test (no network) ----------------------------------------------------------------------------------------
std::vector<uint8_t> udp(const uint8_t src[4], unsigned sp, const uint8_t dst[4], unsigned dp,
                         const std::vector<uint8_t>& pl) {
    std::vector<uint8_t> v(28 + pl.size(), 0);
    v[0] = 0x45;
    v[2] = v.size() >> 8;
    v[3] = v.size() & 255;
    v[8] = 64;
    v[9] = 17;
    memcpy(&v[12], src, 4);
    memcpy(&v[16], dst, 4);
    v[20] = sp >> 8;
    v[21] = sp & 255;
    v[22] = dp >> 8;
    v[23] = dp & 255;
    unsigned ul = 8 + pl.size();
    v[24] = ul >> 8;
    v[25] = ul & 255;
    memcpy(&v[28], pl.data(), pl.size());
    return v;
}
std::vector<uint8_t> someip(unsigned svc, unsigned m, unsigned type, const std::vector<uint8_t>& body) {
    std::vector<uint8_t> v(16 + body.size(), 0);
    unsigned len = 8 + body.size();
    v[0] = svc >> 8;
    v[1] = svc & 255;
    v[2] = m >> 8;
    v[3] = m & 255;
    v[4] = len >> 24;
    v[5] = len >> 16;
    v[6] = len >> 8;
    v[7] = len;
    v[12] = 1;
    v[13] = 1;
    v[14] = type;
    memcpy(&v[16], body.data(), body.size());
    return v;
}

int selftest() {
    const uint8_t hu[4] = {160, 48, 199, 119}, kombi[4] = {160, 48, 199, 20}, mc[4] = {224, 244, 224, 245};
    Summary s;
    // SD offer from the HU with an IPv4 UDP endpoint option.
    std::vector<uint8_t> sd(4 + 4 + 16 + 4 + 12, 0);
    sd[0] = 0xc0;
    sd[7] = 16;  // flags, entries length 16
    uint8_t* e = &sd[8];
    e[0] = 0x01;
    e[1] = 0;
    e[3] = 0x10;
    e[4] = 0x12;
    e[5] = 0x34;
    e[6] = 0;
    e[7] = 1;
    e[8] = 1;
    e[11] = 3;
    sd[27] = 12;  // options length 12
    uint8_t* o = &sd[28];
    o[1] = 9;
    o[2] = 0x04;
    memcpy(o + 4, hu, 4);
    o[9] = 17;
    o[10] = 0x77;
    o[11] = 0x5e;  // udp 160.48.199.119:30558
    std::vector<uint8_t> p = udp(hu, 30490, mc, 30490, someip(0xffff, 0x8100, 0x02, sd));
    s.addIpv4("vbr0.73", p.data(), p.size());
    // Two notifications in one UDP datagram (event 0x8001 of service 0x1234) to the cluster.
    std::vector<uint8_t> a = someip(0x1234, 0x8001, 0x02, std::vector<uint8_t>(5, 0xab));
    std::vector<uint8_t> b = someip(0x1234, 0x8001, 0x02, std::vector<uint8_t>(3, 0xcd));
    a.insert(a.end(), b.begin(), b.end());
    p = udp(hu, 30558, kombi, 30501, a);
    s.addIpv4("vbr0.73", p.data(), p.size());
    // Garbage UDP (not SOME/IP) is ignored.
    p = udp(kombi, 5000, hu, 5001, std::vector<uint8_t>(20, 0x55));
    s.addIpv4("vbr0.73", p.data(), p.size());
    bool ok = s.someip == 3 && s.sd.size() == 1 && s.pairs.count("svc 0x1234 method 0x8001 NOTIFICATION (iface v1)") &&
              s.pairs["svc 0x1234 method 0x8001 NOTIFICATION (iface v1)"].count == 2 &&
              s.sd.begin()->first.find("OFFER") == 0 &&
              s.sd.begin()->first.find("udp 160.48.199.119:30558") != std::string::npos &&
              s.sd.begin()->first.find("svc 0x1234 inst 0x0001") != std::string::npos;
    s.print(stdout);
    // Lengths that would wrap a 32-bit size_t (the car's ARM): entries refused, options ignored, nothing read past the
    // packet.
    Summary w;
    std::vector<uint8_t> bad = sd;
    bad[4] = bad[5] = bad[6] = 0xff;
    bad[7] = 0xf8;  // entries length 0xfffffff8
    p = udp(hu, 30490, mc, 30490, someip(0xffff, 0x8100, 0x02, bad));
    w.addIpv4("vbr0.73", p.data(), p.size());
    const bool entriesRefused = w.someip == 1 && w.sd.empty();
    bad = sd;
    bad[24] = bad[25] = bad[26] = 0xff;
    bad[27] = 0xfc;  // options length 0xfffffffc
    p = udp(hu, 30490, mc, 30490, someip(0xffff, 0x8100, 0x02, bad));
    w.addIpv4("vbr0.73", p.data(), p.size());
    const bool optionsIgnored = w.sd.size() == 1 && w.sd.begin()->first.find("OFFER") == 0 &&
                                w.sd.begin()->first.find(" -> ") == std::string::npos;
    ok = ok && entriesRefused && optionsIgnored;
    printf(ok ? "PASS evo-capture selftest (SD offer + endpoint, 2 SOME/IP in one datagram, non-SOME/IP ignored, "
                "wrapping SD lengths refused)\n"
              : "FAIL evo-capture selftest\n");
    return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    if (argc >= 2 && !strcmp(argv[1], "--selftest")) {
        return selftest();
    }
    if (argc >= 2 && !strcmp(argv[1], "--someip")) {
        long sec = argc >= 3 ? strtol(argv[2], nullptr, 10) : kDefaultSeconds;
        if (sec < 10 || sec > kMaxSeconds) {
            fprintf(stderr, "REFUSED: duration 10..%ld s\n", kMaxSeconds);
            return 2;
        }
        return record(sec);
    }
    puts(
        "Usage: evo-capture --someip [seconds] | --selftest\n"
        "Passive SOME/IP recorder (read only, nothing sent, no promiscuous mode), 240 s by default, 900 s at most.\n"
        "File: /tmp/entry-aa-releve/someip-*.pcap (raw IPv4, Wireshark) and a text summary.");
    return argc >= 2 && !strcmp(argv[1], "--help") ? 0 : 2;
}
