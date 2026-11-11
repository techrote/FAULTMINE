#!/usr/bin/env python3
"""Read-only stdlib qualification for curated image #147 (no native engine emulation)."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

HERE = Path(__file__).resolve().parent
W, H = 384, 240
IMAGE_ID = '410e2f936fee7bd17fe678aa472c7578791f46d3644ec6d24c9c398b92736c14'
SOURCE_ID = '8201f3a38e374a241497f238a475072dd125e9f657a7c2c84321511acac1746e'
PARENT_ID = '7c671de28cad6136aeeadf1c2bc113608448536ea2f1b22ca49e8908d86f0ffa'
GENOME_ID = 'fbbff29e7ef02f9993c5fe5feed8cbd0eee9d2d13afb1b05968dd8f1abbc42b0'
INDEX = 147
ENCODED_SHA = 'b96d3031d751fda3e012856889da718ababea40774f596c7dd5a3b99da6cdbf6'
NATIVE_ENCODED_SHA = 'b96d3031d751fda3e012856889da718ababea40774f596c7dd5a3b99da6cdbf6'


def digest(b):
    return hashlib.sha256(b).hexdigest()


def png_rgba8(raw):
    """Bounded native RGBA8 PNG decode: CRC, five row filters, exact dimensions."""
    assert raw.startswith(b'\x89PNG\r\n\x1a\n')
    pos = 8; header = None; chunks = []; ended = False
    while pos < len(raw):
        assert pos+12 <= len(raw), 'truncated PNG chunk'
        size = int.from_bytes(raw[pos:pos+4], 'big')
        tag = raw[pos+4:pos+8]; end = pos+size+12
        assert size <= 8_000_000 and end <= len(raw), 'bad PNG bounds'
        data = raw[pos+8:pos+8+size]
        assert (zlib.crc32(tag+data)&0xffffffff) == int.from_bytes(raw[end-4:end], 'big'), 'bad CRC'
        if tag == b'IHDR':
            assert header is None and size == 13
            header = struct.unpack('>IIBBBBB',data)
        elif tag == b'IDAT': chunks.append(data)
        elif tag == b'IEND':
            assert size == 0 and end == len(raw), 'PNG trailing data'
            ended = True
            break
        pos = end
    assert ended and header == (W,H,8,6,0,0,0), 'not native 384x240 RGBA8'
    data = zlib.decompress(b''.join(chunks)); stride = W*4
    assert len(data) == H*(stride+1), 'bad decompressed size'
    prev = bytearray(stride); rgba = bytearray()
    for y in range(H):
        start=y*(stride+1); filt=data[start]
        assert 0<=filt<=4, 'bad row filter'
        line=bytearray(data[start+1:start+1+stride])
        for x in range(stride):
            a=line[x-4] if x>=4 else 0
            b=prev[x]
            c=prev[x-4] if x>=4 else 0
            if filt==0: pred=0
            elif filt==1: pred=a
            elif filt==2: pred=b
            elif filt==3: pred=(a+b)//2
            else:
                p=a+b-c; da=abs(p-a); db=abs(p-b); dc=abs(p-c)
                pred=a if da<=db and da<=dc else b if db<=dc else c
            line[x]=(line[x]+pred)&255
        rgba.extend(line);prev=line
    return bytes(rgba)


def verify(native_dir=None):
    genome_file = HERE/'punched-glass-147.genome.json'
    image_file = HERE/'punched-glass-147.png'
    sidecar_file = HERE/'punched-glass-147.fmmanifest.json'
    genome_bytes = genome_file.read_bytes(); genome=json.loads(genome_bytes)
    assert genome_bytes == (json.dumps(genome,separators=(',',':'),ensure_ascii=False)+'\n').encode('utf-8'), 'genome not normalized canonical JSON'
    assert digest(genome_bytes) == GENOME_ID
    assert genome['schema_version'] == genome['engine_contract_version'] == 1
    assert genome['root_seed'] == '0123456789abcdef' and len(genome['operators'])==6
    png=image_file.read_bytes(); assert digest(png)==ENCODED_SHA, 'committed PNG transport changed'
    rgba=png_rgba8(png)
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
    assert deriv['seed'] == 'c9a1704efd3268b5'
    assert deriv['descendant_index'] == INDEX and deriv['mutation_radius'] == 'high'
    if native_dir:
        native_dir=Path(native_dir)
        record=json.loads((native_dir/'batch.fmbatch.json').read_text())
        assert record['complete'] is True and len(record['candidates']) == 192
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
