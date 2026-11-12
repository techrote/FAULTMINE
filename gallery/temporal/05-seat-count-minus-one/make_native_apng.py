#!/usr/bin/env python3
"""Exact-pixel APNG player for archived ART-TEMP-005 Windows CPU frames.

Presentation only: no native render, frame synthesis, or FM-012 GUI export.
Works on Windows with the Python standard library; never writes over an output.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path
from zipfile import ZipFile

from verify_native_frames import DOMAIN, decode_png, png_chunk

RECEIPT_SHA256 = '1ee290b15ffb8a07d4416b51185d223935c321bfea356ddc62e750ab12f4b590'
SOURCE_ID = 'cb8df081f6f79c6ec75e974dc0f20cb227aa79e97c17dcaeadf554b311eda342'
GENOME_ID = 'e8f70bc1b738b5557d2c0e20beb8d1af49b4c1c0894c4d8bbe1758a11acc3850'
ROOT_SEED = '9a5d7e63c204b811'
FRAME_COUNT = 32
NATIVE_DIRS = ('/CANONICAL-REVIEW-EVIDENCE/seat-05-release/', 'NATIVE-CANONICAL-FRAMES/')
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'


def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def unique_entry(names: list[str], filename: str) -> str:
    matches = [n for n in names if any((n == directory.lstrip('/') + filename or n.endswith(directory + filename)) for directory in NATIVE_DIRS)]
    if len(matches) != 1:
        raise ValueError(f'Expected one archived native {filename}, found {len(matches)}')
    return matches[0]


def compressed_png_payload(raw: bytes) -> bytes:
    """Retain the native WIC PNG's compressed scanlines without re-encoding them."""
    if not raw.startswith(PNG_SIGNATURE):
        raise ValueError('Invalid native PNG signature')
    pos = len(PNG_SIGNATURE)
    idat = bytearray()
    while pos < len(raw):
        if pos + 12 > len(raw):
            raise ValueError('Truncated native PNG chunk')
        size = int.from_bytes(raw[pos:pos+4], 'big')
        name = raw[pos+4:pos+8]
        end = pos + size + 12
        if end > len(raw):
            raise ValueError('Truncated native PNG payload')
        if name == b'IDAT':
            idat.extend(raw[pos+8:pos+8+size])
        pos = end
        if name == b'IEND':
            break
    if not idat or pos != len(raw):
        raise ValueError('Missing IDAT or invalid native PNG trailer')
    return bytes(idat)


def read_canonical(archive: Path) -> tuple[list[bytes], list[dict]]:
    with ZipFile(archive) as z:
        names = z.namelist()
        receipt = z.read(unique_entry(names, 'native-cli-replay-audit.json'))
        if sha(receipt) != RECEIPT_SHA256:
            raise ValueError('Pinned Windows-native replay receipt hash differs')
        record = json.loads(receipt)
        for key, value in {'audit_version':1, 'kind':'authored-native-cli-replay-not-fm012',
                           'complete':True, 'native_configuration':'Release',
                           'source_identity':SOURCE_ID, 'genome_identity':GENOME_ID,
                           'root_seed':ROOT_SEED, 'frame_begin':0,
                           'frame_end_exclusive':FRAME_COUNT, 'semantic_rate':'10/1',
                           'out_of_order_replay':[31,0,18,5,24]}.items():
            if record.get(key) != value:
                raise ValueError(f'Native replay provenance mismatch: {key}')
        rows = record.get('frames', [])
        if len(rows) != FRAME_COUNT:
            raise ValueError('Native frame count mismatch')
        originals = []
        ids = set()
        for i, row in enumerate(rows):
            name = f'seat-count_{i:06d}.png'
            if row.get('frame') != i or row.get('file') != name:
                raise ValueError(f'Native ordered frame {i} mismatch')
            raw = z.read(unique_entry(names, name))
            if sha(raw) != row.get('encoded_png_sha256'):
                raise ValueError(f'Native PNG transport mismatch at frame {i}')
            width, height, rgba = decode_png(raw)
            actual = sha(DOMAIN + struct.pack('<II', width, height) + rgba)
            if actual != row.get('output_identity'):
                raise ValueError(f'Native decoded canonical identity mismatch at frame {i}')
            ids.add(actual)
            originals.append(raw)
        if len(ids) != FRAME_COUNT:
            raise ValueError(f'Only {len(ids)}/{FRAME_COUNT} distinct decoded native frames')
    return originals, rows


def assemble_apng(originals: list[bytes]) -> bytes:
    """Full 32-frame 10fps APNG. fcTL/fdAT preserve native compressed PNG rows."""
    if len(originals) != FRAME_COUNT:
        raise ValueError('APNG requires all 32 native frames')
    header = originals[0][16:29]
    if struct.unpack('>IIBBBBB', header) != (336,224,8,6,0,0,0):
        raise ValueError('Expected native 336x224 RGBA8 PNG')
    chunks = [png_chunk(b'IHDR', header), png_chunk(b'acTL', struct.pack('>II', FRAME_COUNT, 0))]
    seq = 0
    for i, raw in enumerate(originals):
        if raw[16:29] != header:
            raise ValueError(f'Frame {i} has an incompatible PNG header')
        # Full-canvas SOURCE replacement, no interpolation or transparency blending.
        chunks.append(png_chunk(b'fcTL', struct.pack('>IIIIIHHBB', seq,336,224,0,0,1,10,0,0)))
        seq += 1
        compressed = compressed_png_payload(raw)
        if i == 0:
            chunks.append(png_chunk(b'IDAT', compressed))
        else:
            chunks.append(png_chunk(b'fdAT', struct.pack('>I', seq) + compressed))
            seq += 1
    chunks.append(png_chunk(b'IEND', b''))
    return PNG_SIGNATURE + b''.join(chunks)


def build(archive: Path, target: Path) -> tuple[str, list[dict]]:
    originals, rows = read_canonical(archive)
    encoded = assemble_apng(originals)
    # Exclusive creation: a stale preview is not silently replaced.
    with target.open('xb') as f:
        f.write(encoded)
    return sha(encoded), rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native_review_zip', type=Path)
    parser.add_argument('out_apng', type=Path)
    args = parser.parse_args()
    digest, rows = build(args.native_review_zip, args.out_apng)
    print('PASS: 32/32 original Windows CPU/WIC PNGs decoded and independently matched to canonical RGBA8 output IDs')
    print('Authored playback: full native frame indices [0,32), 10/1 fps, each frame 100ms')
    print('APNG presentation (not canonical export):', args.out_apng, 'SHA-256', digest)
    print('Native first/last output IDs:', rows[0]['output_identity'], rows[-1]['output_identity'])
    print('PENDING: human aesthetics, physical Win32 project import and FM-012 complete/cancelled sequence export')


if __name__ == '__main__':
    main()
