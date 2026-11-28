# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Statically check the isolated SDK outputs; this is not car-library or release validation."""

import argparse
import re
import struct
import subprocess
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from verify_binary import check_arm_elf

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "artifacts/sdk"
COMMON = {"libpthread.so.0", "libdl.so.2", "libstdc++.so.6", "libgcc_s.so.1", "libc.so.6", "ld-linux-armhf.so.3"}
NEEDED = {
    "entry-aad": COMMON
    | {
        "libxsvideo.so.4",
        "libdrm_omap.so.1",
        "libdrm.so.2",
        "librt.so.1",
        "libssl.so.1.0.0",
        "libcrypto.so.1.0.0",
        "libdbus-1.so.3",
        "libm.so.6",
    },
    "aa-supervisor": COMMON | {"libcrypto.so.1.0.0", "libdbus-1.so.3", "librt.so.1"},
    "hmiapp/libEntryAad.so": COMMON | {"libAppFramework.A.so", "libCoreFramework.A.so", "librt.so.1"},
}
VENDOR_LIBRARIES = {
    "entry-aad": {"libxsvideo.so.4"},
    "aa-supervisor": set(),
    "hmiapp/libEntryAad.so": {"libAppFramework.A.so", "libCoreFramework.A.so"},
}


def manifest_symbols(path):
    symbols = {}
    libraries = set().union(*VENDOR_LIBRARIES.values())
    for number, line in enumerate(path.read_text().splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        fields = line.split()
        if len(fields) != 3:
            raise SystemExit(f"FAIL: {path}:{number}: invalid SDK symbol: {line}")
        lib, kind, name = fields
        if (
            lib not in libraries
            or kind != "FUNC"
            or not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", name)
            or name in symbols
        ):
            raise SystemExit(f"FAIL: {path}:{number}: invalid or duplicate SDK symbol: {line}")
        symbols[name] = lib
    return symbols


def dynamic_symbols(path):
    output = subprocess.check_output(["readelf", "--wide", "--dyn-syms", str(path)], text=True)
    imports, exports = set(), set()
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 8 or not fields[0].endswith(":"):
            continue
        name = fields[7]
        if fields[6] == "UND" and fields[4] == "GLOBAL":
            imports.add(name)
        elif fields[6] != "UND" and fields[4] == "GLOBAL" and fields[5] == "DEFAULT":
            exports.add(name)
    return imports, exports


def check_contract(name, needed, paths, imports, exports, symbols):
    if set(needed) != NEEDED[name] or len(needed) != len(set(needed)):
        raise SystemExit(f"FAIL: {name} has unexpected DT_NEEDED: {sorted(needed)}")
    if paths:
        raise SystemExit(f"FAIL: {name} has RPATH or RUNPATH: {paths}")
    expected = {symbol for symbol, lib in symbols.items() if lib in VENDOR_LIBRARIES[name]}
    imported = {symbol.split("@")[0] for symbol in imports}
    actual = {symbol for symbol in imported if symbol in symbols or symbol.startswith(("xsv_", "_ZN3bmw", "_ZNK3bmw"))}
    if any("@" in symbol and symbol.split("@")[0] in expected for symbol in imports):
        raise SystemExit(f"FAIL: {name} has a versioned proprietary import")
    if expected != actual:
        raise SystemExit(
            f"FAIL: {name} vendor imports differ: missing={sorted(expected - actual)}, "
            f"unexpected={sorted(actual - expected)}"
        )
    if name == "hmiapp/libEntryAad.so" and "pluginAppInfo" not in exports:
        raise SystemExit("FAIL: pluginAppInfo is not exported")


def check_plugin_info(path):
    """Inspect the ARM plugin descriptor without loading the plugin or the SDK stubs."""
    with path.open("rb") as handle:
        elf = ELFFile(handle)
        if not (elf.elfclass == 32 and elf.little_endian and elf["e_machine"] == "EM_ARM"):
            raise SystemExit("FAIL: pluginAppInfo requires a 32-bit little-endian ARM ELF")
        dynsym = elf.get_section_by_name(".dynsym")
        symbols = [] if dynsym is None else dynsym.get_symbol_by_name("pluginAppInfo") or []
        symbols = [
            symbol
            for symbol in symbols
            if symbol["st_shndx"] != "SHN_UNDEF"
            and symbol["st_info"]["bind"] == "STB_GLOBAL"
            and symbol["st_other"]["visibility"] == "STV_DEFAULT"
        ]
        if len(symbols) != 1 or symbols[0]["st_info"]["type"] != "STT_OBJECT" or symbols[0]["st_size"] != 24:
            raise SystemExit("FAIL: pluginAppInfo must be one exported 24-byte object")

        def file_bytes(address, size, field, executable=False):
            for segment in elf.iter_segments():
                start = segment["p_vaddr"]
                if (
                    segment["p_type"] == "PT_LOAD"
                    and segment["p_flags"] & 4
                    and (not executable or segment["p_flags"] & 1)
                    and start <= address
                    and address + size <= start + segment["p_filesz"]
                ):
                    handle.seek(segment["p_offset"] + address - start)
                    data = handle.read(size)
                    if len(data) == size:
                        return data
            raise SystemExit(f"FAIL: pluginAppInfo {field} is not in a suitable file-backed segment")

        address = symbols[0]["st_value"]
        data = file_bytes(address, 24, "descriptor")
        major, minor, name, vendor, rhmi, create, destroy = struct.unpack("<HHIIIII", data)
        if (major, minor) != (1, 0):
            raise SystemExit("FAIL: pluginAppInfo ABI version is not 1.0")
        if rhmi != 1:
            raise SystemExit("FAIL: pluginAppInfo RHMI interface version is not 1")

        dynamic = elf.get_section_by_name(".dynamic")
        tags = {} if dynamic is None else {tag.entry.d_tag: tag.entry.d_val for tag in dynamic.iter_tags()}
        relocations = {}
        for section in elf.iter_sections():
            if not isinstance(section, RelocationSection):
                continue
            kind = "RELA" if section.is_RELA() else "REL"
            if section["sh_addr"] != tags.get("DT_" + kind) or section["sh_size"] != tags.get("DT_" + kind + "SZ"):
                continue
            for relocation in section.iter_relocations():
                relocations.setdefault(relocation["r_offset"], []).append(relocation)

        def pointer(offset, value, field):
            matches = relocations.get(address + offset, [])
            if len(matches) != 1 or matches[0]["r_info_type"] != 23 or matches[0]["r_info_sym"] != 0:
                raise SystemExit(f"FAIL: pluginAppInfo {field} needs one R_ARM_RELATIVE relocation")
            relocation = matches[0]
            return relocation["r_addend"] if relocation.is_RELA() else value

        name = pointer(4, name, "name")
        vendor = pointer(8, vendor, "vendor")
        create = pointer(16, create, "create")
        destroy = pointer(20, destroy, "destroy")
        for field, value, expected in (("name", name, b"Android Auto\0"), ("vendor", vendor, b"EntryAad\0")):
            if not value or file_bytes(value, len(expected), field) != expected:
                raise SystemExit(f"FAIL: pluginAppInfo {field} has unexpected text")
        for field, value in (("create", create), ("destroy", destroy)):
            target = value & ~1
            if not target:
                raise SystemExit(f"FAIL: pluginAppInfo {field} callback is null")
            file_bytes(target, 1, field + " callback", executable=True)
            if not any(
                section["sh_flags"] & 4
                and section["sh_type"] != "SHT_NOBITS"
                and section["sh_addr"] <= target < section["sh_addr"] + section["sh_size"]
                for section in elf.iter_sections()
            ):
                raise SystemExit(f"FAIL: pluginAppInfo {field} callback is not in executable code")
        if (create & ~1) == (destroy & ~1):
            raise SystemExit("FAIL: pluginAppInfo create and destroy callbacks are identical")


def inspect(path, shared):
    interpreter, versions, needed = check_arm_elf(path, shared=shared, readelf="readelf")
    with path.open("rb") as handle:
        elf = ELFFile(handle)
        paths = [
            tag.entry.d_tag
            for tag in elf.get_section_by_name(".dynamic").iter_tags()
            if tag.entry.d_tag in ("DT_RPATH", "DT_RUNPATH")
        ]
    imports, exports = dynamic_symbols(path)
    return interpreter, versions, needed, paths, imports, exports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--compare-reference",
        nargs="?",
        const=ROOT,
        type=Path,
        metavar="DIR",
        help="compare with firmware outputs in DIR (default: repository root)",
    )
    args = parser.parse_args()
    symbols = manifest_symbols(ROOT / "toolchain/sdk.symbols")
    for name in NEEDED:
        sdk = inspect(SDK / name, shared=name.endswith(".so"))
        _, versions, needed, paths, imports, exports = sdk
        check_contract(name, needed, paths, imports, exports, symbols)
        if name == "hmiapp/libEntryAad.so":
            check_plugin_info(SDK / name)
        if not any(v.startswith("GLIBC_") for v in versions) or not any(v.startswith("GLIBCXX_") for v in versions):
            raise SystemExit(f"FAIL: {name} is missing GLIBC or GLIBCXX version requirements")
        if args.compare_reference:
            reference = inspect(args.compare_reference / name, shared=name.endswith(".so"))
            labels = ("interpreter", "versions", "DT_NEEDED", "paths", "imports", "exports")
            for label, actual, expected in zip(labels, sdk, reference):
                if actual != expected:
                    raise SystemExit(
                        f"FAIL: {name} {label} differs from firmware build: SDK={actual} reference={expected}"
                    )
        print(f"PASS SDK ELF {name}" + (" (matches firmware build)" if args.compare_reference else ""))
    print("Static checks only: real-library QEMU and car validation have not been performed.")


if __name__ == "__main__":
    main()
