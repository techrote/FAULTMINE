#!/usr/bin/env python3
"""Independent authored-input preflight; it NEVER renders FAULTMINE frames."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
SOURCE_SHA = 'd8bc0cef014b6f7d92b6a53b436c3aa4859eb5b44b8fdb6b52259a812264a6b8'
SOURCE_ID = 'ba533af14e6dc02861ab0d8e55239edcc2358f554e07f460aa4fde9cd590619c'
GENOME_ID = '0de88b0fdfbd357ce09761200d2b96b90c8aa2977a9887a3dd7011315751cba2'
SEED = 'e30bd49c7a612f08'
PATH = 'gallery/temporal/03-the-wound-that-counts-backwards/source.bmp'
TYPES = ('fault.channel-offset', 'temporal.tearing-phase', 'temporal.trail',
         'temporal.channel-phase', 'temporal.feedback-blend', 'colour.quantize',
         'temporal.timeline-rate')
KEYS = [0, 4, 8, 12, 16, 20, 24, 27, 29]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def strict_pairs(pairs):
    obj = {}
    for key, value in pairs:
        if key in obj:
            raise ValueError('Duplicate JSON key: ' + key)
        obj[key] = value
    return obj


def parse_compact(path: Path):
    raw = path.read_bytes().replace(b'\r\n', b'\n')
    data = json.loads(raw.decode('utf-8'), object_pairs_hook=strict_pairs)
    canonical = (json.dumps(data, separators=(',', ':'), ensure_ascii=False) + '\n').encode()
    assert raw == canonical, path.name + ' must be LF-terminated canonical compact JSON'
    return data, canonical


def source_identity(payload: bytes) -> str:
    if len(payload) < 54 or payload[:2] != b'BM':
        raise ValueError('invalid BMP')
    file_size, = struct.unpack_from('<I', payload, 2)
    offset, = struct.unpack_from('<I', payload, 10)
    hsize, width, height, planes, bpp, compress = struct.unpack_from('<IiiHHI', payload, 14)
    if (file_size, offset, hsize, width, height, planes, bpp, compress) != (
            len(payload), 54, 40, 320, 192, 1, 24, 0):
        raise ValueError('unsupported BMP header/dimensions')
    stride = (width * 3 + 3) & ~3
    if len(payload) != 54 + height * stride:
        raise ValueError('truncated or overlong BMP')
    rgba = bytearray(width * height * 4)
    for y in range(height):
        start = offset + (height - 1 - y) * stride
        for x in range(width):
            b, g, r = payload[start + 3*x:start + 3*x + 3]
            out = 4 * (y * width + x)
            rgba[out:out+4] = bytes((r, g, b, 255))
    return sha(b'FAULTMINE-SOURCE-RGBA8-v1' + struct.pack('<II', width, height) + rgba)


def main():
    source = (HERE / 'source.bmp').read_bytes()
    assert sha(source) == SOURCE_SHA, 'encoded source changed'
    assert source_identity(source) == SOURCE_ID, 'decoded RGBA8 source changed'
    genome, raw = parse_compact(HERE / 'genome.json')
    assert sha(raw) == GENOME_ID, 'canonical genome identity mismatch'
    assert (genome['schema_version'], genome['engine_contract_version'], genome['root_seed']) == (1, 1, SEED)
    ops = genome['operators']
    assert tuple(o['type_id'] for o in ops) == TYPES
    assert len({o['instance_id'] for o in ops}) == 7
    assert all(re.fullmatch('[0-9a-f]{32}', o['instance_id']) for o in ops)
    assert all(o['enabled'] is True and o['type_version'] == 1 for o in ops)
    for o in ops:
        assert list(o['parameters']) == sorted(o['parameters'])
        assert all(v['kind'] in ('string','u64','i64') for v in o['parameters'].values())
    assert ops[3]['parameters']['shape']['value'] == 'sample-hold'
    assert ops[3]['parameters']['period_frames']['value'] == 5
    assert ops[-1]['parameters'] == {
        'rate_den': {'kind':'u64','value':1},
        'rate_num': {'kind':'u64','value':12}}
    project, _ = parse_compact(HERE / 'the-wound-that-counts-backwards.fmproj')
    assert project['project_version'] == 2
    assert project['source'] == {'path': PATH, 'identity': SOURCE_ID}
    assert project['genome'] == genome and project['locks'] == []
    assert project['session']['proxy_method_version'] == 1
    lineage = project['lineage']
    assert lineage['active_genome_identity'] == GENOME_ID
    assert len(lineage['specimens']) == 1
    specimen = lineage['specimens'][0]
    assert specimen['genome_identity'] == GENOME_ID and specimen['source_identity'] == SOURCE_ID
    assert specimen['genome'] == genome and specimen['creation_ordinal'] == 0
    assert specimen['derivation'] == {
        'kind':'manual-root','parents':[],'policy_version':0,
        'seed':None,'descendant_index':None,'radius':None}
    meta = json.loads((HERE / 'artwork.json').read_text(encoding='utf-8'),
                      object_pairs_hook=strict_pairs)
    assert meta['artwork_manifest_version'] == 1
    assert (meta['source_encoding_sha256'], meta['source_identity'],
            meta['genome_identity'], meta['root_seed']) == (SOURCE_SHA,SOURCE_ID,GENOME_ID,SEED)
    assert (meta['frame_begin'],meta['frame_end_exclusive'],
            meta['rate_num'],meta['rate_den']) == (0,30,12,1)
    assert meta['recommended_key_frames'] == KEYS
    assert meta['canonical_frame_status'] == 'pending-native-windows-cpu-render'
    try:
        source_identity(source[:-1])
    except ValueError:
        pass
    else:
        raise AssertionError('truncated BMP accepted')
    damaged = bytearray(source)
    damaged[1024] ^= 1
    assert sha(damaged) != SOURCE_SHA and source_identity(damaged) != SOURCE_ID
    try:
        json.loads('{"x":1,"x":2}', object_pairs_hook=strict_pairs)
    except ValueError:
        pass
    else:
        raise AssertionError('duplicate-key guard failed')
    print('PASS: immutable BMP, normalized RGBA8 ID, genome v1 canonical SHA, project v2 root, typed stack, temporal metadata')
    print('PASS: damaged source and duplicate JSON rejection')
    print('NOT RUN: Windows-native canonical sequence, Debug/Release replay, FM-012 GUI export, visual output acceptance')


if __name__ == '__main__':
    main()
