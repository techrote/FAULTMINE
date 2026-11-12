#!/usr/bin/env python3
"""Read-only independent canonical-pixel audit and noncanonical contact sheet.

Inputs must be genuine native Windows FAULTMINE-render.exe outputs and its
Integration audit; this utility never simulates the canonical fault engine.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

DOMAIN = b'FAULTMINE-SOURCE-RGBA8-v1'
SOURCE_ID = 'ba533af14e6dc02861ab0d8e55239edcc2358f554e07f460aa4fde9cd590619c'
GENOME_ID = '0de88b0fdfbd357ce09761200d2b96b90c8aa2977a9887a3dd7011315751cba2'
SEED = 'e30bd49c7a612f08'
KEY_FRAMES = (0, 4, 8, 12, 16, 20, 24, 27, 29)

def sha(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()

def decode_png(payload: bytes) -> tuple[int, int, bytes]:
    assert payload.startswith(b'\x89PNG\r\n\x1a\n'), 'bad PNG signature'
    off = 8
    chunks = bytearray()
    header = None
    ended = False
    while off < len(payload):
        assert off + 12 <= len(payload), 'truncated PNG header'
        size = int.from_bytes(payload[off:off+4], 'big')
        kind = payload[off+4:off+8]
        end = off + 12 + size
        assert end <= len(payload), 'truncated PNG chunk'
        data = payload[off+8:off+8+size]
        crc = int.from_bytes(payload[end-4:end], 'big')
        assert zlib.crc32(kind+data) & 0xffffffff == crc, 'PNG CRC mismatch'
        if kind == b'IHDR':
            assert header is None and size == 13
            header = struct.unpack('>IIBBBBB', data)
        elif kind == b'IDAT':
            chunks.extend(data)
        elif kind == b'IEND':
            assert size == 0 and end == len(payload), 'trailing PNG bytes'
            ended = True
            break
        off = end
    assert header is not None and ended, 'missing PNG IHDR/IEND'
    w,h,depth,colour,comp,filter_method,interlace = header
    assert (w,h,depth,colour,comp,filter_method,interlace) == (320,192,8,6,0,0,0), 'unexpected PNG format or size'
    stride = w * 4
    expanded = zlib.decompress(bytes(chunks))
    assert len(expanded) == h * (stride+1), 'decoded scanline extent mismatch'
    prev = bytearray(stride)
    image = bytearray()
    pos = 0
    for _ in range(h):
        filt = expanded[pos]
        pos += 1
        line = bytearray(expanded[pos:pos+stride])
        pos += stride
        assert filt in (0,1,2,3,4), 'invalid PNG filter'
        for x in range(stride):
            left = line[x-4] if x >= 4 else 0
            above = prev[x]
            corner = prev[x-4] if x >= 4 else 0
            if filt == 0: predictor = 0
            elif filt == 1: predictor = left
            elif filt == 2: predictor = above
            elif filt == 3: predictor = (left+above)//2
            else:
                p = left+above-corner
                da,db,dc = abs(p-left),abs(p-above),abs(p-corner)
                predictor = left if da <= db and da <= dc else above if db <= dc else corner
            line[x] = (line[x] + predictor) & 255
        image.extend(line)
        prev = line
    return w,h,bytes(image)

def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack('>I',len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind+payload) & 0xffffffff)

def write_contact(path: Path, sampled: dict[int, bytes]) -> None:
    # Deliberately a presentation only: frames are native canonical; grid is not.
    cell_w,cell_h,gap = 320,192,8
    width,height = 3*cell_w+2*gap,3*cell_h+2*gap
    background = bytes((10,13,19,255))
    rows = bytearray()
    for y in range(height):
        rows.append(0)  # PNG filter none
        cell_row, local_y = divmod(y,cell_h+gap)
        for x in range(width):
            cell_col,local_x = divmod(x,cell_w+gap)
            if cell_row < 3 and cell_col < 3 and local_y < cell_h and local_x < cell_w:
                frame = KEY_FRAMES[cell_row*3+cell_col]
                offset = (local_y*cell_w+local_x)*4
                rows.extend(sampled[frame][offset:offset+4])
            else:
                rows.extend(background)
    ihdr = struct.pack('>IIBBBBB', width,height,8,6,0,0,0)
    result = b'\x89PNG\r\n\x1a\n' + png_chunk(b'IHDR',ihdr) + png_chunk(b'IDAT',zlib.compress(bytes(rows),9)) + png_chunk(b'IEND',b'')
    assert not path.exists(), f'no-overwrite contact-sheet gate: {path}'
    path.write_bytes(result)
    print(f'CONTACT presentation-only {path} SHA-256 {sha(result)}; 3x3 row-major frames {KEY_FRAMES}')

def audit(directory: Path, contact: Path | None = None) -> None:
    record_path = directory/'native-cli-replay-audit.json'
    record = json.loads(record_path.read_text(encoding='utf-8'))
    expected = {'audit_version':1,'kind':'authored-native-cli-replay-not-fm012',
                'complete':True,'source_identity':SOURCE_ID,'genome_identity':GENOME_ID,
                'root_seed':SEED,'frame_begin':0,'frame_end_exclusive':30,'semantic_rate':'12/1',
                'out_of_order_replay':[29,0,17,6,24]}
    for key,value in expected.items():
        assert record.get(key)==value, f'integration audit {key}: expected {value!r}'
    frames=record['frames']
    assert len(frames)==30, 'incomplete frame inventory'
    sampled={}
    ids=set()
    for frame,entry in enumerate(frames):
        name=f'reversible-wound_{frame:06}.png'
        assert entry['frame']==frame and entry['file']==name, f'frame {frame}: order/name mismatch'
        payload=(directory/name).read_bytes()
        assert sha(payload)==entry['encoded_png_sha256'], f'frame {frame}: PNG transport altered'
        w,h,rgba=decode_png(payload)
        identity=sha(DOMAIN+struct.pack('<II',w,h)+rgba)
        assert identity==entry['output_identity'], f'frame {frame}: canonical RGBA8 identity mismatch'
        assert identity not in ids, f'frame {frame}: duplicate full-resolution image'
        ids.add(identity)
        if frame in KEY_FRAMES: sampled[frame]=rgba
    assert len(ids)>=6, 'insufficient distinct temporal frames; review motion parameters'
    print(f'PASS: 30/30 native PNGs, decoded RGBA8 identities and at least six distinct frames; source={SOURCE_ID}, genome={GENOME_ID}')
    print('AUTHOR NATIVE AUDIT: nonchronological probes 29,0,17,6,24; this Python script does not rerun native renderer')
    print('PENDING: genuine GUI FM-012 complete/cancelled manifests, project relink and independent artistic admission')
    if contact is not None: write_contact(contact,sampled)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native_folder',type=Path,help='temporal/ directory extracted from Integration Windows artifact')
    parser.add_argument('--contact',type=Path,help='optional new noncanonical 3x3 presentation PNG path')
    args=parser.parse_args()
    audit(args.native_folder,args.contact)
