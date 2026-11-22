# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Focused tests for the SDK ELF contract checks."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_sdk import NEEDED, check_contract  # noqa: E402


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


if __name__ == "__main__":
    unittest.main()
