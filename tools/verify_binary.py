# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Check that entry-aad suits the car: ARMv7 hard-float, old enough glibc, loads with the car's libraries under QEMU.

PC only, against the copy of the car's system; nothing connects to the car. Writes artifacts/binary-validation.json.
tools/verify_wireless.py uses the same checks for the programs of a release.
"""

import json
import re
import subprocess
from datetime import datetime, timezone
from hashlib import sha256
from pathlib import Path

from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]
SYSROOT = Path.home() / "entrynav2/rootfs"
# The newest symbol versions the car's glibc 2.15 and libstdc++ (GCC 4.7) provide.
LIMITS = (("GLIBC_", (2, 15)), ("GLIBCXX_", (3, 4, 17)))
QEMU = ["qemu-arm", "-L", str(SYSROOT), "-E", "LD_LIBRARY_PATH=/usr/local/lib:/usr/lib:/lib", "-E", "LD_BIND_NOW=1"]


def check_arm_elf(path):
    """ELF facts of an ARM program, after checking it would run on the car. Returns (interpreter, versions, needed)."""
    with path.open("rb") as handle:
        elf = ELFFile(handle)
        if not (elf.elfclass == 32 and elf.little_endian and elf["e_machine"] == "EM_ARM"):
            raise SystemExit(f"FAIL: {path.name} is not a 32-bit little-endian ARM program")
        interpreter = next(s.get_interp_name() for s in elf.iter_segments() if s["p_type"] == "PT_INTERP")
        if interpreter != "/lib/ld-linux-armhf.so.3":
            raise SystemExit(f"FAIL: {path.name} asks for the loader {interpreter}")
        references = elf.get_section_by_name(".gnu.version_r").iter_versions()
        versions = sorted({aux.name for _, entries in references for aux in entries})
        dynamic = elf.get_section_by_name(".dynamic").iter_tags()
        needed = [tag.needed for tag in dynamic if tag.entry.d_tag == "DT_NEEDED"]
    for prefix, limit in LIMITS:
        values = [tuple(map(int, v[len(prefix) :].split("."))) for v in versions if v.startswith(prefix)]
        if values and max(values) > limit:
            raise SystemExit(f"FAIL: {path.name} needs {prefix}{'.'.join(map(str, max(values)))}")
    attributes = subprocess.check_output(["arm-linux-gnueabihf-readelf", "-A", str(path)], text=True)
    if "Tag_ABI_VFP_args: VFP registers" not in attributes or not re.search(r"Tag_CPU_arch: v7\b", attributes):
        raise SystemExit(f"FAIL: {path.name} is not built for ARMv7 hard-float")
    return interpreter, versions, needed


def check_help(path, expected):
    """Run the program's --help under QEMU with the car's libraries, every symbol bound at load time."""
    run = subprocess.run([*QEMU, str(path), "--help"], capture_output=True, text=True, timeout=15)
    if run.returncode != 0 or expected not in run.stdout:
        raise SystemExit(f"FAIL: {path.name} --help under QEMU (exit {run.returncode})\n{run.stdout}{run.stderr}")


def main():
    binary = ROOT / "entry-aad"
    interpreter, versions, needed = check_arm_elf(binary)
    for prefix, _ in LIMITS:
        if not any(v.startswith(prefix) for v in versions):
            raise SystemExit(f"FAIL: entry-aad lists no {prefix} version")
    check_help(binary, "--monitor")
    report = {
        "checked_utc": datetime.now(timezone.utc).isoformat(),
        "binary": binary.name,
        "sha256": sha256(binary.read_bytes()).hexdigest(),
        "bytes": binary.stat().st_size,
        "architecture": "ELF32 ARMv7 EABI5 hard-float (VFP register arguments)",
        "interpreter": interpreter,
        "required_versions": versions,
        "needed_libraries": needed,
        "qemu_help_with_hu_libraries_and_LD_BIND_NOW": "PASS",
        "hardware_video_bluetooth_test": "NOT RUN",
    }
    output = ROOT / "artifacts" / "binary-validation.json"
    output.parent.mkdir(exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n")
    print("PASS ARMv7/hard-float, GLIBC <= 2.15, GLIBCXX <= 3.4.17, chargement QEMU avec libs HU")
    print(f"SHA256 {report['sha256']} ({report['bytes']} octets)")
    print(output)


if __name__ == "__main__":
    main()
