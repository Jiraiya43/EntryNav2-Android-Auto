// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Read-only observation of the car's Alps Bluetooth library, found through its exported tables.
#ifndef ENTRY_AAD_PLATFORM_EVO_SPP_SNAPSHOT_HPP
#define ENTRY_AAD_PLATFORM_EVO_SPP_SNAPSHOT_HPP
#include <stdint.h>

#include <string>
#include <vector>
namespace evo {
struct SppSlotSnapshot {
    uint32_t state;
    uint8_t transportState;
    bool connected;
    std::vector<uint8_t> uuidPayload;
    std::vector<uint8_t> transportUuidPayload;
    SppSlotSnapshot() : state(0), transportState(0), connected(false) {}
    bool idle() const { return state == 1 && transportState == 0 && !connected; }
};
struct SppLayout {
    uint32_t app, info, connections;           // tables, relative to the library's load address
    uint32_t appGot, infoGot, connectionsGot;  // slots the loader filled with their addresses
    SppLayout() : app(0), info(0), connections(0), appGot(0), infoGot(0), connectionsGot(0) {}
};
class SppSnapshotReader {
public:
    SppSnapshotReader(const SppSnapshotReader&) = delete;
    SppSnapshotReader& operator=(const SppSnapshotReader&) = delete;
    SppSnapshotReader();
    ~SppSnapshotReader();
    bool open();
    void close();  // forget the Alps process (open() rescans: retries, restarts)
    bool read(unsigned instance, SppSlotSnapshot& out, bool allowSplit = false);
    int pid() const { return pid_; }
    const std::string& identity() const { return identity_; }
    const std::string& error() const { return error_; }
    // Pure decoder, tested using byte layouts and corroborating copies of the UUIDs.
    static bool decode(uint32_t state, const uint8_t record[34], const uint8_t info[80], const uint8_t connected[6],
                       SppSlotSnapshot& out, bool allowSplit = false);

private:
    int fd_, pid_;
    uint32_t base_;
    SppLayout layout_;
    std::string identity_, error_;
    bool bytes(uint32_t offset, void* data, unsigned size);
};
}  // namespace evo
#endif
