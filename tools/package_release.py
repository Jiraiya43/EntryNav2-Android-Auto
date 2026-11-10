# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Build the release archive: artifacts/entry-aad-<version>-armhf.tar, ready to copy to the head unit.

Its files sit at the paths they take on the car (opt/aa/..., opt/hmi/data/ro/Plugins/EntryAad/...), with MD5SUMS
checkable by the car's BusyBox before and after the copy (docs/INSTALLATION.md). Only the programs recorded by the last
validation (make verify_binary verify_wireless) go in. The archive is reproducible: fixed owner, modes and dates.
"""

import io
import json
import subprocess
import sys
import tarfile
from hashlib import md5, sha256
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = "opt/hmi/data/ro/Plugins/EntryAad"
PLUGIN_FILES = (
    "libEntryAad.so",
    "EntryAad.manifest",
    "EntryAad.xml",
    "EntryAad_ALL_Texts.zip",
    "EntryAad_ALL_Images.zip",
)
PROGRAMS = ("entry-aad", "aa-supervisor")
EXECUTABLES = {f"opt/aa/{name}" for name in PROGRAMS} | {f"{PLUGIN}/libEntryAad.so"}


def validated_programs() -> dict:
    """The ARM programs, exactly those recorded by the last validation."""
    report_path = ROOT / "artifacts/wireless-validation.json"
    if not report_path.is_file():
        raise SystemExit(f"FAIL: {report_path} missing: run make verify_binary verify_wireless first")
    report = json.loads(report_path.read_text())
    programs = {}
    for name in PROGRAMS:
        data = (ROOT / name).read_bytes()
        if report["binaries"][name]["sha256"] != sha256(data).hexdigest():
            raise SystemExit(f"FAIL: {name} differs from the validated one: rebuild, then make verify_wireless")
        programs[f"opt/aa/{name}"] = data
    return programs


def release_files() -> dict:
    subprocess.run(
        [sys.executable, "-I", str(ROOT / "hmiapp/make_db.py"), str(ROOT / "hmiapp/res"), str(ROOT / "hmiapp")],
        check=True,
    )
    files = {f"{PLUGIN}/{name}": (ROOT / "hmiapp" / name).read_bytes() for name in PLUGIN_FILES}
    if not files[f"{PLUGIN}/libEntryAad.so"].startswith(b"\x7fELF"):
        raise SystemExit("FAIL: hmiapp/libEntryAad.so is not an ARM build")
    files.update(validated_programs())
    files["MD5SUMS"] = "".join(f"{md5(data).hexdigest()}  {name}\n" for name, data in sorted(files.items())).encode()
    files["LICENSE.md"] = (ROOT / "LICENSE.md").read_bytes()
    files["INSTALLATION.md"] = (ROOT / "docs/INSTALLATION.md").read_bytes()
    return files


def main() -> None:
    version = (ROOT / "VERSION").read_text().strip()
    top = f"entry-aad-{version}"
    epoch = int(
        subprocess.run(["git", "log", "-1", "--format=%ct"], cwd=ROOT, capture_output=True, text=True).stdout or 0
    )
    archive = ROOT / f"artifacts/{top}-armhf.tar"
    archive.parent.mkdir(exist_ok=True)
    payload = io.BytesIO()
    files = release_files()
    folders = sorted({str(Path(name).parent).replace("\\", "/") for name in files} - {"."})
    folders = sorted({"/".join(folder.split("/")[: i + 1]) for folder in folders for i in range(folder.count("/") + 1)})
    with tarfile.open(fileobj=payload, mode="w", format=tarfile.USTAR_FORMAT) as tar:
        for name in [""] + folders:  # explicit folders: no tar has to guess their mode
            item = tarfile.TarInfo(f"{top}/{name}".rstrip("/"))
            item.type = tarfile.DIRTYPE
            item.mode = 0o755
            item.mtime = epoch
            item.uname = item.gname = "root"
            tar.addfile(item)
        for name, data in sorted(files.items()):
            item = tarfile.TarInfo(f"{top}/{name}")
            item.size = len(data)
            item.mode = 0o755 if name in EXECUTABLES else 0o644
            item.mtime = epoch
            item.uname = item.gname = "root"
            tar.addfile(item, io.BytesIO(data))
    archive.write_bytes(payload.getvalue())
    digest = sha256(payload.getvalue()).hexdigest()
    (archive.parent / f"{archive.name}.sha256").write_text(f"{digest}  {archive.name}\n")
    print(f"{archive}\nsha256 {digest}")


if __name__ == "__main__":
    main()
