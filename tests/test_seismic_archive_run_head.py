#!/usr/bin/env python3
"""Regression: the archived native run head is not the later artwork PR head.

The real artifact from Windows run 42973963152 carries exact-head.txt with
0d91cea9...; the finalized #57 author branch is bb2bc1e8... . This unit
test covers provenance acceptance/rejection without embedding or simulating
the actual canonical PNGs. Full native image auditing is a separate gate.
"""
import importlib.util
import io
from pathlib import Path
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "gallery/evolution/admission-20261111-seismic-flight/verify_native_admission.py"
spec = importlib.util.spec_from_file_location("seismic_archive_admission", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

GENUINE_RUN_HEAD = "0d91cea9a74a5db786b27a0bdc1a2fb1cc6ed0c4"
LATER_AUTHOR_PR_HEAD = "bb2bc1e81bb04df376fa19e06260c3b6da4c6007"


def archive_with_head(raw: bytes):
    memory = io.BytesIO()
    with zipfile.ZipFile(memory, "w") as z:
        z.writestr("exact-head.txt", raw)
    memory.seek(0)
    return zipfile.ZipFile(memory, "r")


class ArchivedRunHeadTests(unittest.TestCase):
    def test_genuine_windows_execution_head_passes(self):
        self.assertEqual(module.SEISMIC_RUN_HEAD, GENUINE_RUN_HEAD)
        with archive_with_head((GENUINE_RUN_HEAD + "\n").encode("ascii")) as z:
            self.assertEqual(module.check_seismic_run_head(z), GENUINE_RUN_HEAD)

    def test_later_author_branch_is_not_falsely_substituted(self):
        with archive_with_head((LATER_AUTHOR_PR_HEAD + "\n").encode("ascii")) as z:
            with self.assertRaisesRegex(ValueError, "run head mismatch"):
                module.check_seismic_run_head(z)

    def test_unrecognized_head_is_refused(self):
        with archive_with_head(b"0000000000000000000000000000000000000000\n") as z:
            with self.assertRaisesRegex(ValueError, "run head mismatch"):
                module.check_seismic_run_head(z)

    def test_missing_execution_receipt_fails_closed(self):
        memory = io.BytesIO()
        with zipfile.ZipFile(memory, "w") as z:
            z.writestr("other.txt", b"irrelevant")
        memory.seek(0)
        with zipfile.ZipFile(memory) as z:
            with self.assertRaises(KeyError):
                module.check_seismic_run_head(z)


if __name__ == "__main__":
    unittest.main()
