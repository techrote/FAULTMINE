#!/usr/bin/env python3
"""Read-only PNG transport/decoded-pixel audit, separate from native CPU replay.

The archived still is a lossless RGB PNG transcode of real Windows WIC
canonical output. Restrict the archived PNG encoding deliberately: RGB8,
non-interlaced, filter-0 scanlines, CRC-checked chunks. The authoritative
FAULTMINE-render.exe --verify must independently compare it on Windows.
"""
import hashlib
from pathlib import Path
import struct
import zlib

HERE = Path(__file__).resolve().parent
OUTPUT_ID = '171e395bac997e35d740f400cbbce3e9fdb556056f5be7fd45eef5d158536c7c'
ENCODED_SHA = '59488382c0ad776ee00dc1419d11ba1ddbf79871b161059037a6f4234af33088'
DOMAIN = b'FAULTMINE-SOURCE-RGBA8-v1'


def decode_archived_png(data):
    assert data.startswith(b'\x89PNG\r\n\x1a\n'), 'PNG signature'
    position = 8
    payload = bytearray()
    dimensions = None
    kinds = []
    while position < len(data):
        assert position + 12 <= len(data), 'truncated PNG chunk header'
        length = struct.unpack_from('>I', data, position)[0]
        kind = data[position + 4:position + 8]
        stop = position + 12 + length
        assert stop <= len(data), 'truncated PNG chunk'
        chunk_data = data[position + 8:position + 8 + length]
        claimed = struct.unpack_from('>I', data, position + 8 + length)[0]
        assert zlib.crc32(kind + chunk_data) == claimed, 'PNG chunk CRC'
        kinds.append(kind)
        if kind == b'IHDR':
            assert dimensions is None and length == 13, 'PNG IHDR structure'
            dimensions = struct.unpack('>IIBBBBB', chunk_data)
        elif kind == b'IDAT':
            payload.extend(chunk_data)
        elif kind == b'IEND':
            assert length == 0 and stop == len(data), 'PNG trailing data'
        else:
            raise AssertionError('unexpected PNG chunk')
        position = stop
    assert kinds == [b'IHDR', b'IDAT', b'IEND'], 'archived PNG chunk order'
    assert dimensions == (384, 256, 8, 2, 0, 0, 0), 'PNG dimensions/format'
    width, height = dimensions[:2]
    row_size = width * 3
    rows = zlib.decompress(bytes(payload))
    assert len(rows) == (row_size + 1) * height, 'decompressed PNG length'
    rgba = bytearray()
    for y in range(height):
        start = y * (row_size + 1)
        assert rows[start] == 0, 'archived PNG row filter must be zero'
        rgb = rows[start + 1:start + 1 + row_size]
        for x in range(0, row_size, 3):
            rgba.extend((*rgb[x:x + 3], 255))
    return width, height, rgba


def verify():
    data = (HERE / 'moth-rom-canonical.png').read_bytes()
    assert hashlib.sha256(data).hexdigest() == ENCODED_SHA, 'PNG encoded transport drift'
    width, height, rgba = decode_archived_png(data)
    identity = hashlib.sha256(DOMAIN + struct.pack('<II', width, height) + rgba).hexdigest()
    assert identity == OUTPUT_ID, 'decoded RGBA8 canonical identity drift'
    tampered = bytearray(data)
    tampered[41] ^= 1  # inside first compressed IDAT payload
    try:
        decode_archived_png(tampered)
    except AssertionError:
        pass
    else:
        raise AssertionError('PNG CRC negative control failed')
    print('PASS: archived PNG transport CRC, full decoded RGBA8 hash and negative corruption control')
    print('PENDING: new exact-head Windows native --verify of committed PNG; GUI/FM-012 and aesthetic acceptance')
    return identity


if __name__ == '__main__':
    verify()
