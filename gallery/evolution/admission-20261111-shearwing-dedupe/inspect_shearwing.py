#!/usr/bin/env python3
"""Read-only native archive audit + optional offline review, no renderer/proxy or dependencies."""
import argparse
import base64
from collections import Counter
import hashlib
import html
import json
from pathlib import Path
import struct
import zipfile
import zlib

ARCHIVE_SHAS = {
    # Source-exact Integration package (286 original entries).
    '5135f36d2cfca6baa91b27d7b84e0eefea689e95a38232bb112a73f1bb655b8d',
    # Same 286 entries preserved, with three extra noncanonical owner audit files.
    '323c81905c675f55ed12e1274ed6faa6a5212cacc90b0a2e6cd09841ec82f1eb',
    # Newest owner evaluation ZIP: the 289 original files remain byte-identical;
    # three more standalone read-only native-replay files, never canonical substitutes.
    '8cb1ef4789aa2266c905d8ca53a010f4c48f8cb8c842e322f564b93474d5903a',
}
ARCHIVE_COUNTS = {
    '5135f36d2cfca6baa91b27d7b84e0eefea689e95a38232bb112a73f1bb655b8d': 286,
    '323c81905c675f55ed12e1274ed6faa6a5212cacc90b0a2e6cd09841ec82f1eb': 289,
    '8cb1ef4789aa2266c905d8ca53a010f4c48f8cb8c842e322f564b93474d5903a': 292,
}
PROJECT_SHA = '06e6d8f3a14ee17f7b314fa9d591dfa8c5f4e1714b1bba2a13b9e50c9c20049f'
CONTACT_SHA = '70f7f437e263072ad37b1537e0d839c03cdf801a2b9efe9865fed6db48d97c0d'
CONTACT_MANIFEST_SHA = 'd003ead4cc1d02d612f71a3cd53d3bc7b36463ec71cdca8463981b0cf60c9745'
BATCH_SHA = '20c1a2a55064127311872552129f9854a677d3577904d60aeff280ed4eb21033'
SOURCE_SHA = '4d62b0c7e04f169110928e602af16b58370c0c26eddd313962beadaf02d1c59c'
SOURCE_ID = '31c65bdb62b16ce1c890ac5f4beeca7910dc26e5ad2509b6ca186d1faa34ca1b'
PARENT_ID = '01217a9e77ea4cd446b7f6142ab6ea71db9bedbcca48425a02d60ea15849524d'
WINNER_SHA = '41a2e7863eecd7ef51d460dd7d7909b220b915ee60941cd2077d5df0bb2a4cf5'
WINNER_ID = '6f149f34cb9eca441c4206de1b5e7b0f035cab3b2ad958e3401f5a1cef2d9c50'
PREFIX = 'FAULTMINE-1.0.0-win64/CANONICAL-REVIEW-EVIDENCE/shearwing-05/'
SELECTED = [0, 2, 8, 35, 46, 65, 71, 74]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verify_png(data, width=384, height=256):
    """Decode 8-bit straight-RGBA PNG scanlines; check CRC, filters and dimensions."""
    assert data[:8] == b'\x89PNG\r\n\x1a\n', 'PNG signature'
    p, header, parts, ended = 8, None, [], False
    while p < len(data):
        assert p + 12 <= len(data), 'PNG truncated'
        n = int.from_bytes(data[p:p+4], 'big')
        tag = data[p+4:p+8]
        assert n <= 8_000_000 and p + 12 + n <= len(data), 'PNG chunk size'
        chunk = data[p+8:p+8+n]
        assert zlib.crc32(tag + chunk) & 0xffffffff == int.from_bytes(data[p+8+n:p+12+n], 'big'), 'PNG CRC'
        if tag == b'IHDR':
            assert header is None and len(chunk) == 13, 'PNG IHDR'
            header = struct.unpack('>IIBBBBB', chunk)
        elif tag == b'IDAT':
            parts.append(chunk)
        elif tag == b'IEND':
            assert n == 0 and p + 12 == len(data), 'PNG trailing content'
            ended = True
        p += 12 + n
    assert ended and header == (width, height, 8, 6, 0, 0, 0), 'PNG format/dimensions'
    raw = zlib.decompress(b''.join(parts))
    stride, bpp = width * 4, 4
    assert len(raw) == (stride + 1) * height, 'PNG inflated byte count'
    previous = bytearray(stride)
    pixels = bytearray()
    for y in range(height):
        start = y * (stride + 1)
        mode = raw[start]
        assert mode in range(5), 'PNG row filter'
        line = bytearray(raw[start+1:start+1+stride])
        for j in range(stride):
            a = line[j-bpp] if j >= bpp else 0
            b = previous[j]
            c = previous[j-bpp] if j >= bpp else 0
            if mode == 0: prediction = 0
            elif mode == 1: prediction = a
            elif mode == 2: prediction = b
            elif mode == 3: prediction = (a + b) // 2
            else:
                center = a + b - c
                da, db, dc = abs(center-a), abs(center-b), abs(center-c)
                prediction = a if da <= db and da <= dc else (b if db <= dc else c)
            line[j] = (line[j] + prediction) & 0xff
        pixels.extend(line)
        previous = line
    return bytes(pixels)


