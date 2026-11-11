#!/usr/bin/env python3
"""Read-only independent archive checks, not a replacement for native CPU replay."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
INDEX = 65
W, H = 384, 256
SOURCE_ID = '31c65bdb62b16ce1c890ac5f4beeca7910dc26e5ad2509b6ca186d1faa34ca1b'
PARENT_ID = '01217a9e77ea4cd446b7f6142ab6ea71db9bedbcca48425a02d60ea15849524d'
GENOME_ID = '33d5e6ad5924b3a19fc421ac620386c4e1d232662d1a018d8946889f93f92f78'
PIXEL_ID = '6f149f34cb9eca441c4206de1b5e7b0f035cab3b2ad958e3401f5a1cef2d9c50'
PNG_SHA = '41a2e7863eecd7ef51d460dd7d7909b220b915ee60941cd2077d5df0bb2a4cf5'
CANDIDATE_ID = '205af5069d70415bada7913a67d2e511401b392484dcc1aebb4daa42e4c7de3d'
SEED = 'f70ab531c9d204e6'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def decode_rgba_png(data):
    assert data.startswith(b'\x89PNG\r\n\x1a\n'), 'not PNG'
    pos = 8
    ihdr = None
    idat = []
    iend = False
    while pos < len(data):
        assert pos + 12 <= len(data), 'truncated PNG chunk'
        n = int.from_bytes(data[pos:pos + 4], 'big')
        tag = data[pos + 4:pos + 8]
        end = pos + n + 12
        assert n <= 8_000_000 and end <= len(data), 'PNG bounds'
        chunk = data[pos + 8:pos + 8 + n]
        expected_crc = int.from_bytes(data[end - 4:end], 'big')
        assert (zlib.crc32(tag + chunk) & 0xffffffff) == expected_crc, 'PNG CRC'
        if tag == b'IHDR':
            assert ihdr is None and n == 13
            ihdr = struct.unpack('>IIBBBBB', chunk)
        elif tag == b'IDAT':
            idat.append(chunk)
        elif tag == b'IEND':
            assert n == 0 and end == len(data), 'PNG trailing data'
            iend = True
            break
        pos = end
    assert iend and ihdr == (W, H, 8, 6, 0, 0, 0), 'unsupported image type/dimensions'
    raw = zlib.decompress(b''.join(idat))
    stride = 4 * W
    assert len(raw) == H * (stride + 1), 'decompressed size'
    prior = bytearray(stride)
    result = bytearray()
    for y in range(H):
        start = y * (stride + 1)
        mode = raw[start]
        assert 0 <= mode <= 4, 'PNG filter'
        scan = bytearray(raw[start + 1:start + 1 + stride])
        for i in range(stride):
            a = scan[i - 4] if i >= 4 else 0
            b = prior[i]
            c = prior[i - 4] if i >= 4 else 0
            if mode == 0: prediction = 0
            elif mode == 1: prediction = a
            elif mode == 2: prediction = b
            elif mode == 3: prediction = (a + b) // 2
            else:
                p = a + b - c
                da, db, dc = abs(p - a), abs(p - b), abs(p - c)
                prediction = a if da <= db and da <= dc else (b if db <= dc else c)
            scan[i] = (scan[i] + prediction) & 255
        result.extend(scan)
        prior = scan
    return bytes(result)

def verify(native_dir=None):
    genome_bytes = (HERE / 'shearwing-65.genome.json').read_bytes()
    genome = json.loads(genome_bytes)
    assert genome_bytes == (json.dumps(genome, separators=(',', ':'), ensure_ascii=False) + '\n').encode('utf8')
    assert digest(genome_bytes) == GENOME_ID, 'genome identity changed'
    assert genome['schema_version'] == genome['engine_contract_version'] == 1
    assert genome['root_seed'] == '0123456789abcdef'
    encoded = (HERE / 'shearwing-65.png').read_bytes()
    assert digest(encoded) == PNG_SHA, 'archived native PNG bytes changed'
    pixels = decode_rgba_png(encoded)
    assert digest(b'FAULTMINE-SOURCE-RGBA8-v1' + struct.pack('<II', W, H) + pixels) == PIXEL_ID
    manifest_bytes = (HERE / 'shearwing-65.fmmanifest.json').read_bytes()
    sidecar = json.loads(manifest_bytes)
    assert sidecar['manifest_version'] == 1 and sidecar['kind'] == 'still'
    assert sidecar['source']['identity'] == SOURCE_ID
    assert sidecar['genome']['identity'] == GENOME_ID
    assert sidecar['genome']['canonical'] == genome
    assert sidecar['output']['image_identity'] == PIXEL_ID
    assert sidecar['output']['width'] == W and sidecar['output']['height'] == H
    assert sidecar['output']['canonical_full_resolution'] is True
    assert sidecar['still']['frame'] == 0
    lineage = sidecar['derivation']
    assert lineage['kind'] == 'mutation' and lineage['parents'] == [PARENT_ID]
    assert lineage['seed'] == SEED and lineage['descendant_index'] == INDEX
    assert lineage['mutation_radius'] == 'high' and lineage['policy_version'] == 1
    assert digest((ROOT / 'source.bmp').read_bytes()) == '4d62b0c7e04f169110928e602af16b58370c0c26eddd313962beadaf02d1c59c'
    if native_dir is not None:
        native_dir = Path(native_dir)
        record = json.loads((native_dir / 'batch.fmbatch.json').read_text('utf8'))
        assert record['complete'] and not record['cancelled']
        assert record['source_identity'] == SOURCE_ID and record['parent_genome_identity'] == PARENT_ID
        assert record['mutation_seed'] == SEED and record['index_end_exclusive'] == 96
        assert len(record['candidates']) == 96 and len(record['failures']) == 0
        candidate = record['candidates'][INDEX]
        assert candidate['selected'] and candidate['candidate_identity'] == CANDIDATE_ID
        assert candidate['pixel_identity'] == PIXEL_ID and candidate['genome_identity'] == GENOME_ID
        assert candidate['near_representative_index'] == INDEX
        assert candidate['canonical_genome'] + '\n' == genome_bytes.decode('utf8')
        path = native_dir / 'selected' / f'candidate_{INDEX:020}.png'
        assert path.read_bytes() == encoded
        assert Path(str(path) + '.fmmanifest.json').read_bytes() == manifest_bytes
    return {'status': 'PASS', 'selected': INDEX, 'canonical_rgba8': PIXEL_ID, 'genome': GENOME_ID,
            'native_archive_compared': native_dir is not None}

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--native-dir')
    args = p.parse_args()
    print(json.dumps(verify(args.native_dir), indent=2))
