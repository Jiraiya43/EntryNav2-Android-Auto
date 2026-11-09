// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Reads the symbols a shared library of the car exports, from its file. Nothing is loaded or run.

// Every offset of the file is bounds-checked: a damaged or unexpected library is an answer ("no"), not a crash.
#ifndef ENTRY_AAD_PLATFORM_EVO_ELF_HPP
#define ENTRY_AAD_PLATFORM_EVO_ELF_HPP
#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

namespace evo {

struct ElfSymbol {
    uint32_t value;  // address in the library, relative to where it is loaded
    uint32_t size;
    uint32_t got;  // address of the slot the loader fills with this symbol's address; 0 when there is none
    ElfSymbol() : value(0), size(0), got(0) {}
};

class ElfFile {
public:
    ElfFile() : symbols_(), strings_(), relocations_() {}

    bool load(const std::string& path) {
        data_.clear();
        relocations_.clear();
        FILE* file = fopen(path.c_str(), "rb");
        if (!file) {
            return false;
        }
        const size_t kMaxBytes = 64 * 1024 * 1024;
        std::vector<uint8_t> buffer(65536);
        size_t count;
        while ((count = fread(&buffer[0], 1, buffer.size(), file)) != 0 && data_.size() <= kMaxBytes) {
            data_.insert(data_.end(), buffer.begin(), buffer.begin() + count);
        }
        const bool failed = ferror(file) != 0;
        fclose(file);
        return !failed && data_.size() <= kMaxBytes && parse();
    }

    // A symbol this library defines; names it only imports do not count.
    bool symbol(const char* name, ElfSymbol& out) const {
        const size_t wanted = strlen(name) + 1;
        const size_t entries = symbols_.sh_size / sizeof(Elf32_Sym);
        for (size_t i = 0; i < entries; ++i) {
            Elf32_Sym entry;
            if (!copyAt(symbols_.sh_offset + i * sizeof(Elf32_Sym), &entry, sizeof(entry)) ||
                entry.st_shndx == SHN_UNDEF || entry.st_name >= strings_.sh_size ||
                wanted > strings_.sh_size - entry.st_name ||
                memcmp(&data_[strings_.sh_offset + entry.st_name], name, wanted) != 0) {
                continue;
            }
            out = ElfSymbol();
            out.value = entry.st_value;
            out.size = entry.st_size;
            out.got = gotSlot(static_cast<uint32_t>(i));
            return true;
        }
        return false;
    }

private:
    std::vector<uint8_t> data_;
    Elf32_Shdr symbols_, strings_;
    std::vector<Elf32_Shdr> relocations_;

    bool copyAt(uint64_t offset, void* out, size_t size) const {
        if (offset > data_.size() || size > data_.size() - offset) {
            return false;
        }
        memcpy(out, &data_[offset], size);
        return true;
    }
    bool inFile(const Elf32_Shdr& section) const {
        return section.sh_offset <= data_.size() && section.sh_size <= data_.size() - section.sh_offset;
    }
    bool parse() {
        Elf32_Ehdr header;
        if (!copyAt(0, &header, sizeof(header)) || memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
            header.e_ident[EI_CLASS] != ELFCLASS32 || header.e_ident[EI_DATA] != ELFDATA2LSB ||
            header.e_machine != EM_ARM || header.e_shentsize != sizeof(Elf32_Shdr)) {
            return false;
        }
        bool found = false;
        for (unsigned i = 0; i < header.e_shnum; ++i) {
            Elf32_Shdr section;
            if (!copyAt(header.e_shoff + uint64_t(i) * sizeof(section), &section, sizeof(section))) {
                return false;
            }
            if (section.sh_type == SHT_REL && inFile(section)) {
                relocations_.push_back(section);
            } else if (section.sh_type == SHT_DYNSYM) {
                symbols_ = section;
                found =
                    inFile(section) && section.sh_link < header.e_shnum &&
                    copyAt(header.e_shoff + uint64_t(section.sh_link) * sizeof(section), &strings_, sizeof(strings_)) &&
                    strings_.sh_type == SHT_STRTAB && inFile(strings_);
            }
        }
        return found;
    }
    uint32_t gotSlot(uint32_t index) const {
        for (size_t s = 0; s < relocations_.size(); ++s) {
            const size_t entries = relocations_[s].sh_size / sizeof(Elf32_Rel);
            for (size_t i = 0; i < entries; ++i) {
                Elf32_Rel entry;
                if (copyAt(relocations_[s].sh_offset + i * sizeof(Elf32_Rel), &entry, sizeof(entry)) &&
                    ELF32_R_SYM(entry.r_info) == index && ELF32_R_TYPE(entry.r_info) == R_ARM_GLOB_DAT) {
                    return entry.r_offset;
                }
            }
        }
        return 0;
    }
};

}  // namespace evo
#endif