def l1(a, b):
    return sum(abs(x-y) for x,y in zip(a,b))


def diff_rgb(a, b):
    assert len(a) == len(b)
    return round(100 * sum(a[i:i+3] != b[i:i+3] for i in range(0,len(a),4)) / (len(a)//4), 3)


def audit(path):
    archive_sha = sha(path.read_bytes())
    assert archive_sha in ARCHIVE_SHAS, 'not a pinned qualified Integration ZIP; do not mix source heads'
    with zipfile.ZipFile(path) as z:
        # Each qualified variant is an additive packaging revision. Guard member
        # multiplicity/paths separately to reject accidental unsafe repacks.
        names = z.namelist()
        assert len(names) == ARCHIVE_COUNTS[archive_sha], 'qualified archive member count drift'
        assert len(names) == len(set(names)), 'duplicate ZIP member names'
        assert all(not name.startswith('/') and '..' not in name.split('/') for name in names), 'unsafe ZIP member path'
        member = lambda name: z.read(PREFIX + name)
        assert sha(member('batch.fmbatch.json')) == BATCH_SHA, 'native batch receipt drift'
        assert sha(member('batch.fmproj')) == PROJECT_SHA, 'native GUI import project drift'
        assert sha(member('selected-contact-sheet.png')) == CONTACT_SHA, 'native contact sheet drift'
        assert sha(member('selected-contact-sheet.png.fmmanifest.json')) == CONTACT_MANIFEST_SHA, 'native contact manifest drift'
        assert sha(z.read('FAULTMINE-1.0.0-win64/gallery/evolution/05-shearwing-telemetry/source.bmp')) == SOURCE_SHA, 'authored source drift'
        batch = json.loads(member('batch.fmbatch.json'))
        assert (batch['complete'], batch['cancelled'], len(batch['failures']),
                batch['index_begin'], batch['index_end_exclusive'],batch['frame_index'],
                batch['render_mode'],batch['near_duplicate_threshold']) == (True,False,0,0,96,0,'canonical',8192)
        assert (batch['source_identity'],batch['parent_genome_identity'],batch['mutation_seed'],
                batch['radius']) == (SOURCE_ID,PARENT_ID,'f70ab531c9d204e6','high')
        entries = batch['candidates']
        assert len(entries) == 96
        groups = Counter()
        pixel_ids = set()
        genomes = set()
        decoded = {}
        originals = {}
        for i,c in enumerate(entries):
            assert c['descendant_index'] == i, 'nonconsecutive index'
            data = member(f'candidates/candidate_{i:020d}.png')
            pixels = verify_png(data)
            calculated = sha(b'FAULTMINE-SOURCE-RGBA8-v1' + struct.pack('<II',384,256) + pixels)
            assert calculated == c['pixel_identity'], f'canonical RGBA8 drift at {i}'
            assert calculated not in pixel_ids, 'exact duplicate not recorded'
            pixel_ids.add(calculated)
            assert c['genome_identity'] not in genomes, 'duplicate genome not recorded'
            genomes.add(c['genome_identity'])
            assert c['pixel_representative_index'] == i and c['genome_representative_index'] == i
            rep = c['near_representative_index']
            assert isinstance(rep,int) and 0 <= rep <= i, 'invalid near representative'
            assert l1(c['descriptor'],entries[rep]['descriptor']) <= 8192, 'bad near clustering'
            assert entries[rep]['near_representative_index'] == rep, 'indirect near representative'
            groups[rep] += 1
            if i in (0,2,8,24,35,46,65,71,74,81):
                decoded[i] = pixels
                originals[i] = data
            if i in SELECTED:
                assert c['selected'], 'missing selected index'
                assert member(f'selected/candidate_{i:020d}.png') == data, 'selected PNG drift'
            else:
                assert not c['selected'], 'unexpected selection'
        assert len(pixel_ids) == len(genomes) == 96 and len(groups) == 70
        assert sum(v-1 for v in groups.values()) == 26
        assert len([v for v in groups.values() if v > 1]) == 10
        assert groups[65] == 1 and groups[24] == 2 and entries[81]['near_representative_index'] == 24
        assert entries[65]['pixel_identity'] == WINNER_ID
        assert sha(originals[65]) == WINNER_SHA
        assert member('selected/candidate_00000000000000000065.png') == originals[65]
        assert [c['descendant_index'] for c in entries if c['selected']] == SELECTED
        pairs = {(24,81): diff_rgb(decoded[24],decoded[81]),
                 (65,24): diff_rgb(decoded[65],decoded[24]),
                 (65,81): diff_rgb(decoded[65],decoded[81]),
                 (65,35): diff_rgb(decoded[65],decoded[35]),
                 (65,71): diff_rgb(decoded[65],decoded[71])}
        # Two other genuinely native, unadmitted families for compositional context.
        # Re-verify decoded bytes against independently archived native receipts,
        # rather than using invented thumbnails or noncanonical D3D previews.
        comparison = [
            ('bell', 'CANONICAL-REVIEW-EVIDENCE/original-04/silent-bell-canonical.png',
             'CANONICAL-REVIEW-EVIDENCE/original-04/native-replay-receipt.json',
             384, 256, 0, 'e9b492eddbc4d16ee528c18a236b0930c278aa8909ecdca3d68ef07c4e1d657a',
             'd1778bc3d688e5fa5694485954c268cce88c54cc033a6933d3edf2311cf35b8e'),
            ('angle', 'CANONICAL-REVIEW-EVIDENCE/angle-04-release/angle-departure_000018.png',
             'CANONICAL-REVIEW-EVIDENCE/angle-04-release/native-cli-replay-audit.json',
             320, 192, 18, '92e64eee8e4ed7e27d7b61f7ec8cba0a676510209efa96a21ad9bca5fd835475',
             '88e8f3c2a99fa38ed9a018fd3afd52193c1e68173beb34cc8ea933e2a86a3dbb'),
        ]
        base = 'FAULTMINE-1.0.0-win64/'
        for key, image_path, receipt_path, width, height, frame, encoded_sha, pixel_id in comparison:
            data = z.read(base + image_path)
            assert sha(data) == encoded_sha, f'{key} archived image drift'
            raw = verify_png(data, width, height)
            calculated = sha(b'FAULTMINE-SOURCE-RGBA8-v1' + struct.pack('<II', width, height) + raw)
            assert calculated == pixel_id, f'{key} decoded canonical identity drift'
            receipt = json.loads(z.read(base + receipt_path))
            record = receipt if frame == 0 else receipt['frames'][frame]
            assert record.get('frame') == frame, f'{key} frame ordering drift'
            assert record['encoded_png_sha256'] == encoded_sha, f'{key} native encoded receipt drift'
            assert record['output_identity'] == pixel_id, f'{key} native pixel receipt drift'
            originals[key] = data
        result = {'archive_sha256': archive_sha, 'native_project_sha256': PROJECT_SHA,
                  'native_contact_sha256': CONTACT_SHA,
                  'requested':96,'completed':96,'failed':0,'unique_genomes':len(genomes),
                  'unique_decoded_images':len(pixel_ids),'near_representatives':len(groups),
                  'near_redundant_unique_images':26,'multi_member_groups':10,
                  'largest_near_group':max(groups.values()),'selected':65,
                  'winner_pixel_identity':WINNER_ID,
                  'changed_rgb_percent':{f'{a}-{b}':v for (a,b),v in pairs.items()}}
        return result, originals


def review_page(output, result, originals):
    # All eight authentic FM-014 novelty picks, then one near-identical pair for rejection.
    info = [('0','novelty pick, interrupted telemetry'),
            ('2','novelty pick, fractured pale wing'),
            ('8','novelty pick, saturated rainbow raster; reject for lost silhouette'),
            ('35','novelty pick, dense twin-wing'),
            ('46','novelty pick, solid red saturation; reject for lost silhouette'),
            ('65','author-curated winner; two legible separated wing ribbons'),
            ('71','novelty pick, dispersed wing fragments'),
            ('74','novelty pick, spare partial echo'),
            ('24','non-selected wing alternative'),
            ('81','non-selected near duplicate of 24; reject'),
            ('bell','SILENT BELL original Draft #64, native frame 0; centered casting, distinct from wing passages'),
            ('angle','ANGLE temporal Draft #50, native frame 18; folded planes, distinct from wing passages')]
    cards = '\n'.join('<figure><img alt="Native canonical specimen #'+i+'" src="data:image/png;base64,'+
                      base64.b64encode(originals[int(i) if i.isdecimal() else i]).decode('ascii')+'"><figcaption><b>#'+i+'</b> — '+html.escape(label)+'</figcaption></figure>' for i,label in info)
    message = (f"96/96 completed; 0 failures; 96 distinct decoded images; 70 near representatives; "
               f"26 near-redundant unique images across 10 multi-member groups. "
               f"#24 vs #81: {result['changed_rgb_percent']['24-81']}% changed RGB pixels; "
               f"#65 vs #24: {result['changed_rgb_percent']['65-24']}%. "
               "Pixel difference is not an aesthetic score. Bell and Angle are unadmitted Draft comparators, not accepted art.")
    page = '<!doctype html><html lang="en"><meta charset="utf-8"><meta http-equiv="Content-Security-Policy" content="default-src \'none\'; img-src data:; style-src \'unsafe-inline\'; base-uri \'none\'; form-action \'none\'"><meta name="viewport" content="width=device-width, initial-scale=1"><title>FAULTMINE native Shearwing admission</title>'
    page += '<style>body{margin:auto;padding:1.2rem;max-width:1150px;background:#12151b;color:#e2e9e8;font:16px/1.5 system-ui}header{margin-bottom:2rem}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,320px),1fr));gap:1rem}figure{margin:0;border:1px solid #454b52;padding:.6rem;background:#1b2029}img{display:block;width:100%;height:auto;image-rendering:pixelated}figcaption{padding:.5rem 0}p{max-width:72ch}small{color:#b7c5ca}</style>'
    page += '<header><h1>Shearwing — native admission comparison with Draft references</h1><p>'+html.escape(message)+'</p><small>Embedded byte-identical Windows CPU canonical PNGs: Shearwing/Bell frame 0 (384×256), Angle frame 18 (320×192). No accepted gallery work represented. Static read-only review, not a new render or physical GUI acceptance.</small></header><main>'+cards+'</main></html>'
    output.write_text(page, encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('zip', type=Path, help='Exact current-main four-family ZIP')
    parser.add_argument('--html', type=Path, help='Generate verified offline contact page')
    args = parser.parse_args()
    if not __debug__:
        parser.exit(2, 'FAIL: Python -O disables integrity assertions; rerun without optimization\n')
    try:
        result, originals = audit(args.zip)
        if args.html:
            review_page(args.html, result, originals)
        print(json.dumps(result, indent=2))
        print('PASS: pinned native canonical archive, project, contact sheet and exact-output indices')
    except (AssertionError,KeyError,ValueError,OSError,zipfile.BadZipFile,zlib.error) as exc:
        parser.exit(1, 'FAIL: '+str(exc)+'\n')

if __name__ == '__main__':
    main()
