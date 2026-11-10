# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Check the ARM programs of a release against the copy of the car's libraries, under QEMU; nothing connects to the car.

Writes artifacts/wireless-validation.json, whose fingerprints the packaging requires: a program rebuilt since then
must be checked again. The developer tools (evo-check, evo-capture) are not in a release: tests/run_qemu.py runs them.
"""

import json
import sys
from datetime import datetime, timezone
from hashlib import sha256
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from verify_binary import check_arm_elf, check_help  # noqa: E402

# Each released program, with a word its --help must print.
RELEASE_PROGRAMS = (("entry-aad", "--run"), ("aa-supervisor", "--service"))


def main():
    report = {"checked_utc": datetime.now(timezone.utc).isoformat(), "hardware_test": "NOT RUN", "binaries": {}}
    for name, expected in RELEASE_PROGRAMS:
        path = ROOT / name
        _, versions, _ = check_arm_elf(path)
        check_help(path, expected)
        report["binaries"][name] = {
            "sha256": sha256(path.read_bytes()).hexdigest(),
            "bytes": path.stat().st_size,
            "required_versions": versions,
            "qemu_hu_libraries_LD_BIND_NOW": "PASS",
            "architecture": "ARMv7 hard-float",
        }
    (ROOT / "artifacts").mkdir(exist_ok=True)
    (ROOT / "artifacts/wireless-validation.json").write_text(json.dumps(report, indent=2) + "\n")
    print("PASS release programs: ARM ABI and eager loading with the copied HU libraries; hardware NOT RUN")


if __name__ == "__main__":
    main()
