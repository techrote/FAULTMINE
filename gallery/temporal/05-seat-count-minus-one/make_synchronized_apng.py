#!/usr/bin/env python3
"""Lossless, offline, synchronized Angle/authored-Seat/editorial-Seat APNG.

Verifies every original Windows-native CPU PNG before assembling a NONCANONICAL
presentation. Uses only stdlib, adds no fabricated frames, and never overwrites.
Open generated .png in Firefox/Chrome for a self-running A/B/C motion review.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
from pathlib import Path
import struct
import zlib
from zipfile import ZipFile

from compare_native import SPECS, SEAT_STUDY, read_family, decode_rgba

PNG_MAGIC = b'\x89PNG\r\n\x1a\n'
# Native panels in order: Angle at 12/1, Seat at 10/1, proposed Seat study.
FRAME_MS = 50                 # 20 presentation frames/s covers all 36 Angle frames
DURATION_MS = 3200           # Seat's original 32 frames at 10/1
TIMES = range(0, DURATION_MS, FRAME_MS)
WIDTH, HEIGHT = 1024, 240
PANELS = ((8, 24, 320, 192), (336, 8, 336, 224), (680, 8, 336, 224))


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack('>I', len(payload)) + kind + payload +
            struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff))


def seat_study_at(t: int) -> int:
    remaining = t
    for index in SEAT_STUDY:
        exposure = 500 if index == 0 else 545 if index == 31 else 155 if index < 9 else 115
        if remaining < exposure:
            return index
        remaining -= exposure
    return 31


def blit(target: bytearray, src: bytes, x: int, y: int, w: int, h: int) -> None:
    if len(src) != w*h*4 or x < 0 or y < 0 or x+w > WIDTH or y+h > HEIGHT:
        raise ValueError('Invalid lossless native frame blit')
    stride = WIDTH*4
    for row in range(h):
        dest = (y+row)*stride+x*4
        target[dest:dest+w*4] = src[row*w*4:(row+1)*w*4]


def presentation_frame(angle: bytes, seat: bytes, editorial: bytes) -> bytes:
    rgba = bytearray(b'\x0c\x11\x19\xff'*(WIDTH*HEIGHT))
    # Thin static panel identifiers only; original native pixels occupy untouched panes.
    for x, w, rgb in ((8,320,(47,197,225)),(336,336,(238,191,113)),(680,336,(190,139,238))):
        for y in range(0,4):
            for i in range(w):
                k=(y*WIDTH+x+i)*4
                rgba[k:k+4] = bytes((*rgb,255))
    blit(rgba,angle,*PANELS[0]); blit(rgba,seat,*PANELS[1]); blit(rgba,editorial,*PANELS[2])
    return bytes(rgba)


def compress_png_rgba(rgba: bytes) -> bytes:
    stride=WIDTH*4
    if len(rgba)!=HEIGHT*stride:
        raise ValueError('Invalid presentation dimensions')
    scanlines = b''.join(b'\0'+rgba[y*stride:(y+1)*stride] for y in range(HEIGHT))
    return zlib.compress(scanlines, 6)


def build(archive: Path, output: Path) -> dict:
    with ZipFile(archive) as z:
        images={s['key']:read_family(z,s) for s in SPECS}
    # read_family has validated the pinned receipt, encoded bytes, CRC, all 68
    # output identities, source/genome/seed, contiguous addresses and replay set.
    pixels = {}
    for spec in SPECS:
        frames=[]
        for image in images[spec['key']]:
            encoded=base64.b64decode(image['url'].partition(',')[2], validate=True)
            frames.append(decode_rgba(encoded, spec['width'], spec['height']))
        pixels[spec['key']]=frames
    if len(pixels['angle']) != 36 or len(pixels['seat']) != 32:
        raise ValueError('Incomplete authenticated native frame set')

    seq=0
    created=False
    try:
        with output.open('xb') as out:
            created=True
            def put(kind: bytes, payload: bytes) -> None:
                out.write(png_chunk(kind,payload))
            out.write(PNG_MAGIC)
            put(b'IHDR', struct.pack('>IIBBBBB',WIDTH,HEIGHT,8,6,0,0,0))
            put(b'acTL', struct.pack('>II', len(TIMES),0))
            for n,t in enumerate(TIMES):
                ia=min(35,t*12//1000)
                ib=min(31,t//100)
                ie=seat_study_at(t)
                native=presentation_frame(pixels['angle'][ia],pixels['seat'][ib],pixels['seat'][ie])
                compressed=compress_png_rgba(native)
                put(b'fcTL', struct.pack('>IIIIIHHBB',seq,WIDTH,HEIGHT,0,0,FRAME_MS,1000,0,0));seq+=1
                if n==0:
                    put(b'IDAT', compressed)
                else:
                    put(b'fdAT',struct.pack('>I',seq)+compressed);seq+=1
            put(b'IEND',b'')
    except BaseException:
        if created:
            output.unlink(missing_ok=True)
        raise
    digest=hashlib.sha256(output.read_bytes()).hexdigest()
    result={'frames':len(TIMES),'duration_ms':DURATION_MS,'sha256':digest,
            'angle_native_frames':36,'seat_native_frames':32,'editorial_native_indices':list(SEAT_STUDY)}
    print('PASS: authenticated 68 native CPU frames; 64 synchronized lossless presentation samples; no interpolation')
    print('APNG (NONCANONICAL 3-up presentation):',output,'SHA-256:',digest)
    return result


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('four_family_review_zip',type=Path)
    p.add_argument('out_apng',type=Path)
    args=p.parse_args()
    build(args.four_family_review_zip,args.out_apng)
