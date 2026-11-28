# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Focused tests for the SDK ELF contract checks."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_sdk import NEEDED, check_contract, manifest_symbols  # noqa: E402


class SdkContractTest(unittest.TestCase):
    def setUp(self):
        self.name = "hmiapp/libEntryAad.so"
        self.needed = NEEDED[self.name].copy()
        self.symbols = {"_ZN3bmw3fooEv": "libAppFramework.A.so"}
        self.imports = {"_ZN3bmw3fooEv", "clock_gettime"}
        self.exports = {"pluginAppInfo"}

    def check(self, paths=()):
        check_contract(self.name, self.needed, paths, self.imports, self.exports, self.symbols)

    def test_valid_contract(self):
        self.check()

    def test_rejects_unexpected_dependency(self):
        self.needed.add("libunexpected.so")
        with self.assertRaisesRegex(SystemExit, "DT_NEEDED"):
            self.check()

    def test_rejects_duplicate_dependency(self):
        self.needed = [*self.needed, next(iter(self.needed))]
        with self.assertRaisesRegex(SystemExit, "DT_NEEDED"):
            self.check()

    def test_rejects_runtime_search_path(self):
        with self.assertRaisesRegex(SystemExit, "RPATH or RUNPATH"):
            self.check(["DT_RUNPATH"])

    def test_rejects_missing_or_unlisted_vendor_symbol(self):
        self.imports.remove("_ZN3bmw3fooEv")
        with self.assertRaisesRegex(SystemExit, "vendor imports differ"):
            self.check()
        self.imports.update({"_ZN3bmw3fooEv", "_ZN3bmw3barEv"})
        with self.assertRaisesRegex(SystemExit, "vendor imports differ"):
            self.check()

    def test_rejects_missing_plugin_export(self):
        self.exports.clear()
        with self.assertRaisesRegex(SystemExit, "pluginAppInfo"):
            self.check()

    def test_rejects_versioned_vendor_symbol(self):
        self.imports.remove("_ZN3bmw3fooEv")
        self.imports.add("_ZN3bmw3fooEv@BMW_1")
        with self.assertRaisesRegex(SystemExit, "versioned proprietary import"):
            self.check()


class SdkManifestTest(unittest.TestCase):
    def parse(self, contents):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sdk.symbols"
            path.write_text(contents)
            return manifest_symbols(path)

    def test_valid_manifest(self):
        self.assertEqual(self.parse("# comment\n\nlibxsvideo.so.4 FUNC xsv_open\n"), {"xsv_open": "libxsvideo.so.4"})

    def test_invalid_rows_report_line(self):
        for line in (
            "libxsvideo.so.4 FUNC",
            "libxsvideo.so.4 OBJECT xsv_open",
            "libunknown.so FUNC xsv_open",
            "libxsvideo.so.4 FUNC xsv_open@VERSION",
            "libxsvideo.so.4 FUNC xsv_open extra",
        ):
            with self.subTest(line=line), self.assertRaisesRegex(SystemExit, r"sdk\.symbols:2: invalid"):
                self.parse(f"# comment\n{line}\n")

    def test_rejects_duplicate_symbols_across_libraries(self):
        contents = "libxsvideo.so.4 FUNC symbol\nlibAppFramework.A.so FUNC symbol\n"
        with self.assertRaisesRegex(SystemExit, r"sdk\.symbols:2: invalid or duplicate"):
            self.parse(contents)


if __name__ == "__main__":
    unittest.main()
