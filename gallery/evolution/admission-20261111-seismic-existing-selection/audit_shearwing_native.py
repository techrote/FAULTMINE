#!/usr/bin/env python3
"""Independently qualify the existing Windows-native Shearwing FM-014 archive.

No new mining, proxies, rerender, GUI actions, network or author file changes.
Python 3 standard library only. Use with the four-family Windows review ZIP.
"""
import argparse
import json
from pathlib import Path
import struct
import sys
from zipfile import ZipFile

from verify_and_review import descriptor_distance, descriptor_v1, png_rgba, require, sha

BASE = 'FAULTMINE-1.0.0-win64/CANONICAL-REVIEW-EVIDENCE/shearwing-05/'
SOURCE = 'FAULTMINE-1.0.0-win64/gallery/evolution/05-shearwing-telemetry/source.bmp'
SOURCE_SHA = '4d62b0c7e04f169110928e602af16b58370c0c26eddd313962beadaf02d1c59c'
SOURCE_ID = '31c65bdb62b16ce1c890ac5f4beeca7910dc26e5ad2509b6ca186d1faa34ca1b'
PARENT_ID = '01217a9e77ea4cd446b7f6142ab6ea71db9bedbcca48425a02d60ea15849524d'
AUTHORED_ROOT_ID = '634ce6f118512f179f4719770f46413d4e7e0e1ba92d54ab3d8f9e8b219406ca'
ORIGINAL_REVIEW_PROJECT_SHA = 'f5cd89c2b0449f8b3beea8eaaa534ed05e02cf7af3b483884b16468442530eab'
ORIGINAL_REVIEW_PROJECT = 'FAULTMINE-1.0.0-win64/REVIEW-PROJECTS/04-shearwing.fmproj'
WINNER_SHA = '41a2e7863eecd7ef51d460dd7d7909b220b915ee60941cd2077d5df0bb2a4cf5'
WINNER_ID = '6f149f34cb9eca441c4206de1b5e7b0f035cab3b2ad958e3401f5a1cef2d9c50'
SELECTED = [0, 2, 8, 35, 46, 65, 71, 74]
DIMS = (384, 256)


def bmp_source_identity(data):
    """Decode the original uncompressed Windows 24-bit BMP, bottom-up, as RGBA8."""
    require(data[:2] == b'BM' and len(data) >= 54, 'BMP signature/header')
    size = struct.unpack_from('<I', data, 2)[0]
    offset = struct.unpack_from('<I', data, 10)[0]
    header_size = struct.unpack_from('<I', data, 14)[0]
    w, h, planes, bits, compression = struct.unpack_from('<iiHHI', data, 18)
    require(size == len(data) and offset == 54 and header_size == 40
            and (w, h) == DIMS and planes == 1 and bits == 24 and compression == 0,
            'unexpected source BMP layout')
    pitch = (3*w + 3) & ~3
    require(offset + pitch*h == len(data), 'BMP exact payload length')
    rgba = bytearray()
    for y in range(h):
        base = offset + (h-y-1)*pitch
        for x in range(w):
            b, g, r = data[base+3*x:base+3*x+3]
            rgba.extend((r, g, b, 255))
    return sha(b'FAULTMINE-SOURCE-RGBA8-v1' + struct.pack('<II', w, h) + rgba)


def selected_by_farthest_point(rows, representatives):
    """Replay FM-014's first-index then maximum minimum L1 descriptor selection."""
    available = sorted(representatives)
    require(len(available) >= 8, 'insufficient near representatives')
    chosen = []
    for rank in range(8):
        if not rank:
            idx, novelty = available[0], 0
        else:
            dist = {i: min(descriptor_distance(rows[i]['descriptor'],
                      rows[j]['descriptor']) for j in chosen)
                    for i in available if i not in chosen}
            idx = min(dist, key=lambda i: (-dist[i], i, rows[i]['genome_identity']))
            novelty = dist[idx]
        chosen.append(idx)
        require(rows[idx]['novelty_distance'] == novelty,
                f'novelty distance mismatch for descendant {idx}')
    return chosen


