// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// ELF symbol reader on a small hand-made ELF, cut and damaged in every way, then on a copy of the car's library.

// The car's library is read only when ENTRY_AAD_FIXTURE_LIB names a copy. Runs on the PC and on ARM under QEMU.
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "evo_elf.hpp"

namespace {

std::string g_path;

void writeFile(const std::vector<uint8_t>& bytes) {
    FILE* file = fopen(g_path.c_str(), "wb");
    assert(file);
    if (!bytes.empty()) {
        assert(fwrite(&bytes[0], 1, bytes.size(), file) == bytes.size());
    }
    fclose(file);
}

template <typename T>
void append(std::vector<uint8_t>& out, const T& value) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&value);
    out.insert(out.end(), p, p + sizeof(T));
}

// Header, then .dynstr, .dynsym, .rel.dyn, then the section table: null, dynsym, dynstr, rel.
std::vector<uint8_t> sampleElf() {
    const char strings[] = "\0app\0imported\0";
    const uint32_t stringsAt = sizeof(Elf32_Ehdr);
    const uint32_t symbolsAt = stringsAt + sizeof(strings);
    const uint32_t relAt = symbolsAt + 3 * sizeof(Elf32_Sym);
    const uint32_t sectionsAt = relAt + sizeof(Elf32_Rel) * 2;

    Elf32_Ehdr header;
    memset(&header, 0, sizeof(header));
    memcpy(header.e_ident, ELFMAG, SELFMAG);
    header.e_ident[EI_CLASS] = ELFCLASS32;
    header.e_ident[EI_DATA] = ELFDATA2LSB;
    header.e_machine = EM_ARM;
    header.e_shoff = sectionsAt;
    header.e_shentsize = sizeof(Elf32_Shdr);
    header.e_shnum = 4;

    std::vector<uint8_t> out;
    append(out, header);
    out.insert(out.end(), strings, strings + sizeof(strings));

    Elf32_Sym none, app, imported;
    memset(&none, 0, sizeof(none));
    memset(&app, 0, sizeof(app));
    memset(&imported, 0, sizeof(imported));
    app.st_name = 1;
    app.st_value = 0xb42f88;
    app.st_size = 15032;
    app.st_shndx = 22;
    imported.st_name = 5;
    imported.st_shndx = SHN_UNDEF;
    append(out, none);
    append(out, app);
    append(out, imported);

    Elf32_Rel other, slot;
    other.r_offset = 0x1000;
    other.r_info = ELF32_R_INFO(1, R_ARM_RELATIVE);
    slot.r_offset = 0x133ae8;
    slot.r_info = ELF32_R_INFO(1, R_ARM_GLOB_DAT);
    append(out, other);
    append(out, slot);

    Elf32_Shdr section;
    memset(&section, 0, sizeof(section));
    append(out, section);
    section.sh_type = SHT_DYNSYM;
    section.sh_offset = symbolsAt;
    section.sh_size = 3 * sizeof(Elf32_Sym);
    section.sh_link = 2;
    append(out, section);
    section.sh_type = SHT_STRTAB;
    section.sh_offset = stringsAt;
    section.sh_size = sizeof(strings);
    section.sh_link = 0;
    append(out, section);
    section.sh_type = SHT_REL;
    section.sh_offset = relAt;
    section.sh_size = 2 * sizeof(Elf32_Rel);
    append(out, section);
    return out;
}

void realLibrary() {
    const char* library = getenv("ENTRY_AAD_FIXTURE_LIB");
    if (!library) {
        return;
    }
    evo::ElfFile elf;
    evo::ElfSymbol app, info, connected, missing;
    assert(elf.load(library));
    assert(elf.symbol("app", app) && app.value == 0xb42f88 && app.size == 15032 && app.got == 0x133ae8);
    assert(elf.symbol("spp_info", info) && info.value == 0xb4a3fc && info.size == 480 && info.got == 0x133b7c);
    assert(elf.symbol("spp_connected", connected) && connected.value == 0xb4a654 && connected.size == 36 &&
           connected.got == 0x133e30);
    assert(!elf.symbol("spp_connecte", missing) && !elf.symbol("nothing_like_this", missing));
}

}  // namespace

int main() {
    char path[] = "/tmp/entry-aad-elf-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    g_path = path;

    const std::vector<uint8_t> good = sampleElf();
    evo::ElfFile elf;
    evo::ElfSymbol symbol;
    writeFile(good);
    assert(elf.load(g_path));
    assert(elf.symbol("app", symbol));
    assert(symbol.value == 0xb42f88 && symbol.size == 15032 && symbol.got == 0x133ae8);
    assert(!elf.symbol("ap", symbol));        // the whole name, not its start
    assert(!elf.symbol("imported", symbol));  // only imported here
    assert(!elf.symbol("absent", symbol));

    // Cut anywhere: refused or read within the file, never a crash.
    for (size_t size = 0; size < good.size(); ++size) {
        writeFile(std::vector<uint8_t>(good.begin(), good.begin() + size));
        evo::ElfFile cut;
        if (cut.load(g_path)) {
            cut.symbol("app", symbol);
        }
    }
    // Any byte damaged, with the worst values: same.
    for (size_t at = 0; at < good.size(); ++at) {
        for (int value = 0; value < 2; ++value) {
            std::vector<uint8_t> bad = good;
            bad[at] = value ? 0xff : 0x00;
            writeFile(bad);
            evo::ElfFile damaged;
            if (damaged.load(g_path)) {
                damaged.symbol("app", symbol);
            }
        }
    }
    // Not for this processor, or not ELF at all.
    std::vector<uint8_t> other = good;
    other[offsetof(Elf32_Ehdr, e_machine)] = EM_386;
    writeFile(other);
    assert(!elf.load(g_path));
    other = good;
    other[0] = 'X';
    writeFile(other);
    assert(!elf.load(g_path));
    assert(!elf.load("/nonexistent/library.so"));

    realLibrary();
    unlink(path);
    puts("PASS lecture des symboles d'une bibliotheque : fichiers coupes ou abimes refuses sans plantage");
    return 0;
}
