#!/usr/bin/env python3
"""Preflight authored inputs. DOES NOT emulate or claim native canonical frames."""
from pathlib import Path
import hashlib
import json
import re
import struct

HERE = Path(__file__).resolve().parent
SOURCE_SHA = '9ffbeb836d5437487d45dce6cdb394d7d2c08992edb9deba387cab8025a1986f'
SOURCE_ID = '906aaeb14df9d961ae302c641b053e7db5cf5e13305abeb67ea438733d736d60'
GENOME_ID = '0a13596eea4f9a872d470f66f9e70ae9dca1ee004d27ff773f45a55c6ecb923e'
SEED = '67da08c2be71fa30'
sha = lambda data: hashlib.sha256(data).hexdigest()


def canonical_json(path):
    raw = path.read_bytes()
    data = json.loads(raw.decode('utf-8'))
    canonical = (json.dumps(data, separators=(',', ':'), ensure_ascii=False) + '\n').encode()
    assert raw.replace(b'\r\n', b'\n') == canonical, f'{path.name}: not canonical compact JSON'
    return data, canonical


def normalized_source():
    raw = (HERE / 'source.bmp').read_bytes()
    assert sha(raw) == SOURCE_SHA, 'source image byte drift'
    assert len(raw) >= 54 and raw[:2] == b'BM'
    size, pixel_offset = struct.unpack_from('<IxxxxI', raw, 2)
    header, width, height, planes, bpp, compression = struct.unpack_from('<IiiHHI', raw, 14)
    assert (size, pixel_offset, header, width, height, planes, bpp, compression) == (
        len(raw), 54, 40, 288, 192, 1, 24, 0)
    stride = (width * 3 + 3) & ~3
    assert len(raw) == 54 + height * stride
    rgba = bytearray()
    for y in range(height):
        start = pixel_offset + (height - 1 - y) * stride
        for x in range(width):
            b, g, r = raw[start + x * 3:start + x * 3 + 3]
            rgba.extend((r, g, b, 255))
    identity = sha(b'FAULTMINE-SOURCE-RGBA8-v1' + struct.pack('<II', width, height) + rgba)
    assert identity == SOURCE_ID, 'normalized RGBA8 identity drift'
    return identity


def main():
    sid = normalized_source()
    genome, canonical = canonical_json(HERE / 'genome.json')
    assert sha(canonical) == GENOME_ID
    assert (genome['schema_version'], genome['engine_contract_version'], genome['root_seed']) == (1, 1, SEED)
    ops = genome['operators']
    assert len(ops) == len({o['instance_id'] for o in ops}) == 6
    assert all(re.fullmatch('[0-9a-f]{32}', o['instance_id']) for o in ops)
    assert all(o['enabled'] is True and o['type_version'] == 1 for o in ops)
    assert [o['type_id'] for o in ops] == [
        'temporal.phase-drift', 'temporal.partial-refresh',
        'temporal.feedback-displace', 'temporal.channel-phase',
        'colour.quantize', 'temporal.timeline-rate']
    assert all(p['kind'] in ('u64', 'i64', 'string') for o in ops for p in o['parameters'].values())
    assert ops[-1]['parameters'] == {'rate_den': {'kind': 'u64', 'value': 1},
                                     'rate_num': {'kind': 'u64', 'value': 10}}
    project, _ = canonical_json(HERE / 'the-lift-that-forgot-floors.fmproj')
    assert project['project_version'] == 2 and project['genome'] == genome
    assert project['source'] == {'path': 'gallery/temporal/02-the-lift-that-forgot-floors/source.bmp', 'identity': sid}
    assert project['locks'] == [] and project['session']['proxy_method_version'] == 1
    lineage = project['lineage']
    assert lineage['active_genome_identity'] == GENOME_ID
    assert len(lineage['specimens']) == 1
    specimen = lineage['specimens'][0]
    assert specimen['source_identity'] == sid and specimen['genome_identity'] == GENOME_ID
    assert specimen['genome'] == genome and specimen['creation_ordinal'] == 0
    assert specimen['derivation'] == {'kind': 'manual-root', 'parents': [],
         'policy_version': 0, 'seed': None, 'descendant_index': None, 'radius': None}
    art = json.loads((HERE / 'artwork.json').read_text(encoding='utf-8'))
    assert (art['source_encoding_sha256'], art['source_identity'], art['genome_identity'], art['root_seed']) == (
        SOURCE_SHA, SOURCE_ID, GENOME_ID, SEED)
    assert (art['frame_begin'], art['frame_end_exclusive'], art['rate_num'], art['rate_den']) == (0, 24, 10, 1)
    assert art['canonical_frame_status'] == 'pending-native-windows-cpu-render'
    assert art['recommended_key_frames'] == [0, 3, 6, 9, 12, 15, 18, 21, 23]
    print('PASS: exact source BMP and RGBA identity, genome, project v2 lineage, explicit sequence metadata')
    print('NOT RUN: native Windows WIC/schema/CPU temporal export and actual image inspection')


if __name__ == '__main__':
    main()
