#!/usr/bin/env python3
# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Format every source file of the project.

Usage: python tools/format_sources.py [--check] [--tools DIR]

--check reports the files that would change and exits with status 1 if there are any.
--tools names the folder holding clang-format and ruff when they are not on the PATH.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIRS = ("core", "platform", "hmiapp", "tools", "tests")
EXCLUDED_DIRS = {"__pycache__", "fixtures"}
CPP_SUFFIXES = {".c", ".cpp", ".h", ".hpp"}


def source_files(suffixes):
    """Return the project's files having one of these suffixes, sorted."""
    found = []
    for name in SOURCE_DIRS:
        for folder, subfolders, files in os.walk(ROOT / name):
            subfolders[:] = [sub for sub in subfolders if sub not in EXCLUDED_DIRS]
            found.extend(Path(folder) / file for file in files if Path(file).suffix.lower() in suffixes)
    return sorted(found)


def find_tool(name, tools_dir):
    """Locate a formatter in --tools, then on the PATH."""
    path = shutil.which(name, path=tools_dir) if tools_dir else None
    path = path or shutil.which(name)
    if not path:
        raise SystemExit(f"{name} not found: pip install clang-format==18.1.8 ruff==0.6.9")
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="report without changing anything")
    parser.add_argument("--tools", help="folder holding clang-format and ruff")
    args = parser.parse_args()

    clang_format = find_tool("clang-format", args.tools)
    ruff = find_tool("ruff", args.tools)
    cpp_files = [str(path) for path in source_files(CPP_SUFFIXES)]

    failed = False
    for start in range(0, len(cpp_files), 40):
        batch = cpp_files[start : start + 40]
        options = ["--dry-run", "--Werror"] if args.check else ["-i"]
        failed |= subprocess.run([clang_format, "--style=file", *options, *batch], cwd=ROOT).returncode != 0

    python_dirs = [str(ROOT / name) for name in SOURCE_DIRS]
    ruff_format = [ruff, "format", *(["--check"] if args.check else []), *python_dirs]
    failed |= subprocess.run(ruff_format, cwd=ROOT).returncode != 0

    if failed:
        print("FAIL: formatting differences or tool errors")
        return 1
    print(f"PASS: {len(cpp_files)} C/C++ files and the Python files are formatted")
    return 0


if __name__ == "__main__":
    sys.exit(main())
