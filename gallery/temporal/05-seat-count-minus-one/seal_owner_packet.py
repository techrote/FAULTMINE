#!/usr/bin/env python3
"""Seal an *already native-qualified* Angle/Seat offline owner review folder.

Packaging integrity only: this does not qualify CPU replay or assert Win32 acceptance.
Refuses missing outputs/overwrite. Python 3 stdlib to GENERATE; the sealed owner
package runs with Windows PowerShell 5.1 and a browser, no Python required.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import re

REQUIRED = (
    'FULL-SEQUENCE-PREVIEW.png',
    'FULL-SEQUENCE-SEAT-32-PLUS-ANGLE.svg',
    'NATIVE-CONTACT-SHEET.png',
    'NATIVE-IDENTITY-MANIFEST.json',
    'OPEN-REVIEW.html',
    'START-HERE.txt',
    'WATCH-THREE-TRACKS-LABELED.png',
)


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def generate(folder: Path) -> dict:
    folder = folder.resolve()
    if not folder.is_dir():
        raise ValueError('Owner review directory does not exist')
    for name in REQUIRED:
        p = folder / name
        if not p.is_file() or p.is_symlink() or not p.stat().st_size:
            raise ValueError(f'Missing/empty/symlink qualified review asset: {name}')
    targets = [folder / name for name in ('SHA256SUMS.txt','VERIFY-REVIEW.ps1','START-REVIEW.cmd')]
    for target in targets:
        if target.exists() or target.is_symlink():
            raise FileExistsError(f'Refusing to overwrite prior sealed output: {target}')

    sums = ''.join(f'{sha(folder / name)}  {name}\n' for name in REQUIRED)
    digest = hashlib.sha256(sums.encode('ascii')).hexdigest()
    # PowerShell has no external modules. Pin the manifest content hash in the
    # independent verifier so accidental edits to both an image and checksum
    # file cannot pass. Invalid names, duplicates and extra files are rejected.
    ps = r'''# Offline FAULTMINE review package verifier - generated, exact-pinned.
# PowerShell 5.1+; no network; refuses to open edited/missing exhibits.
$ErrorActionPreference = 'Stop'
try {
  $root = $PSScriptRoot
  $sumPath = Join-Path $root 'SHA256SUMS.txt'
  if (-not (Test-Path -LiteralPath $sumPath -PathType Leaf)) { throw 'SHA256SUMS.txt missing' }
  $pinned = '__PINNED_SHA256__'
  if ((Get-FileHash -LiteralPath $sumPath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pinned) { throw 'checksum manifest differs from sealed original' }
  $lines = @(Get-Content -LiteralPath $sumPath -Encoding ASCII)
  if ($lines.Count -ne __NUM__) { throw 'wrong checksum entry count' }
  $expected = @(__NAMES__)
  for ($i = 0; $i -lt $expected.Count; $i++) {
    $match = [regex]::Match($lines[$i], '^([a-f0-9]{64})  ([A-Za-z0-9_.-]+)$')
    if (-not $match.Success -or $match.Groups[2].Value -cne $expected[$i]) { throw "invalid/unexpected checksum entry at index $i" }
    $path = Join-Path $root $expected[$i]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "missing exhibit: $($expected[$i])" }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $match.Groups[1].Value) { throw "exhibit modified: $($expected[$i])" }
  }
  Write-Host ('PASS: ' + $expected.Count + ' unchanged original reviewer assets; evidence archive was prequalified separately.')
  exit 0
} catch {
  Write-Error ('FAULTMINE review integrity failure: ' + $_.Exception.Message)
  exit 1
}
'''
    names=', '.join("'" + n + "'" for n in REQUIRED)
    ps=ps.replace('__PINNED_SHA256__',digest).replace('__NUM__',str(len(REQUIRED))).replace('__NAMES__',names)
    cmd = r'''@echo off
setlocal
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0VERIFY-REVIEW.ps1"
if errorlevel 1 (
  echo [FAULTMINE] Review integrity failed. Original evidence left unchanged.
  pause
  exit /b 1
)
start "" "%~dp0OPEN-REVIEW.html"
if errorlevel 1 exit /b 1
exit /b 0
'''
    # Create atomically with exclusive creation: interrupted jobs cannot
    # silently replace an existing owner packet or artist's original assets.
    for p, content in zip(targets, [sums, ps, cmd]):
        with p.open('x',encoding='ascii',newline='\n') as f:
            f.write(content)
    return {'files':len(REQUIRED), 'manifest_sha256':digest,
            'entrypoint':'START-REVIEW.cmd'}


def main() -> None:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('review_folder',type=Path)
    args=ap.parse_args()
    print(generate(args.review_folder))

if __name__=='__main__':
    main()