def verify_portable_lineage(review, manifest, authored_root):
    """Pin all eight chosen descendants' derivations, not just the active genome.

    A portable review project can have a correct #65 active genome while
    retaining a different or misleading mutation history. That is not an
    admission-ready provenance record.
    """
    specimens = review['lineage']['specimens']
    require(len(specimens) == 10, 'expected two roots and eight chosen descendants')
    require(authored_root['genome_identity'] == AUTHORED_ROOT_ID
            and specimens[0] == authored_root,
            'portable authored root differs from pinned original project')
    require([s['creation_ordinal'] for s in specimens] == list(range(10)),
            'portable lineage creation order changed')
    require(all(s['source_identity'] == SOURCE_ID for s in specimens),
            'portable lineage source identity changed')
    selected, parents = {}, []
    for specimen in specimens:
        derivation = specimen['derivation']
        genome_id = specimen['genome_identity']
        if genome_id == PARENT_ID:
            parents.append(specimen)
        index = derivation['descendant_index']
        if index is None:
            require(derivation == {
                'kind': 'manual-root', 'descendant_index': None,
                'parents': [], 'policy_version': 0, 'radius': None,
                'seed': None}, 'malformed manual root derivation')
            continue
        require(index in SELECTED and index not in selected,
                'unexpected/repeated portable lineage descendant')
        row = manifest['candidates'][index]
        require(specimen['genome'] == json.loads(row['canonical_genome'])
                and genome_id == row['genome_identity']
                and specimen['favourite'] is True,
                f'portable lineage selected genome/favourite mismatch #{index}')
        require(derivation == {
            'kind': 'mutation', 'descendant_index': index,
            'parents': [PARENT_ID], 'policy_version': 1,
            'radius': 'high', 'seed': 'f70ab531c9d204e6'},
            f'portable lineage mutation address mismatch #{index}')
        selected[index] = genome_id
    require(len(parents) == 1
            and parents[0]['genome'] == json.loads(manifest['parent_genome']),
            'portable lineage accepted parent missing or altered')
    require(sorted(selected) == SELECTED, 'portable lineage selections incomplete')
    require(selected[65] == review['lineage']['active_genome_identity'],
            'portable lineage active specimen differs from native #65')


def negative_portable_lineage_tests(review, manifest, authored_root):
    """Fail-closed in-memory adversarial checks; original archive stays read-only."""
    from copy import deepcopy
    for kind in ('seed', 'parent', 'index', 'source', 'genome',
                 'favourite', 'missing', 'reorder', 'extra', 'parent-genome',
                 'root-derivation', 'authored-root-genome',
                 'authored-root-identity', 'authored-root-favourite'):
        altered = deepcopy(review)
        specimens = altered['lineage']['specimens']
        derivation = specimens[7]['derivation']
        if kind == 'seed':
            derivation['seed'] = 'f70ab531c9d204e7'
        elif kind == 'parent':
            derivation['parents'] = ['0' * 64]
        elif kind == 'index':
            derivation['descendant_index'] = 46
        elif kind == 'source':
            specimens[7]['source_identity'] = '0' * 64
        elif kind == 'genome':
            specimens[7]['genome']['root_seed'] = '0000000000000000'
        elif kind == 'favourite':
            specimens[7]['favourite'] = False
        elif kind == 'missing':
            specimens.pop(7)
        elif kind == 'reorder':
            specimens[7]['creation_ordinal'] = 0
        elif kind == 'extra':
            specimens.append(deepcopy(specimens[7]))
        elif kind == 'parent-genome':
            specimens[1]['genome']['root_seed'] = '0000000000000000'
        elif kind == 'root-derivation':
            specimens[0]['derivation']['kind'] = 'mutation'
        elif kind == 'authored-root-genome':
            specimens[0]['genome']['root_seed'] = 'deadbeefdeadbeef'
        elif kind == 'authored-root-identity':
            specimens[0]['genome_identity'] = '0' * 64
        elif kind == 'authored-root-favourite':
            specimens[0]['favourite'] = True
        try:
            verify_portable_lineage(altered, manifest, authored_root)
        except (ValueError, KeyError, IndexError, TypeError):
            continue
        raise ValueError('tampered portable lineage was accepted: ' + kind)

