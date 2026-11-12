#!/usr/bin/env python3
"""Read-only stdlib qualification for curated image #223 (no native engine emulation)."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

HERE = Path(__file__).resolve().parent
W, H = 384, 240
IMAGE_ID = 'eb70ef28390a69e3e1087471d611b41c94239e4140313ca9b782abd073472c14'
SOURCE_ID = '0023ceb14294cf69b7db062a1afc255ebaa8cc6e2a0119f4bb81d91be053125a'
PARENT_ID = '01217a9e77ea4cd446b7f6142ab6ea71db9bedbcca48425a02d60ea15849524d'
GENOME_ID = '3128292f906dfc15b661d2cc749c628a541a3301138ef07c150722900f71190b'
INDEX = 223
ENCODED_SHA = 'd492bd7a49c74f6ab495a7075889d96fc91336f11fbe7a1bf0aab42f423af81a'
NATIVE_ENCODED_SHA = 'e779818cbb1a3f631d8f4d2479816af9f6496ad46436eb4600479d3cf8cdd70c'


def digest(b):
    return hashlib.sha256(b).hexdigest()


def png_palette_rgba(raw):
    """Bounded PNG indexed-colour decoder: CRC, filters, palette, no interlace."""
    assert raw.startswith(b'\x89PNG\r\n\x1a\n')
    pos = 8; ihdr = None; palette = None; trans = b''; chunks = []; saw_end = False
    while pos < len(raw):
        assert pos + 12 <= len(raw), 'truncated chunk header'
        size = int.from_bytes(raw[pos:pos+4], 'big')
        typ = raw[pos+4:pos+8]; end = pos+size+12
        assert size <= 8_000_000 and end <= len(raw), 'oversize/truncated chunk'
        data = raw[pos+8:pos+8+size]
        assert (zlib.crc32(typ+data) & 0xffffffff) == int.from_bytes(raw[end-4:end], 'big'), 'PNG CRC mismatch'
        if typ == b'IHDR':
            assert ihdr is None and size == 13
            ihdr = struct.unpack('>IIBBBBB', data)
        elif typ == b'PLTE': palette = data
        elif typ == b'tRNS': trans = data
        elif typ == b'IDAT': chunks.append(data)
        elif typ == b'IEND': saw_end = True; break
        pos = end
    assert saw_end and ihdr == (W,H,8,3,0,0,0), 'expected exactly 384x240 indexed 8-bit PNG'
    assert palette is not None and len(palette) % 3 == 0 and 0 < len(palette) <= 768
    assert len(trans) <= len(palette)//3
    decoded = zlib.decompress(b''.join(chunks)); stride = W
    assert len(decoded) == H * (stride + 1), 'unexpected pixel payload'
    rgba = bytearray(); prev = bytearray(stride); off = 0
    for _ in range(H):
        filt = decoded[off]; off += 1
        line = bytearray(decoded[off:off+stride]); off += stride
        assert 0 <= filt <= 4, 'unsupported PNG filter'
        for i in range(stride):
            a = line[i-1] if i else 0; b=prev[i]; c=prev[i-1] if i else 0
            if filt == 1: pred=a
            elif filt == 2: pred=b
            elif filt == 3: pred=(a+b)//2
            elif filt == 4:
                p = a+b-c; da=abs(p-a); db=abs(p-b); dc=abs(p-c)
                pred = a if da<=db and da<=dc else b if db<=dc else c
            else: pred=0
            line[i] = (line[i]+pred) & 255
        for idx in line:
            j = idx*3
            assert j+2 < len(palette)
            rgba.extend((palette[j],palette[j+1],palette[j+2],trans[idx] if idx < len(trans) else 255))
        prev = line
    return bytes(rgba)


def verify(native_dir=None):
    genome_file = HERE/'ossuary-transmission-223.genome.json'
    image_file = HERE/'ossuary-transmission-223.png'
    sidecar_file = HERE/'ossuary-transmission-223.fmmanifest.json'
    genome_bytes = genome_file.read_bytes(); genome=json.loads(genome_bytes)
    assert genome_bytes == (json.dumps(genome,separators=(',',':'),ensure_ascii=False)+'\n').encode('utf-8'), 'genome not normalized canonical JSON'
    assert digest(genome_bytes) == GENOME_ID
    assert genome['schema_version'] == genome['engine_contract_version'] == 1
    assert genome['root_seed'] == '0123456789abcdef' and len(genome['operators'])==4
    png=image_file.read_bytes(); assert digest(png)==ENCODED_SHA, 'committed PNG transport changed'
    rgba=png_palette_rgba(png)
    assert digest(b'FAULTMINE-SOURCE-RGBA8-v1' + struct.pack('<II',W,H) + rgba) == IMAGE_ID, 'decoded RGBA not canonical winner'
    sidecar=json.loads(sidecar_file.read_text('utf-8'))
    assert sidecar['manifest_version'] == 1 and sidecar['kind'] == 'still'
    assert sidecar['source']['identity'] == SOURCE_ID
    assert sidecar['genome']['identity'] == GENOME_ID
    assert sidecar['genome']['canonical'] == genome
    assert sidecar['genome']['root_seed'] == '0123456789abcdef'
    assert sidecar['output']['image_identity'] == IMAGE_ID
    assert (sidecar['output']['width'],sidecar['output']['height']) == (W,H)
    assert sidecar['output']['canonical_full_resolution'] is True
    assert sidecar['still']['frame'] == 0
    deriv = sidecar['derivation']
    assert deriv['kind'] == 'mutation' and deriv['parents'] == [PARENT_ID]
    assert deriv['seed'] == '4d13b09ec72a8851'
    assert deriv['descendant_index'] == INDEX and deriv['mutation_radius'] == 'high'
    if native_dir:
        native_dir=Path(native_dir)
        record=json.loads((native_dir/'batch.fmbatch.json').read_text())
        assert record['complete'] is True and len(record['candidates']) == 384
        c=record['candidates'][INDEX]
        assert c['selected'] and c['genome_identity'] == GENOME_ID
        assert c['pixel_identity'] == IMAGE_ID and c['canonical_genome']+'\n' == genome_bytes.decode()
        assert c['pixel_representative_index'] == c['near_representative_index'] == INDEX
        native=native_dir/'selected'/f'candidate_{INDEX:020}.png'
        assert digest(native.read_bytes()) == NATIVE_ENCODED_SHA, 'native container unexpectedly changed'
        # The native PNG has already been independently decoded against the manifest by the FM-014 auditor.
        assert (native_dir/'selected'/f'candidate_{INDEX:020}.png.fmmanifest.json').read_bytes() == sidecar_file.read_bytes()
    return {'status':'PASS','index':INDEX,'pixel_identity':IMAGE_ID,'genome_identity':GENOME_ID,'encoded_png_sha256':ENCODED_SHA,'native_evidence_compared':bool(native_dir)}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--native-dir',default=None)
    args=p.parse_args();print(json.dumps(verify(args.native_dir),indent=2))
