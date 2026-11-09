# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Compare actual C++ GPS messages with pinned aasdk protobuf via Google's protoc/runtime.

Requires protoc + python protobuf on the PC. No vehicle or telephone connection.
Run after make test-gps-host. Add --arm after test-gps-arm-build to check ARM/QEMU too.
Expected coordinates are literal E7 values, independent of our C++ scaling/varint code.
"""

import importlib.util
import json
import subprocess
import sys
import tempfile
from hashlib import sha256
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ROOT / "tests/fixtures/gps_e7"
SCHEMA = SOURCES / "LocationData.proto"
# Degrees as text, and the E7 values the schema must carry.
CASES = [
    ("45.05", "3.88", 450500000, 38800000),
    ("48.123456", "-2.123456", 481234560, -21234560),
    ("-33.9", "151.2", -339000000, 1512000000),
    ("90", "180", 900000000, 1800000000),
    ("-90", "-180", -900000000, -1800000000),
    ("0", "0", 0, 0),
    ("-0.000001", "0.000001", -10, 10),
]
# Every field CarPlay gives the iPhone: latitude, longitude, accuracy, altitude, speed, bearing, UTC time.
FULL_CASES = [
    (
        ("45.05", "3.88", "10", "230.5", "25", "123.45", "1793750000"),
        {
            "timestamp": 1793750000,
            "latitude_e7": 450500000,
            "longitude_e7": 38800000,
            "accuracy_e3": 10000,
            "altitude_e2": 23050,
            "speed_e3": 25000,
            "bearing_e6": 123450000,
        },
    ),
    (
        ("-33.9", "151.2", "10", "-12.3", "0", "0", "0"),
        {
            "latitude_e7": -339000000,
            "longitude_e7": 1512000000,
            "accuracy_e3": 10000,
            "altitude_e2": -1230,
            "speed_e3": 0,
            "bearing_e6": 0,
        },
    ),
]


def load_schema(folder):
    """The pinned LocationData schema compiled by protoc into a Python module."""
    subprocess.run(["protoc", "-I", str(SOURCES), "--python_out=" + folder, str(SCHEMA)], check=True)
    spec = importlib.util.spec_from_file_location("gps_oracle_proto", Path(folder) / "LocationData_pb2.py")
    proto = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(proto)
    return proto


def wire(command, *args):
    """The frame the C++ code writes for these arguments."""
    result = subprocess.run([*command, *args], capture_output=True, text=True, check=True, timeout=60)
    return bytes.fromhex(result.stdout.strip())


def check_program(name, command, proto):
    """Compare one build's frames with the oracle; returns what was checked."""
    checks = []
    for lat, lon, e7lat, e7lon in CASES:
        actual = wire(command, "--wire", lat, lon)
        payload = proto.LocationData(latitude_e7=e7lat, longitude_e7=e7lon).SerializeToString()
        assert len(payload) < 128
        expected = b"\x80\x03\x0a" + bytes([len(payload)]) + payload
        parsed = proto.LocationData()
        parsed.ParseFromString(actual[4:])
        assert actual == expected, (
            name,
            lat,
            lon,
            "expected E7",
            e7lat,
            e7lon,
            "received",
            parsed.latitude_e7,
            parsed.longitude_e7,
        )
        assert parsed.IsInitialized()
        checks.append(
            {"degrees": [lat, lon], "wire_e7": [parsed.latitude_e7, parsed.longitude_e7], "frame_hex": actual.hex()}
        )
    for args, want in FULL_CASES:
        actual = wire(command, "--wire-full", *args)
        payload = proto.LocationData(**want).SerializeToString()
        assert actual[:3] == b"\x80\x03\x0a" and actual[3] == len(payload) and actual[4:] == payload, (
            name,
            args,
            want,
            actual.hex(),
        )
        checks.append({"full": want, "frame_hex": actual.hex()})
    print("PASS " + name + ": full LocationData (speed as read, bearing, altitude, accuracy, time) matches the oracle")
    print("PASS " + name + ": 7 degree/E7 fixtures match the independent aasdk/protoc oracle byte for byte")
    return checks


def main():
    record = next(s for s in json.loads((SOURCES / "sources.json").read_text()) if s["file"] == "LocationData.proto")
    if sha256(SCHEMA.read_bytes()).hexdigest() != record["sha256"]:
        raise SystemExit("FAIL: oracle schema changed")
    commands = {"host": [str(ROOT / "tests/test-evo-gps")]}
    if "--arm" in sys.argv:
        commands["ARM_QEMU"] = [
            "qemu-arm",
            "-L",
            str(Path.home() / "entrynav2/rootfs"),
            "-E",
            "LD_LIBRARY_PATH=/opt/hmi/lib:/usr/local/lib:/usr/lib:/lib",
            "-E",
            "LD_BIND_NOW=1",
            str(ROOT / "tests/test-evo-gps-arm"),
        ]
    report = {"vehicle_connected": False, "schema": record, "tests": {}}
    with tempfile.TemporaryDirectory(prefix="entry-gps-oracle-") as temporary:
        proto = load_schema(temporary)
        for name, command in commands.items():
            report["tests"][name] = {
                "result": "PASS",
                "cases": check_program(name, command, proto),
                "binary_sha256": sha256(Path(command[-1]).read_bytes()).hexdigest(),
            }
    out = ROOT / "artifacts/validation-pc"
    out.mkdir(parents=True, exist_ok=True)
    (out / "gps-wire-oracle.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
