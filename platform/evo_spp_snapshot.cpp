// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Reads the SPP slots of the Alps Bluetooth process through /proc/<pid>/mem, read-only.
#define _LARGEFILE64_SOURCE
#include "evo_spp_snapshot.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "evo_elf.hpp"

namespace evo {
namespace {
// The tables come from the library's own exports, so another build of it is found too. Their sizes pin the layout
// the offsets in read() were established on (BMW G20, software 202111011321). Another build with these sizes is
// accepted, and decode() then cross-checks every read: the two copies of each UUID must agree, or nothing is changed.
const uint32_t kAppSize = 15032, kInfoSize = 480, kConnectionsSize = 36;
bool readLayout(const std::string& path, SppLayout& out, std::string& why) {
    ElfFile elf;
    ElfSymbol app, info, connections;
    if (!elf.load(path)) {
        why = "Alps library unreadable";
        return false;
    }
    if (!elf.symbol("app", app) || !elf.symbol("spp_info", info) || !elf.symbol("spp_connected", connections) ||
        !app.got || !info.got || !connections.got) {
        why = "Alps tables missing from the library";
        return false;
    }
    if (app.size != kAppSize || info.size != kInfoSize || connections.size != kConnectionsSize) {
        char detail[96];
        snprintf(detail, sizeof(detail), "unknown Alps structure (sizes %u/%u/%u)", app.size, info.size,
                 connections.size);
        why = detail;
        return false;
    }
    out.app = app.value;
    out.info = info.value;
    out.connections = connections.value;
    out.appGot = app.got;
    out.infoGot = info.got;
    out.connectionsGot = connections.got;
    return true;
}
std::string startIdentity(int pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    std::ifstream input(path);
    std::string line;
    std::getline(input, line);
    size_t p = line.rfind(')');
    if (p == std::string::npos) {
        return "";
    }
    std::istringstream fields(line.substr(p + 1));
    std::string value;
    for (int i = 3; i <= 22; ++i) {
        if (!(fields >> value)) {
            return "";
        }
    }
    return value;  // monotonic process start ticks, survives command-name spaces
}
}  // namespace
SppSnapshotReader::SppSnapshotReader() : fd_(-1), pid_(0), base_(0) {}
SppSnapshotReader::~SppSnapshotReader() {
    close();
}
void SppSnapshotReader::close() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
    fd_ = -1;
    pid_ = 0;
    base_ = 0;
    identity_.clear();
}
bool SppSnapshotReader::bytes(uint32_t offset, void* data, unsigned size) {
    uint64_t address = static_cast<uint64_t>(base_) + offset;
    return fd_ >= 0 && address + size <= 0x100000000ULL &&
           pread64(fd_, data, size, static_cast<off64_t>(address)) == static_cast<ssize_t>(size);
}
bool SppSnapshotReader::open() {
    if (fd_ >= 0) {
        return false;
    }
    error_ = "Alps library not found or not unique";
    std::string rejected;
    SppLayout found;
    DIR* proc = opendir("/proc");
    if (!proc) {
        return false;
    }
    dirent* item;
    unsigned matches = 0;
    std::string selectedPath;
    while ((item = readdir(proc))) {
        char* end = nullptr;
        long pid = strtol(item->d_name, &end, 10);
        if (!*item->d_name || *end || pid <= 0 || pid > INT_MAX) {
            continue;
        }
        std::string prefix = std::string("/proc/") + item->d_name;
        std::ifstream maps((prefix + "/maps").c_str());
        std::string line;
        while (std::getline(maps, line)) {
            unsigned long begin, finish, offset, inode;
            unsigned major, minor;
            char perms[5];
            int used = 0;
            if (sscanf(line.c_str(), "%lx-%lx %4s %lx %x:%x %lu %n", &begin, &finish, perms, &offset, &major, &minor,
                       &inode, &used) != 7 ||
                offset != 0 || !used) {
                continue;
            }
            std::string path = line.substr(used);
            size_t slash = path.rfind('/');
            if (slash == std::string::npos || path.substr(slash + 1) != "libevo_btappl.so") {
                continue;
            }
            if (begin > 0xffffffffUL) {
                continue;
            }
            std::string why;
            if (!readLayout(prefix + "/root" + path, found, why)) {
                rejected = why;
                continue;
            }
            ++matches;
            pid_ = static_cast<int>(pid);
            base_ = static_cast<uint32_t>(begin);
            selectedPath = prefix;
        }
    }
    closedir(proc);
    if (matches != 1) {
        if (!matches && !rejected.empty()) {
            error_ = rejected;
        }
        return false;
    }
    layout_ = found;
    identity_ = startIdentity(pid_);
    if (identity_.empty()) {
        return false;
    }
    // No ptrace, no pause, no write to a BMW process. O_RDONLY only.
    fd_ = ::open((selectedPath + "/mem").c_str(), O_RDONLY | O_CLOEXEC);
    if (fd_ < 0) {
        error_ = "reading Alps in /proc refused: no Bluetooth change allowed";
        return false;
    }
    uint32_t a, b, c;
    if (!bytes(layout_.appGot, &a, 4) || !bytes(layout_.infoGot, &b, 4) || !bytes(layout_.connectionsGot, &c, 4) ||
        a != base_ + layout_.app || b != base_ + layout_.info || c != base_ + layout_.connections) {
        error_ = "incompatible Alps layout (relocations)";
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    error_.clear();
    return true;
}
bool SppSnapshotReader::decode(uint32_t state, const uint8_t record[34], const uint8_t data[80],
                               const uint8_t connected[6], SppSlotSnapshot& out, bool allowSplit) {
    static const unsigned length[] = {0, 2, 4, 16}, tags[] = {0, 1, 2, 4};
    SppSlotSnapshot parsed;
    if (state > 2 || record[32] < 1 || record[32] > 3 || record[33] < 1 || record[33] > 3) {
        return false;
    }
    for (unsigned i = 0; i < 2; ++i) {
        unsigned type = record[32 + i], dataOffset = i ? 56 : 39, tagOffset = i ? 55 : 38;
        unsigned lowType = data[tagOffset] == 1 ? 1 : data[tagOffset] == 2 ? 2 : data[tagOffset] == 4 ? 3 : 0;
        if (!lowType) {
            return false;
        }
        if (!allowSplit &&
            (data[tagOffset] != tags[type] || memcmp(record + 16 * i, data + dataOffset, length[type]))) {
            return false;
        }
        parsed.uuidPayload.push_back(static_cast<uint8_t>(type));
        parsed.uuidPayload.insert(parsed.uuidPayload.end(), record + 16 * i, record + 16 * i + length[type]);
        parsed.transportUuidPayload.push_back(static_cast<uint8_t>(lowType));
        parsed.transportUuidPayload.insert(parsed.transportUuidPayload.end(), data + dataOffset,
                                           data + dataOffset + length[lowType]);
    }
    parsed.state = state;
    parsed.transportState = data[4];
    for (unsigned i = 0; i < 6; ++i) {
        parsed.connected = parsed.connected || connected[i] != 0;
    }
    out = parsed;
    return true;
}
bool SppSnapshotReader::read(unsigned instance, SppSlotSnapshot& out, bool allowSplit) {
    if (instance > 5 || identity_.empty() || startIdentity(pid_) != identity_) {
        error_ = "invalid instance or Alps process replaced";
        return false;
    }
    SppSlotSnapshot first, second;
    for (int round = 0; round < 2; ++round) {
        uint32_t state;
        uint8_t record[34], data[80], connected[6];
        if (!bytes(layout_.app + 0x514 + instance * 4, &state, 4) ||
            !bytes(layout_.app + 0x64c + instance * 34, record, 34) || !bytes(layout_.info + instance * 80, data, 80) ||
            !bytes(layout_.connections + instance * 6, connected, 6)) {
            error_ = "incomplete read of the Alps tables in /proc";
            return false;
        }
        if (!decode(state, record, data, connected, round ? second : first, allowSplit)) {
            char detail[160];
            snprintf(detail, sizeof(detail),
                     "SPP state or UUID copies incompatible (state=%u transport=%u types=%u/%u tags=%u/%u)", state,
                     data[4], record[32], record[33], data[38], data[55]);
            error_ = detail;
            return false;
        }
        if (!round) {
            usleep(20000);
        }
    }
    if (first.state != second.state || first.transportState != second.transportState ||
        first.connected != second.connected || first.uuidPayload != second.uuidPayload ||
        first.transportUuidPayload != second.transportUuidPayload) {
        error_ = "SPP state changing";
        return false;
    }
    out = second;
    error_.clear();
    return true;
}
}  // namespace evo
