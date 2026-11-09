// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Read-only Alps Bluetooth state reader against a child process mapping a copy of the car's library.

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "evo_spp_snapshot.hpp"

#ifdef MAP_32BIT
static void readerFixture(const char* library, bool accepted) {
    int control[2], ready[2];
    assert(pipe(control) == 0 && pipe(ready) == 0);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        close(control[1]);
        close(ready[0]);
        uint8_t* base = static_cast<uint8_t*>(
            mmap(0, 0xc00000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0));
        assert(base != MAP_FAILED);
        int fd = open(library, O_RDONLY);
        struct stat st;
        assert(fd >= 0 && fstat(fd, &st) == 0);
        assert(mmap(base, st.st_size, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, 0) == base);
        close(fd);
        const uint32_t address = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base));
        *reinterpret_cast<uint32_t*>(base + 0x133ae8) = address + 0xb42f88;
        *reinterpret_cast<uint32_t*>(base + 0x133b7c) = address + 0xb4a3fc;
        *reinterpret_cast<uint32_t*>(base + 0x133e30) = address + 0xb4a654;
        *reinterpret_cast<uint32_t*>(base + 0xb42f88 + 0x514) = 1;
        uint8_t* record = base + 0xb42f88 + 0x64c;
        uint8_t* info = base + 0xb4a3fc;
        record[0] = record[16] = info[39] = info[56] = 0x11;
        record[1] = record[17] = info[40] = info[57] = record[32] = record[33] = info[38] = info[55] = 1;
        assert(write(ready[1], "R", 1) == 1);
        char cmd;
        while (read(control[0], &cmd, 1) == 1) {
            if (cmd == 'X') {
                break;
            }
            if (cmd == 'B') {
                info[39] = 0xff;
            }
            assert(write(ready[1], "R", 1) == 1);
        }
        _exit(0);
    }
    close(control[0]);
    close(ready[1]);
    char value;
    assert(read(ready[0], &value, 1) == 1);
    evo::SppSnapshotReader reader;
    if (!accepted) {
        assert(!reader.open() && reader.error().find("unknown Alps structure") != std::string::npos);
        assert(write(control[1], "X", 1) == 1);
        int refused;
        assert(waitpid(child, &refused, 0) == child && WEXITSTATUS(refused) == 0);
        close(control[1]);
        close(ready[0]);
        return;
    }
    assert(reader.open() && reader.pid() == child);
    evo::SppSlotSnapshot slot;
    assert(reader.read(0, slot) && slot.idle());
    // The service retries its preflight: an open reader refuses open(), close() allows a fresh rescan.
    assert(!reader.open());
    reader.close();
    assert(reader.pid() == 0 && !reader.read(0, slot));
    assert(reader.open() && reader.pid() == child && reader.read(0, slot) && slot.idle());
    assert(!reader.read(6, slot));
    assert(write(control[1], "B", 1) == 1 && read(ready[0], &value, 1) == 1);
    assert(!reader.read(0, slot));  // disagreement between actual mapped copies is rejected
    assert(write(control[1], "X", 1) == 1);
    int status;
    assert(waitpid(child, &status, 0) == child && WEXITSTATUS(status) == 0);
    assert(!reader.read(0, slot));  // no stale access after process disappearance
    close(control[1]);
    close(ready[0]);
}
// A copy under the same name, changed by `change`, as another build of the library on another car.
static std::string libraryCopy(const char* library, const char* folder, void (*change)(std::vector<char>&)) {
    std::vector<char> bytes;
    FILE* in = fopen(library, "rb");
    assert(in);
    char block[65536];
    size_t count;
    while ((count = fread(block, 1, sizeof(block), in)) != 0) {
        bytes.insert(bytes.end(), block, block + count);
    }
    fclose(in);
    change(bytes);
    std::string path = std::string(folder) + "/libevo_btappl.so";
    FILE* out = fopen(path.c_str(), "wb");
    assert(out && fwrite(&bytes[0], 1, bytes.size(), out) == bytes.size());
    fclose(out);
    return path;
}
static void otherBuild(std::vector<char>& bytes) {
    bytes[bytes.size() - 1] ^= 1;  // the file differs, the tables do not
}
static void otherLayout(std::vector<char>& bytes) {
    // `app`: value 0xb42f88, size 15032 in each symbol table. The size grows: its fields may have moved.
    const char entry[] = {'\x88', '\x2f', '\xb4', '\x00', '\xb8', '\x3a', '\x00', '\x00'};
    unsigned changed = 0;
    for (size_t i = 0; i + sizeof(entry) <= bytes.size(); ++i) {
        if (!memcmp(&bytes[i], entry, sizeof(entry))) {
            bytes[i + 4] = '\xbc';
            ++changed;
        }
    }
    assert(changed >= 1);
}
static void realLibrary() {
    const char* library = getenv("ENTRY_AAD_FIXTURE_LIB");
    if (!library) {
        return;
    }
    readerFixture(library, true);
    char folder[] = "/tmp/entry-aad-alps-XXXXXX";
    assert(mkdtemp(folder));
    std::string copy = libraryCopy(library, folder, otherBuild);
    readerFixture(copy.c_str(), true);
    copy = libraryCopy(library, folder, otherLayout);
    readerFixture(copy.c_str(), false);
    assert(unlink(copy.c_str()) == 0 && rmdir(folder) == 0);
    puts(
        "PASS real /proc reader: exported tables, sizes, load bias, GOT pointers, UUID copies, process lifetime; "
        "another "
        "build accepted, another layout refused; child fixture only");
}
#else
static void realLibrary() {}
#endif
int main() {
    uint8_t record[34] = {}, info[80] = {}, connected[6] = {};
    record[0] = record[16] = info[39] = info[56] = 0x11;
    record[1] = record[17] = info[40] = info[57] = 1;
    record[32] = record[33] = info[38] = info[55] = 1;
    evo::SppSlotSnapshot s;
    assert(evo::SppSnapshotReader::decode(1, record, info, connected, s) && s.idle());
    const uint8_t generic[] = {1, 0x11, 1, 1, 0x11, 1};
    assert(s.uuidPayload == std::vector<uint8_t>(generic, generic + 6));
    assert(evo::SppSnapshotReader::decode(2, record, info, connected, s) && !s.idle());
    connected[0] = 1;
    assert(evo::SppSnapshotReader::decode(1, record, info, connected, s) && !s.idle());
    connected[0] = 0;
    info[4] = 3;
    assert(evo::SppSnapshotReader::decode(1, record, info, connected, s) && !s.idle());
    info[4] = 0;
    info[39] = 0x22;
    assert(!evo::SppSnapshotReader::decode(1, record, info, connected, s));
    assert(evo::SppSnapshotReader::decode(1, record, info, connected, s, true));
    assert(s.uuidPayload != s.transportUuidPayload);  // recovery must check BOTH against saved/AA values
    for (unsigned i = 0; i < 32; ++i) {
        record[i] = static_cast<uint8_t>(i);
    }
    record[32] = record[33] = 3;
    info[38] = info[55] = 4;
    memcpy(info + 39, record, 16);
    memcpy(info + 56, record + 16, 16);
    assert(evo::SppSnapshotReader::decode(1, record, info, connected, s));
    assert(s.uuidPayload.size() == 34 && s.uuidPayload[0] == 3 && s.uuidPayload[17] == 3 && s.uuidPayload[33] == 31);
    info[55] = 3;
    assert(!evo::SppSnapshotReader::decode(1, record, info, connected, s));
    record[32] = 255;
    assert(!evo::SppSnapshotReader::decode(1, record, info, connected, s));
    puts("PASS snapshot: exact UUID pair, corroboration, busy/transition/invalid states rejected");
    realLibrary();
}
