"""Exercise identity validation, pending/active isolation and persisted byte format."""
from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from tests.harness import ROOT


class IdentityTests(unittest.TestCase):
    def test_native_identity_regressions(self) -> None:
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("host C compiler unavailable")
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "identity"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "tests/native/include"),
                str(ROOT / "tests/native/identity/runner.c"),
                str(ROOT / "src/zerofido_identity.c"), "-o", str(binary),
            ], check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
            self.assertIn("native identity regressions passed", result.stdout)

    def test_attestation_identity_and_cache(self) -> None:
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("host C compiler unavailable")
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "attestation"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-DZF_HOST_TEST", "-DZF_PACKED_ATTESTATION=1", "-DZF_RELEASE_DIAGNOSTICS=0",
                "-ffunction-sections", "-fdata-sections",
                "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
                "-I", str(ROOT / "tests/native/include"),
                str(ROOT / "tests/native/identity/attestation_runner.c"),
                str(ROOT / "src/zerofido_attestation.c"), "-o", str(binary),
            ], check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
            self.assertIn("native attestation identity regressions passed", result.stdout)
