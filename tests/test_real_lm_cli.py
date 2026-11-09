# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Exercise the car's own LayerManagerControl parser under QEMU, every ILM call it makes replaced by a stub.

Needs tests/lm-cli-stub.so (make test-lm-cli-arm-build) and the copy of the car's system. tests/run_qemu.py runs it.
"""

import json
import subprocess
from hashlib import sha256
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SYSROOT = Path.home() / "entrynav2/rootfs"
CLI = [
    "qemu-arm",
    "-L",
    str(SYSROOT),
    "-E",
    "LD_LIBRARY_PATH=/usr/local/lib:/usr/lib:/lib",
    "-E",
    "LD_PRELOAD=" + str(ROOT / "tests/lm-cli-stub.so"),
    str(SYSROOT / "usr/bin/LayerManagerControl"),
]
# Render order arguments and the order the stub then receives (None: the CLI refuses a space-separated list).
ORDERS = [
    (["0x0", "0x7eaa0001"], None),
    (["0x0,0x7eaa0001"], "count=2 0 7eaa0001"),
    (["0x7eaa0001,0x0"], "count=2 7eaa0001 0"),
    (["0x101,0x102,0x7eaa0001"], "count=3 101 102 7eaa0001"),
    (["0x0"], "count=1 0"),
    ([], "count=0"),
]


def cli(*args):
    return subprocess.run([*CLI, *args], capture_output=True, text=True, timeout=10)


def main():
    for args, expected in ORDERS:
        r = cli("set", "layer", "0x80000000", "render", "order", *args)
        assert r.returncode == 0, (args, r.stdout, r.stderr)
        if expected is None:
            assert "Interpreter error:" in r.stderr and "FIXTURE" not in r.stdout
        else:
            assert not r.stderr and f"FIXTURE order 80000000 {expected}\n" in r.stdout, (args, r.stdout, r.stderr)
    for region in ["source", "destination"]:
        for height in [352, 480]:
            r = cli("set", "surface", "0x7eaa0001", region, "region", "0", "0", "1280", str(height))
            assert r.returncode == 0 and not r.stderr, (r.stdout, r.stderr)
            assert f"FIXTURE {region} 7eaa0001 0 0 1280 {height}\n" in r.stdout, r.stdout
    files = ["platform/evo_ilm.cpp", "tests/test_real_lm_cli.py", "tests/lm_cli_stub.c", "tests/lm-cli-stub.so"]
    report = {
        "hardware_test": "NOT RUN",
        "real_parser": "PASS",
        "volume_crop_restore_rectangles": "PASS",
        "old_space_list_rejected_with_exit_zero": True,
        "comma_list_multi_single_empty": "PASS",
        "test_surface_first_order_preserved": "PASS",
        "cli_sha256": sha256((SYSROOT / "usr/bin/LayerManagerControl").read_bytes()).hexdigest(),
        "files": {name: sha256((ROOT / name).read_bytes()).hexdigest() for name in files},
    }
    (ROOT / "artifacts").mkdir(exist_ok=True)
    (ROOT / "artifacts/lm-cli-validation.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        "PASS REAL HU LayerManagerControl: space-separated list refused (stderr, exit 0); comma-separated "
        "multi/single/empty orders captured; no compositor"
    )


if __name__ == "__main__":
    main()
