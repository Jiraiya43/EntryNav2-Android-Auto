# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post-build regression tests for the real, never-executed SDK plugin ELF."""

import struct
import sys
import tempfile
import unittest
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_sdk import SDK, check_plugin_info  # noqa: E402


class SdkPluginArtifactTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = SDK / "hmiapp/libEntryAad.so"
        if not cls.path.is_file():
            raise AssertionError(f"SDK plugin not built: {cls.path}")
        cls.original = cls.path.read_bytes()
        with cls.path.open("rb") as handle:
            elf = ELFFile(handle)
            dynsym = elf.get_section_by_name(".dynsym")
            symbol_index, symbol = next(
                (index, symbol) for index, symbol in enumerate(dynsym.iter_symbols()) if symbol.name == "pluginAppInfo"
            )
            cls.symbol_offset = dynsym["sh_offset"] + symbol_index * dynsym["sh_entsize"]
            address = symbol["st_value"]
            segment = next(
                segment
                for segment in elf.iter_segments()
                if segment["p_type"] == "PT_LOAD"
                and segment["p_vaddr"] <= address
                and address + 24 <= segment["p_vaddr"] + segment["p_filesz"]
            )
            cls.descriptor_offset = segment["p_offset"] + address - segment["p_vaddr"]
            rel = next(
                section
                for section in elf.iter_sections()
                if isinstance(section, RelocationSection) and not section.is_RELA() and section.name == ".rel.dyn"
            )
            cls.create_relocation_offset = next(
                rel["sh_offset"] + index * rel["sh_entsize"]
                for index, relocation in enumerate(rel.iter_relocations())
                if relocation["r_offset"] == address + 16
            )

    def test_valid_plugin_descriptor(self):
        check_plugin_info(self.path)

    def test_rejects_descriptor_mutations(self):
        offset = self.descriptor_offset
        create = struct.unpack_from("<I", self.original, offset + 16)[0]
        vendor = struct.unpack_from("<I", self.original, offset + 8)[0]
        mutations = (
            ("object size", self.symbol_offset + 8, "<I", 20, "exported 24-byte object"),
            ("ABI version", offset, "<H", 2, "ABI version"),
            ("RHMI version", offset + 12, "<I", 2, "RHMI interface version"),
            ("name pointer", offset + 4, "<I", 0xFFFFFFFF, "name"),
            ("name text", offset + 4, "<I", vendor, "name has unexpected text"),
            ("vendor pointer", offset + 8, "<I", 0, "vendor"),
            ("null callback", offset + 16, "<I", 0, "create callback"),
            ("Thumb null callback", offset + 16, "<I", 1, "create callback"),
            ("invalid callback", offset + 20, "<I", 0xFFFFFFFF, "destroy callback"),
            ("non-code callback", offset + 20, "<I", vendor, "destroy callback.*executable"),
            ("identical callbacks", offset + 20, "<I", create, "callbacks are identical"),
            ("missing relocation", self.create_relocation_offset, "<I", 0, "create.*R_ARM_RELATIVE"),
            ("unsupported relocation", self.create_relocation_offset + 4, "<I", 0, "create.*R_ARM_RELATIVE"),
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "libEntryAad.so"
            for case, position, fmt, value, error in mutations:
                with self.subTest(case=case):
                    mutated = bytearray(self.original)
                    struct.pack_into(fmt, mutated, position, value)
                    path.write_bytes(mutated)
                    with self.assertRaisesRegex(SystemExit, error):
                        check_plugin_info(path)


if __name__ == "__main__":
    unittest.main()
