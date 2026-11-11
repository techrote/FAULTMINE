#!/usr/bin/env python3
"""Native ART-TEMP-005 APNG integrity, frame order and fail-closed regression."""
from __future__ import annotations
import hashlib
import struct
import sys
import tempfile
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

import make_native_apng as app
from verify_native_frames import DOMAIN, decode_png, png_chunk


def parse_apng(raw: bytes):
    assert raw.startswith(app.PNG_SIGNATURE)
    pos = len(app.PNG_SIGNATURE)
    ihdr = None
    frames = []
    current = None
    seqs = []
    actl = None
    while pos < len(raw):
        if pos + 12 > len(raw): raise AssertionError('APNG truncated chunk')
        n = int.from_bytes(raw[pos:pos+4], 'big')
        typ = raw[pos+4:pos+8]
        end = pos + 12 + n
        assert end <= len(raw)
        payload = raw[pos+8:pos+8+n]
        assert raw[pos:end] == png_chunk(typ,payload)
        if typ == b'IHDR': ihdr = payload
        elif typ == b'acTL': actl = struct.unpack('>II',payload)
        elif typ == b'fcTL':
            if current is not None: frames.append(current)
            current = []
            seq, w,h,x,y,num,den,dispose,blend = struct.unpack('>IIIIIHHBB',payload)
            assert (w,h,x,y,num,den,dispose,blend) == (336,224,0,0,1,10,0,0)
            seqs.append(seq)
        elif typ == b'IDAT': current.append(payload)
        elif typ == b'fdAT':
            seq = struct.unpack('>I',payload[:4])[0]
            seqs.append(seq)
            current.append(payload[4:])
        elif typ == b'IEND':
            assert end == len(raw)
            frames.append(current)
        pos = end
    assert actl == (32,0) and len(frames) == 32
    assert seqs == list(range(len(seqs))), seqs
    assert len(seqs) == 63
    return [app.PNG_SIGNATURE + png_chunk(b'IHDR',ihdr) + png_chunk(b'IDAT', b''.join(f)) + png_chunk(b'IEND',b'') for f in frames]


def run(archive: Path):
    with tempfile.TemporaryDirectory() as td:
        out = Path(td)/'review.png'
        digest, rows = app.build(archive,out)
        assert digest == hashlib.sha256(out.read_bytes()).hexdigest()
        recovered = parse_apng(out.read_bytes())
        with ZipFile(archive) as z:
            for i, rec in enumerate(recovered):
                original = z.read(app.unique_entry(z.namelist(),f'seat-count_{i:06d}.png'))
                w,h,rgba = decode_png(original)
                w2,h2,rgba2 = decode_png(rec)
                assert (w,h,rgba)==(w2,h2,rgba2),f'APNG frame {i} pixel difference'
                assert hashlib.sha256(DOMAIN+struct.pack('<II',w,h)+rgba2).hexdigest()==rows[i]['output_identity']
        print('PASS: 32/32 APNG frame reconstruction, original native decoded-pixel equality, ordering and 10fps controls')
        try: app.build(archive,out)
        except FileExistsError: pass
        else: raise AssertionError('Output overwrite not refused')
        with ZipFile(archive) as z:
            names=z.namelist(); raw={n:z.read(n) for n in names if app.NATIVE_DIRS[0] in n or app.NATIVE_DIRS[1] in n}
        def reject(replace, part):
            bad=Path(td)/'altered.zip'
            with ZipFile(bad,'w',compression=ZIP_DEFLATED) as z:
                for name,payload in raw.items(): z.writestr(name,replace(name,payload))
            try: app.build(bad,Path(td)/'reject.png')
            except (ValueError,AssertionError) as e:
                assert part in str(e),(str(e),part)
            else: raise AssertionError(f'Corruption passed: {part}')
        reject(lambda name,payload: payload+b'!' if name.endswith('native-cli-replay-audit.json') else payload,'receipt hash')
        reject(lambda name,payload:payload[:-1]+bytes([payload[-1]^1]) if name.endswith('seat-count_000014.png') else payload,'transport mismatch at frame 14')
        print('PASS: no-overwrite, mutated receipt and mutated PNG rejected')


if __name__=='__main__':
    if len(sys.argv)!=2: raise SystemExit('usage: python test_native_apng.py NATIVE-REVIEW.zip')
    run(Path(sys.argv[1]))
