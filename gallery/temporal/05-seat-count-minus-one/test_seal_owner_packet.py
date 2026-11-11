#!/usr/bin/env python3
"""Portable regression of the seal and explicit tamper classes (not PS execution)."""
from pathlib import Path
import hashlib
import re
import shutil
import tempfile
import unittest
import seal_owner_packet as subj

class SealTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder=Path(self.temp.name)
        for i, name in enumerate(subj.REQUIRED):
            (self.folder/name).write_bytes(bytes([i+17])*100)

    def check(self):
        manifest=self.folder/'SHA256SUMS.txt'
        ps=(self.folder/'VERIFY-REVIEW.ps1').read_text('ascii')
        assert subj.sha(manifest) in ps
        lines=manifest.read_text('ascii').splitlines()
        self.assertEqual(len(lines),len(subj.REQUIRED))
        for line, name in zip(lines,subj.REQUIRED):
            self.assertRegex(line, r'^[a-f0-9]{64}  [A-Za-z0-9_.-]+$')
            h,fn=line.split('  ',1)
            self.assertEqual(fn,name)
            self.assertEqual(h,subj.sha(self.folder/name))

    def test_emitted_and_readonly(self):
        subj.generate(self.folder)
        self.check()
        self.assertIn('if errorlevel 1', (self.folder/'START-REVIEW.cmd').read_text('ascii'))
        self.assertIn('Get-FileHash', (self.folder/'VERIFY-REVIEW.ps1').read_text('ascii'))
        with self.assertRaises(FileExistsError):subj.generate(self.folder)

    def test_missing_guard(self):
        (self.folder/subj.REQUIRED[3]).unlink()
        with self.assertRaises(ValueError):subj.generate(self.folder)
        self.assertFalse((self.folder/'SHA256SUMS.txt').exists())

    def test_bad_filename_and_modified_payload_are_detectable(self):
        subj.generate(self.folder)
        ps=(self.folder/'VERIFY-REVIEW.ps1').read_text('ascii')
        h= re.search(r"\$pinned = '([a-f0-9]{64})'",ps).group(1)
        self.assertEqual(h,subj.sha(self.folder/'SHA256SUMS.txt'))
        # Mutated payload fails its pinned image digest even if PNG still parses.
        (self.folder/subj.REQUIRED[2]).write_bytes(b'altered')
        self.assertNotEqual(subj.sha(self.folder/subj.REQUIRED[2]),
                            dict((name, h) for h, name in (x.split('  ') for x in (self.folder/'SHA256SUMS.txt').read_text('ascii').splitlines()))[subj.REQUIRED[2]])
        # Fake updated manifest fails verifier's immutable pinned manifest hash.
        with (self.folder/'SHA256SUMS.txt').open('a') as f:f.write('0'*64+'  injected.png\n')
        self.assertNotEqual(h,subj.sha(self.folder/'SHA256SUMS.txt'))

    def test_existing_output_prevents_any_write(self):
        (self.folder/'VERIFY-REVIEW.ps1').write_text('owner data')
        with self.assertRaises(FileExistsError):subj.generate(self.folder)
        self.assertFalse((self.folder/'SHA256SUMS.txt').exists())
        self.assertEqual((self.folder/'VERIFY-REVIEW.ps1').read_text(),'owner data')

if __name__=='__main__':unittest.main(verbosity=2)
