#!/usr/bin/env python3
"""Review existing Windows-canonical Seismic #57 picks against Shearwing #53.

No mining or rendering. Only archived native PNG bytes enter the labelled offline
contact sheet. Refuses unpinned/missing/changed evidence. Python 3 stdlib only.
"""
import argparse
import base64
from collections import Counter
import hashlib
import html
import json
from pathlib import Path
import struct
import sys
from zipfile import ZipFile
import zlib

SEISMIC_ZIP_SHA = '16cd21878c8872e134c3c8783f9ea56924800ea37755fa2e7e72c8a2de56f977'
SEISMIC_HEAD = '0d91cea9a74a5db786b27a0bdc1a2fb1cc6ed0c4'  # archived native CI run, not PR head
SEISMIC_MANIFEST_SHA = '85e2faa021132bb803192c2471d2b453f064c71522cfc5bc6beef8551f18cf6d'
SEISMIC_WINNER_SHA = '19629f3ccf873f8eb39cac7f8729c58ddb6a71e27b07039720f0766f32a9b337'
SHEARWING_WINNER_SHA = '41a2e7863eecd7ef51d460dd7d7909b220b915ee60941cd2077d5df0bb2a4cf5'
SEISMIC_WINNER_PIXEL = '492f844f1dc21cd3abf4516aa7c383fc276fbfda72205ae628b03a6b52a443e2'
SHEARWING_WINNER_PIXEL = '6f149f34cb9eca441c4206de1b5e7b0f035cab3b2ad958e3401f5a1cef2d9c50'
SEISMIC_PREFIX = 'seismic-debt-06/'
SHEARWING_PREFIX = 'FAULTMINE-1.0.0-win64/CANONICAL-REVIEW-EVIDENCE/shearwing-05/'
CHOSEN = [0, 1, 3, 12, 14, 24, 33, 84, 97, 101, 102, 125]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def png_rgba(data, dims):
    """Strictly decode 8-bit noninterlaced RGBA PNG, including all PNG row filters."""
    require(data.startswith(b'\x89PNG\r\n\x1a\n'), 'PNG signature')
    pos, hdr, chunks, ended = 8, None, [], False
    while pos < len(data):
        require(pos + 12 <= len(data), 'truncated PNG chunk')
        n = int.from_bytes(data[pos:pos+4], 'big')
        require(n <= 8_000_000 and pos + n + 12 <= len(data), 'PNG size bound')
        kind, payload = data[pos+4:pos+8], data[pos+8:pos+8+n]
        require(zlib.crc32(kind + payload) & 0xffffffff ==
                int.from_bytes(data[pos+8+n:pos+12+n], 'big'), 'PNG CRC')
        if kind == b'IHDR':
            require(hdr is None and n == 13, 'PNG IHDR')
            hdr = struct.unpack('>IIBBBBB', payload)
        if kind == b'IDAT':
            chunks.append(payload)
        pos += n + 12
        if kind == b'IEND':
            require(n == 0 and pos == len(data), 'PNG trailing bytes')
            ended = True
            break
    w, h = dims
    require(ended and hdr == (w, h, 8, 6, 0, 0, 0), 'PNG dimensions/type')
    raw = zlib.decompress(b''.join(chunks))
    stride = 4 * w
    require(len(raw) == h * (stride + 1), 'PNG scanline bytes')
    out, previous = bytearray(), bytearray(stride)
    for y in range(h):
        start = y * (stride + 1)
        filt = raw[start]
        require(0 <= filt <= 4, 'PNG filter')
        row = bytearray(raw[start+1:start+1+stride])
        for x in range(stride):
            a = row[x-4] if x >= 4 else 0
            b = previous[x]
            c = previous[x-4] if x >= 4 else 0
            if filt == 1: predictor = a
            elif filt == 2: predictor = b
            elif filt == 3: predictor = (a+b)//2
            elif filt == 4:
                p = a+b-c
                predictor = min(((abs(p-a), 0, a), (abs(p-b), 1, b), (abs(p-c), 2, c)))[2]
            else: predictor = 0
            row[x] = (row[x] + predictor) & 255
        out.extend(row)
        previous = row
    rgba = bytes(out)
    return sha(b'FAULTMINE-SOURCE-RGBA8-v1' + struct.pack('<II', w, h) + rgba), rgba


