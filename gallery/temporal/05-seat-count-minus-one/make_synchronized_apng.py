#!/usr/bin/env python3
"""Lossless, offline, synchronized Angle/authored-Seat/editorial-Seat APNG.

Verifies every original Windows-native CPU PNG before assembling a NONCANONICAL
presentation with native-frame labels outside the image panes. Uses only stdlib,
adds no fabricated frames, and never overwrites.
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
WIDTH, HEIGHT = 1024, 272
PANELS = ((8, 24, 320, 192), (336, 8, 336, 224), (680, 8, 336, 224))


# Three-bit-wide, five-pixel-high in-tree captions avoid external font/assets.
# Captions are outside all canonical image panes; native pixels never change.
FONT3 = {
    'A': ('010','101','111','101','101'), 'B': ('110','101','110','101','110'),
    'C': ('011','100','100','100','011'), 'D': ('110','101','101','101','110'),
    'E': ('111','100','110','100','111'), 'F': ('111','100','110','100','100'),
    'G': ('011','100','101','101','011'), 'H': ('101','101','111','101','101'),
    'I': ('111','010','010','010','111'), 'J': ('111','001','001','101','010'),
    'K': ('101','101','110','101','101'), 'L': ('100','100','100','100','111'),
    'M': ('101','111','111','101','101'), 'N': ('101','111','111','111','101'),
    'O': ('010','101','101','101','010'), 'P': ('110','101','110','100','100'),
    'Q': ('010','101','101','111','011'), 'R': ('110','101','110','101','101'),
    'S': ('011','100','010','001','110'), 'T': ('111','010','010','010','010'),
    'U': ('101','101','101','101','111'), 'V': ('101','101','101','101','010'),
    'W': ('101','101','111','111','101'), 'X': ('101','101','010','101','101'),
    'Y': ('101','101','010','010','010'), 'Z': ('111','001','010','100','111'),
    '0': ('111','101','101','101','111'), '1': ('010','110','010','010','111'),
    '2': ('110','001','010','100','111'), '3': ('110','001','011','001','110'),
    '4': ('101','101','111','001','001'), '5': ('111','100','110','001','110'),
    '6': ('011','100','111','101','111'), '7': ('111','001','001','010','010'),
    '8': ('111','101','111','101','111'), '9': ('111','101','111','001','110'),
    ' ': ('000',)*5, '/': ('001','001','010','100','100'),
}


def label(target: bytearray, title: str, number: int, total: int,
          x: int, rgb: tuple[int,int,int]) -> None:
    """Add noncanonical transport labels in the reserved bottom 40px only."""
    for line, y in ((title,238), (f'FRAME {number:02d}/{total:02d}',253)):
        for i, char in enumerate(line):
            if char not in FONT3:
                raise ValueError(f'Unsupported caption glyph: {char!r}')
            for gy, bits in enumerate(FONT3[char]):
                for gx, bit in enumerate(bits):
                    if bit == '1':
                        xx, yy = x + 6 + i*8 + gx*2, y + gy*2
                        if xx+1 >= WIDTH or yy+1 >= HEIGHT:
                            raise ValueError('Caption exceeds presentation extent')
                        for dy in range(2):
                            for dx in range(2):
                                k = ((yy+dy)*WIDTH + xx+dx)*4
                                target[k:k+4] = bytes((*rgb, 255))


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


def presentation_frame(angle: bytes, seat: bytes, editorial: bytes,
                       ia: int, ib: int, ie: int) -> bytes:
    rgba = bytearray(b'\x0c\x11\x19\xff'*(WIDTH*HEIGHT))
    # Thin static panel identifiers only; original native pixels occupy untouched panes.
    for x, w, rgb in ((8,320,(47,197,225)),(336,336,(238,191,113)),(680,336,(190,139,238))):
        for y in range(0,4):
            for i in range(w):
                k=(y*WIDTH+x+i)*4
                rgba[k:k+4] = bytes((*rgb,255))
    blit(rgba,angle,*PANELS[0]); blit(rgba,seat,*PANELS[1]); blit(rgba,editorial,*PANELS[2])
    label(rgba, 'ANGLE NATIVE 12FPS', ia, 35, 8, (47,197,225))
    label(rgba, 'SEAT AUTHOR 10FPS', ib, 31, 336, (238,191,113))
    label(rgba, 'SEAT CUT 19 FRAMES', ie, 31, 680, (190,139,238))
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
                native=presentation_frame(pixels['angle'][ia],pixels['seat'][ib],pixels['seat'][ie],ia,ib,ie)
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
