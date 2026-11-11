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
INDEX = 101
W, H = 384, 240
SOURCE_ID = '298051e0e8c048f36f4ac830baedf989e3daf4907fa360d75f59c8e3450dec88'
PARENT_ID = '01217a9e77ea4cd446b7f6142ab6ea71db9bedbcca48425a02d60ea15849524d'
GENOME_ID = 'aebb39166b6c61da4d847b3302021f393406c9de038df136b8097c1c28650acf'
PIXEL_ID = '492f844f1dc21cd3abf4516aa7c383fc276fbfda72205ae628b03a6b52a443e2'
PNG_SHA = '19629f3ccf873f8eb39cac7f8729c58ddb6a71e27b07039720f0766f32a9b337'
CANDIDATE_ID = 'a5249d5c88129e0f0fd78d11769d68570304ccde7845dffedb9b1f0e12fe60c1'
SEED = '6ac9f2e430bd7185'

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
    genome_bytes = (HERE / 'the-broken-register.genome.json').read_bytes()
    genome = json.loads(genome_bytes)
    assert genome_bytes == (json.dumps(genome, separators=(',', ':'), ensure_ascii=False) + '\n').encode('utf8')
    assert digest(genome_bytes) == GENOME_ID, 'genome identity changed'
    assert genome['schema_version'] == genome['engine_contract_version'] == 1
    assert genome['root_seed'] == '0123456789abcdef'
    encoded = (HERE / 'the-broken-register.png').read_bytes()
    assert digest(encoded) == PNG_SHA, 'archived native PNG bytes changed'
    pixels = decode_rgba_png(encoded)
    assert digest(b'FAULTMINE-SOURCE-RGBA8-v1' + struct.pack('<II', W, H) + pixels) == PIXEL_ID
    manifest_bytes = (HERE / 'the-broken-register.png.fmmanifest.json').read_bytes()
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
    assert digest((ROOT / 'source.bmp').read_bytes()) == 'c751316e7615d53cf8a43e79e3a8395ec63f49392e7d16e37be16ad5f100f875'
    record = json.loads((HERE / 'batch.fmbatch.json').read_text('utf8'))
    assert digest((HERE / 'atlas.png').read_bytes()) == 'b0f17a3dfdd8574a04424860ae2b48a355904802dfdde5f9e53d0a5ff4580cc1'
    assert json.loads((HERE / 'atlas.png.fmmanifest.json').read_text('utf8'))['kind'] == 'contact-sheet'
    assert digest((HERE / 'batch.fmbatch.json').read_bytes()) == '85e2faa021132bb803192c2471d2b453f064c71522cfc5bc6beef8551f18cf6d'
    assert len({c['pixel_identity'] for c in record['candidates']}) == 125
    assert sum(c['near_representative_index'] == c['descendant_index'] for c in record['candidates']) == 85
    assert len([c for c in record['candidates'] if c['selected']]) == 12
    candidate = record['candidates'][INDEX]
    assert candidate['candidate_identity'] == CANDIDATE_ID and candidate['selected']
    assert candidate['novelty_distance'] == 72780
    audit = json.loads((HERE / 'native-audit.json').read_text('utf8'))
    assert audit['completed'] == 128 and audit['failed'] == 0
    assert audit['debug_release_readonly_replays'] == 24
    assert audit['verified_resume_manifest_sha256'] == '85e2faa021132bb803192c2471d2b453f064c71522cfc5bc6beef8551f18cf6d'
    if native_dir is not None:
        native_dir = Path(native_dir)
        record = json.loads((native_dir / 'batch.fmbatch.json').read_text('utf8'))
        assert record['complete'] and not record['cancelled']
        assert record['source_identity'] == SOURCE_ID and record['parent_genome_identity'] == PARENT_ID
        assert record['mutation_seed'] == SEED and record['index_end_exclusive'] == 128
        assert len(record['candidates']) == 128 and len(record['failures']) == 0
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