def verify(seismic_file, four_file):
    require(sha(seismic_file.read_bytes()) == SEISMIC_ZIP_SHA, 'unrecognized native Seismic archive')
    with ZipFile(seismic_file) as s, ZipFile(four_file) as f:
        names = s.namelist()
        require(len(names) == len(set(names)), 'duplicate ZIP member')
        require(s.read('exact-head.txt').decode().strip() == SEISMIC_HEAD,
                'archive run head mismatch')
        manifest_bytes = s.read(SEISMIC_PREFIX + 'batch.fmbatch.json')
        require(sha(manifest_bytes) == SEISMIC_MANIFEST_SHA, 'manifest bytes mismatch')
        manifest = json.loads(manifest_bytes)
        rows = manifest['candidates']
        require(len(rows) == 128 and manifest['complete'] and not manifest['cancelled']
                and not manifest['failures'] and manifest['render_mode'] == 'canonical'
                and manifest['frame_index'] == 0 and manifest['near_duplicate_threshold'] == 8192,
                'Seismic batch incomplete/noncanonical')
        identities, genomes, pixel_first, near_first = set(), set(), {}, set()
        selected = {}
        for i, row in enumerate(rows):
            require(row['descendant_index'] == i, 'unsorted manifest')
            payload = s.read(SEISMIC_PREFIX + f'candidates/candidate_{i:020}.png')
            pixel, _ = png_rgba(payload, (384, 240))
            require(pixel == row['pixel_identity'], f'canonical decoded pixel mismatch #{i}')
            identities.add(pixel)
            genomes.add(row['genome_identity'])
            require(row['pixel_representative_index'] == pixel_first.setdefault(pixel,i),
                    f'exact dedupe mismatch #{i}')
            if row['pixel_representative_index'] == i and row['near_representative_index'] == i:
                near_first.add(i)
            if row['selected']:
                require(i in CHOSEN, 'unexpected selected index')
                require(s.read(SEISMIC_PREFIX + f'selected/candidate_{i:020}.png') == payload,
                        f'selected still differs from canonical cache #{i}')
                selected[i] = payload
        require(list(selected) == CHOSEN, 'selected list differs from original novelty selections')
        require(len(identities) == 125 and len(genomes) == 128 and len(near_first) == 85,
                'actual native diversity counts differ')
        require(sha(selected[101]) == SEISMIC_WINNER_SHA and
                rows[101]['pixel_identity'] == SEISMIC_WINNER_PIXEL,
                'Seismic winner changed')
        wing = f.read(SHEARWING_PREFIX + 'selected/candidate_00000000000000000065.png')
        require(sha(wing) == SHEARWING_WINNER_SHA and
                png_rgba(wing, (384, 256))[0] == SHEARWING_WINNER_PIXEL,
                'Shearwing comparison not source-exact')
        # Match the same fixed thematic framing, not an invented accepted gallery.
        return manifest, selected, wing


