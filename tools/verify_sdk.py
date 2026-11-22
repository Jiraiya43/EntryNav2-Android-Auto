# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Statically check the isolated SDK outputs; this is not car-library or release validation."""

import argparse
import subprocess
from pathlib import Path

from elftools.elf.elffile import ELFFile
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
    "aa-supervisor": COMMON | {"libdbus-1.so.3", "librt.so.1"},
    "hmiapp/libEntryAad.so": COMMON | {"libAppFramework.A.so", "libCoreFramework.A.so"},
}
VENDOR_LIBRARIES = {
    "entry-aad": {"libxsvideo.so.4"},
    "aa-supervisor": set(),
    "hmiapp/libEntryAad.so": {"libAppFramework.A.so", "libCoreFramework.A.so"},
}


def manifest_symbols(path):
    symbols = {}
    for line in path.read_text().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        lib, kind, name = line.split()
        if kind != "FUNC" or name in symbols:
            raise SystemExit(f"FAIL: invalid or duplicate SDK symbol: {line}")
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
