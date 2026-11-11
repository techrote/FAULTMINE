#!/usr/bin/env python3
"""Package authentic, already-rendered ART-TEMP-004 native frames as lossless APNG.

The CPU canonical pipeline is NOT reimplemented here; APNG is presentation only.
The pinned Windows evidence and every decoded RGBA8 frame are validated first.
"""
from __future__ import annotations
import argparse
import hashlib
import struct
from pathlib import Path
from verify_native_frames import audit, decode_png, png_chunk

RECEIPT_SHA256 = 'fedc650318aedf9b6f1cbe5809bd5e0f648c4a25fffcac8092e3ced396ec6b5c'
SIGNATURE = b'\x89PNG\r\n\x1a\n'
FRAMES = 36
HEADER = struct.pack('>IIBBBBB', 320, 192, 8, 6, 0, 0, 0)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def compressed_rows(png: bytes) -> bytes:
    if not png.startswith(SIGNATURE) or png[16:29] != HEADER:
        raise ValueError('Unexpected native PNG header')
    position, parts = len(SIGNATURE), []
    while position < len(png):
        if position + 12 > len(png):
            raise ValueError('Truncated PNG')
        length = int.from_bytes(png[position:position + 4], 'big')
        kind = png[position + 4:position + 8]
        end = position + 12 + length
        if end > len(png):
            raise ValueError('Invalid PNG chunk boundary')
        if kind == b'IDAT':
            parts.append(png[position + 8:position + 8 + length])
        position = end
        if kind == b'IEND':
            break
    if not parts or position != len(png):
        raise ValueError('Missing image data or trailing PNG bytes')
    return b''.join(parts)


def build(native_dir: Path, output: Path) -> str:
    if output.exists():
        raise FileExistsError(f'Existing presentation will not be overwritten: {output}')
    receipt = (native_dir / 'native-cli-replay-audit.json').read_bytes()
    if digest(receipt) != RECEIPT_SHA256:
        raise ValueError('Input is not the pinned Windows-native Release receipt')
    # Independent PNG parser checks all CRCs, native encoded hashes, normalized
    # RGBA8 identities, 36 ordered unique frame IDs and source/genome/seed.
    audit(native_dir)
    chunks = [png_chunk(b'IHDR', HEADER),
              png_chunk(b'acTL', struct.pack('>II', FRAMES, 0))]
    sequence = 0
    for frame in range(FRAMES):
        png = (native_dir / f'angle-departure_{frame:06}.png').read_bytes()
        decode_png(png)  # additional full-shape guard before copying rows
        # SOURCE-replace full frame at precisely 12/1 fps, with no interpolation.
        chunks.append(png_chunk(b'fcTL', struct.pack('>IIIIIHHBB',
                       sequence, 320, 192, 0, 0, 1, 12, 0, 0)))
        sequence += 1
        original_rows = compressed_rows(png)
        if frame == 0:
            chunks.append(png_chunk(b'IDAT', original_rows))
        else:
            chunks.append(png_chunk(b'fdAT', struct.pack('>I', sequence) + original_rows))
            sequence += 1
    result = SIGNATURE + b''.join(chunks) + png_chunk(b'IEND', b'')
    with output.open('xb') as fh:
        fh.write(result)
    return digest(result)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native_frames_directory', type=Path)
    parser.add_argument('new_presentation_apng', type=Path)
    args = parser.parse_args()
    sha = build(args.native_frames_directory, args.new_presentation_apng)
    print('PASS: unchanged 36-frame 320x192 Windows CPU/WIC APNG at 12fps, SHA-256', sha)
    print('NOT PROVED: Win32 GUI playback or FM-012 complete/cancelled export')