def make_html(manifest, selected, wing):
    cards = []
    for i, payload in selected.items():
        data = base64.b64encode(payload).decode('ascii')
        r = manifest['candidates'][i]
        status = 'CURATED WINNER / HOLD' if i == 101 else ('LEGIBILITY ALTERNATE' if i == 102 else 'NATIVE SELECTION')
        cards.append(f'<figure><img alt="Native canonical Seismic descendant {i}" src="data:image/png;base64,{data}"><figcaption>'
                     f'<strong>#{i:03d}</strong> · {status}<small>RG​BA8 {html.escape(r["pixel_identity"][:16])} · near-rep {r["near_representative_index"]}</small>'
                     '</figcaption></figure>')
    wing_data = base64.b64encode(wing).decode('ascii')
    return ('<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
            '<meta http-equiv="Content-Security-Policy" content="default-src \'none\'; img-src data:; style-src \'unsafe-inline\'">'
            '<title>FAULTMINE — Existing native Seismic admission</title><style>'
            ':root{color-scheme:dark;font-family:system-ui,sans-serif;background:#10151a;color:#e4e8e9}body{max-width:1250px;margin:auto;padding:18px}h1{font-size:clamp(1.6rem,4vw,2.6rem)}'
            'p{line-height:1.5;color:#c5d0d2}.grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px}figure{min-width:0;margin:0;background:#182128;border:1px solid #43525c;padding:7px}'
            'img{display:block;width:100%;height:auto;image-rendering:pixelated}figcaption{padding:9px 2px 3px;font-size:.86rem}small{display:block;color:#adbcc1;font-size:.70rem;overflow-wrap:anywhere}'
            'a{color:#bddfff}.final{display:grid;grid-template-columns:1fr 1fr;gap:14px}.final img{max-width:384px}footer{margin-top:26px;border-top:1px solid #52606b;padding-top:12px;color:#b9c7cb}'
            '@media(max-width:800px){.grid{grid-template-columns:repeat(2,minmax(0,1fr))}.final{grid-template-columns:1fr}}@media(max-width:510px){.grid{grid-template-columns:1fr}}'
            '</style><h1>Seismic Debt — admission comparison</h1>'
            '<p><b>Draft #57, not accepted art.</b> Twelve actual archived Windows-native canonical CPU outputs, exact bytes embedded. '
            '128/128 completed, 0 failed, 128 distinct genomes, 125 distinct decoded RGBA8 images, three exact repeats, '
            '85 descriptor-near representatives (40 additional near-redundant unique images; 32.0% of unique), 12 novelty picks. '
            'Descriptor L1 threshold is 8192 inclusive, not an aesthetic metric.</p><div class="grid">' + ''.join(cards) + '</div>'
            '<h2>Concentrated owner decision</h2><section class="final"><figure><img alt="Seismic native curated descendant 101" src="data:image/png;base64,' + base64.b64encode(selected[101]).decode('ascii') + '"><figcaption><b>Seismic #101</b> — layered fractured registers; original channel separation compromised.</figcaption></figure>'
            '<figure><img alt="Shearwing native curated descendant 65" src="data:image/png;base64,' + wing_data + '"><figcaption><b>Shearwing #65</b> — more explicit wing figure and negative space; competing Draft #53, not admitted.</figcaption></figure></section>'
            '<p><b>Curatorial recommendation:</b> keep #101 as the single Seismic contender but HOLD #57 behind #53. '
            'The alternate #102 preserves channel readability but loses much of the fracture; #14 overwhelms the source in red and #33/#84 are effectively noise or void. '
            'No accepted-gallery comparator is available on main. Human aesthetic judgement and physical Win32 GUI export/cancellation remain open.</p>'
            '<footer>Static offline review only — images are byte-identical to pinned Windows-native archives, not re-rendered or GPU previews. '
            '<a href="https://github.com/techrote/FAULTMINE/pull/57">Artwork #57</a> · '
            '<a href="https://github.com/techrote/FAULTMINE/pull/53">Artwork #53</a></footer></html>')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('seismic_native_zip', type=Path)
    p.add_argument('four_family_zip', type=Path)
    p.add_argument('--html', type=Path, help='New output HTML; refuses overwrite')
    p.add_argument('--negative-test', action='store_true')
    args = p.parse_args()
    manifest, selected, wing = verify(args.seismic_native_zip, args.four_family_zip)
    if args.negative_test:
        corrupted = bytearray(selected[101]); corrupted[-9] ^= 1
        try: png_rgba(bytes(corrupted), (384, 240))
        except ValueError: pass
        else: raise ValueError('corrupt PNG was accepted')
    if args.html:
        require(not args.html.exists(), 'output exists; overwrite refused')
        args.html.write_text(make_html(manifest, selected, wing), encoding='utf-8')
        print('REVIEW:', args.html)
    print('PASS: 128/128 genuine archived canonical PNG identities; 0 failures; 125 decoded unique; 85 near representatives; 12 selections')
    print('PASS: curated #101 and competing #65 pinned and decoded; source archive and run provenance verified')
    print('NOT CLAIMED: new Win64 render, latest #57 head CI, accepted-gallery comparison, Win32 GUI or owner aesthetic approval')


if __name__ == '__main__':
    try: main()
    except (ValueError, KeyError, OSError, IndexError, zlib.error) as e:
        sys.exit('FAIL: ' + str(e))