def audit(archive):
    with ZipFile(archive) as z:
        names = z.namelist()
        require(len(names) == len(set(names)), 'duplicate ZIP member')
        source = z.read(SOURCE)
        require(sha(source) == SOURCE_SHA, 'source transport hash mismatch')
        require(bmp_source_identity(source) == SOURCE_ID,
                'decoded normalized source RGBA8 mismatch')
        manifest = json.loads(z.read(BASE + 'batch.fmbatch.json'))
        rows = manifest['candidates']
        require(manifest['manifest_version'] == 1 and manifest['descriptor_version'] == 1
                and manifest['mutation_policy_version'] == 1
                and manifest['source_identity'] == SOURCE_ID
                and manifest['parent_genome_identity'] == PARENT_ID
                and manifest['mutation_seed'] == 'f70ab531c9d204e6'
                and manifest['radius'] == 'high'
                and (manifest['index_begin'], manifest['index_end_exclusive']) == (0, 96)
                and manifest['frame_index'] == 0 and manifest['render_mode'] == 'canonical'
                and manifest['near_duplicate_threshold'] == 8192
                and manifest['requested_selection_count'] == 8
                and manifest['complete'] is True and manifest['cancelled'] is False
                and manifest['failures'] == [] and len(rows) == 96,
                'incomplete/noncanonical/wrong lineage archive')
        require(json.loads(manifest['parent_genome'])['schema_version'] == 1,
                'canonical parent genome schema mismatch')
        project = json.loads(z.read(BASE + 'batch.fmproj'))
        require(project['project_version'] == 2
                and project['source']['identity'] == SOURCE_ID,
                'project v2 source binding mismatch')
        stored_source_path = project['source']['path']
        require(stored_source_path.endswith(
                    'gallery\\evolution\\05-shearwing-telemetry\\source.bmp'),
                'unrecognized original Windows project path')
        genome_first, pixel_first, near_first = {}, {}, []
        selected, chosen_png = [], {}
        for i, row in enumerate(rows):
            require(row['descendant_index'] == i, f'out of order candidate {i}')
            png = z.read(BASE + f'candidates/candidate_{i:020}.png')
            decoded_id, rgba = png_rgba(png, DIMS)
            require(decoded_id == row['pixel_identity'], f'canonical pixels {i}')
            descriptor = descriptor_v1(rgba, DIMS)
            require(descriptor == row['descriptor'], f'descriptor {i}')
            gf = genome_first.setdefault(row['genome_identity'], i)
            pf = pixel_first.setdefault(decoded_id, i)
            require((row['genome_representative_index'],
                     row['pixel_representative_index']) == (gf, pf),
                    f'genome/pixel exact dedupe {i}')
            near = pf if pf != i else next(
                (j for j, d in near_first
                 if descriptor_distance(descriptor, d) <= 8192), i)
            if pf == i and near == i:
                near_first.append((i, descriptor))
            require(row['near_representative_index'] == near,
                    f'first qualifying descriptor-near representative {i}')
            if row['selected']:
                selected.append(i)
                still = z.read(BASE + f'selected/candidate_{i:020}.png')
                require(still == png, f'altered selected still {i}')
                chosen_png[i] = still
                audit_genome = json.loads(z.read(BASE +
                                      f'audit-genome-{i:020}.json'))
                require(audit_genome == json.loads(row['canonical_genome']),
                        f'genome mismatch on audited selected {i}')
        require(len(genome_first) == len(pixel_first) == 96 and len(near_first) == 70,
                'native diversity counts changed')
        rank = selected_by_farthest_point(rows, [i for i, _ in near_first])
        require(selected == SELECTED and set(rank) == set(SELECTED),
                'independent farthest-point selection disagrees')
        require(sha(chosen_png[65]) == WINNER_SHA
                and rows[65]['pixel_identity'] == WINNER_ID, 'winner #65 changed')
        # The immutable native batch project records its original runner's
        # absolute path. The separately packaged, author-genome-exact WINNER
        # review project is the intended relocatable owner entrypoint.
        review_path = ('FAULTMINE-1.0.0-win64/REVIEW-PROJECTS/'
                       '04-shearwing-WINNER-65.fmproj')
        review = json.loads(z.read(review_path))
        relative_source = 'gallery/evolution/05-shearwing-telemetry/source.bmp'
        require(review['project_version'] == 2
                and review['source'] == {'path': relative_source,
                                        'identity': SOURCE_ID}
                and 'FAULTMINE-1.0.0-win64/' + relative_source in names,
                'portable winner project has wrong/missing source')
        require(review['genome'] == json.loads(rows[65]['canonical_genome'])
                and review['lineage']['active_genome_identity'] ==
                    rows[65]['genome_identity'],
                'portable winner project active genome/lineage mismatch')
        original_project_bytes = z.read(ORIGINAL_REVIEW_PROJECT)
        require(sha(original_project_bytes) == ORIGINAL_REVIEW_PROJECT_SHA,
                'authored project changed from known portable review source')
        original_project = json.loads(original_project_bytes)
        require(original_project['source']['identity'] == SOURCE_ID,
                'original portable review project source changed')
        authored_root = original_project['lineage']['specimens'][0]
        verify_portable_lineage(review, manifest, authored_root)
        receipt = json.loads(z.read(
            BASE + 'selected/candidate_00000000000000000065.png.fmmanifest.json'))
        require(receipt['kind'] == 'still'
                and receipt['source']['identity'] == SOURCE_ID
                and receipt['genome']['identity'] == rows[65]['genome_identity']
                and receipt['genome']['canonical'] == review['genome']
                and receipt['output']['image_identity'] == WINNER_ID
                and receipt['output']['canonical_full_resolution'] is True
                and receipt['still']['frame'] == 0
                and receipt['derivation']['descendant_index'] == 65,
                'native winner still receipt/project/lineage mismatch')
        return {
            'requested': 96, 'completed': 96, 'failures': 0,
            'unique_genomes': len(genome_first),
            'unique_decoded_images': len(pixel_first),
            'exact_duplicate_images': 96 - len(pixel_first),
            'near_representatives': len(near_first),
            'near_redundant_unique_images': len(pixel_first) - len(near_first),
            'near_redundant_fraction': '26/96 (27.08%)',
            'selected_indices': selected, 'diversity_rank_order': rank,
            'winner_index': 65, 'winner_png_sha256': WINNER_SHA,
            'winner_rgba8_identity': WINNER_ID,
            'original_project_source_path': stored_source_path,
            'portable_source_path': relative_source,
            'portable_winner_project_sha256': sha(z.read(review_path)),
            'portable_project_gui_open': 'physical GUI verification outstanding'
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('four_family_native_zip', type=Path)
    parser.add_argument('--negative-test', action='store_true')
    args = parser.parse_args()
    result = audit(args.four_family_native_zip)
    if args.negative_test:
        with ZipFile(args.four_family_native_zip) as z:
            native_manifest = json.loads(z.read(BASE + 'batch.fmbatch.json'))
            winner_project = json.loads(z.read('FAULTMINE-1.0.0-win64/REVIEW-PROJECTS/04-shearwing-WINNER-65.fmproj'))
            original_bytes = z.read(ORIGINAL_REVIEW_PROJECT)
            require(sha(original_bytes) == ORIGINAL_REVIEW_PROJECT_SHA,
                    'original portable review project checksum changed')
            authored_root = json.loads(original_bytes)['lineage']['specimens'][0]
            negative_portable_lineage_tests(winner_project, native_manifest, authored_root)
        with ZipFile(args.four_family_native_zip) as z:
            altered = bytearray(z.read(BASE + 'selected/candidate_00000000000000000065.png'))
        altered[-9] ^= 1
        try:
            png_rgba(bytes(altered), DIMS)
        except ValueError:
            pass
        else:
            raise ValueError('tampered native selected PNG was not rejected')
    print(json.dumps(result, indent=2, sort_keys=True))
    print('PASS: native source, 96 decoded canonical stills/descriptors, 70 near representatives, 8 selected/genome identities, full portable winner lineage including pinned authored root and native export receipt')
    print('NOT CLAIMED: new Windows render, physical GUI/relink/export or owner artistic admission')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError, IndexError, TypeError) as exc:
        sys.exit('FAIL: ' + str(exc))
