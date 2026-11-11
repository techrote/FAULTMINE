#!/usr/bin/env python3
"""Verify archived ART-TEMP-005 native frames; build a standalone offline review HTML.

Usage: python review_native.py path/to/FAULTMINE-2026-11-11-main-four-family-evaluation-win64.zip
The HTML is a presentation, NOT a canonical output or Win32 GUI acceptance.
"""
import base64
import hashlib
import json
import sys
from pathlib import Path
from zipfile import ZipFile

RECEIPT_SHA256 = '1ee290b15ffb8a07d4416b51185d223935c321bfea356ddc62e750ab12f4b590'
SOURCE_ID = 'cb8df081f6f79c6ec75e974dc0f20cb227aa79e97c17dcaeadf554b311eda342'
GENOME_ID = 'e8f70bc1b738b5557d2c0e20beb8d1af49b4c1c0894c4d8bbe1758a11acc3850'
PREFIX = '/CANONICAL-REVIEW-EVIDENCE/seat-05-release/'


def build(archive: Path, target: Path) -> None:
    with ZipFile(archive) as z:
        entries = [x for x in z.namelist() if PREFIX in x and not x.endswith('/')]
        receipts = [x for x in entries if x.endswith(PREFIX + 'native-cli-replay-audit.json')]
        if len(receipts) != 1:
            raise ValueError('Expected exactly one pinned native replay receipt in package')
        raw_receipt = z.read(receipts[0])
        if hashlib.sha256(raw_receipt).hexdigest() != RECEIPT_SHA256:
            raise ValueError('Native replay receipt changed: artifact is not the pinned exact-head sequence')
        record = json.loads(raw_receipt)
        if any((record.get('source_identity') != SOURCE_ID,
                record.get('genome_identity') != GENOME_ID,
                record.get('root_seed') != '9a5d7e63c204b811',
                record.get('semantic_rate') != '10/1',
                record.get('frame_begin') != 0,
                record.get('frame_end_exclusive') != 32,
                record.get('complete') is not True,
                record.get('native_configuration') != 'Release',
                record.get('kind') != 'authored-native-cli-replay-not-fm012')):
            raise ValueError('Native receipt provenance/range is invalid')
        if len(record.get('frames', [])) != 32:
            raise ValueError('Native frame set is incomplete')
        frames = []
        seen_ids = set()
        for n, row in enumerate(record['frames']):
            filename = f'seat-count_{n:06d}.png'
            matches = [x for x in entries if x.endswith(PREFIX + filename)]
            if len(matches) != 1 or row['file'] != filename or row['frame'] != n:
                raise ValueError(f'Missing, duplicate or misordered frame {n}')
            raw = z.read(matches[0])
            if hashlib.sha256(raw).hexdigest() != row['encoded_png_sha256']:
                raise ValueError(f'Canonical PNG SHA-256 mismatch, frame {n}')
            if raw[:8] != b'\x89PNG\r\n\x1a\n' or tuple(int.from_bytes(raw[k:k+4], 'big') for k in (16,20)) != (336,224):
                raise ValueError(f'Unexpected native PNG dimensions/format, frame {n}')
            if row['output_identity'] in seen_ids:
                raise ValueError(f'Repeated canonical output identity: frame {n}')
            seen_ids.add(row['output_identity'])
            frames.append({'src':'data:image/png;base64,' + base64.b64encode(raw).decode('ascii'),
                           'id':row['output_identity'],'sha':row['encoded_png_sha256']})
    page = HTML.replace('__FRAMES__', json.dumps(frames, separators=(',',':')))
    target.write_text(page, encoding='utf-8')
    print('PASS: 32/32 original native Release PNGs and pinned provenance verified')
    print('review (presentation only):', target)
    print('review SHA-256:', hashlib.sha256(page.encode('utf-8')).hexdigest())

HTML = '''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; connect-src 'none'; img-src data:; style-src 'unsafe-inline'; script-src 'unsafe-inline'">
<title>FAULTMINE | Seat Count Minus One | 32 native frames</title>
<style>body{font:16px system-ui,sans-serif;background:#0b1016;color:#eaf0f8;max-width:960px;margin:auto;padding:20px}h1{font-size:1.6rem}p{line-height:1.5;color:#b8c7d6}main{display:grid;gap:12px}#image{width:min(100%,700px);height:auto;aspect-ratio:3/2;object-fit:contain;display:block;margin:auto;image-rendering:pixelated;border:1px solid #506070}button{background:#1e3041;color:#fff;padding:10px;border:1px solid #7b8d9d;border-radius:3px;cursor:pointer}button:focus-visible,input:focus-visible{outline:3px solid #f8c260;outline-offset:2px}.controls{display:flex;gap:8px;flex-wrap:wrap;align-items:center}input[type=range]{flex:1;min-width:130px;accent-color:#f8c260}.sheet{display:grid;grid-template-columns:repeat(8,minmax(0,1fr));gap:5px}.sheet button{padding:0;border:2px solid transparent}.sheet button.active{border-color:#f8c260}.sheet img{width:100%;display:block;image-rendering:pixelated}.sheet span{display:block;font-size:.75rem}#identity{font:12px/1.6 ui-monospace,monospace;word-break:break-all;color:#a6b8c9}.notice{border-left:3px solid #f8c260;padding:10px;background:#14202b}@media(max-width:520px){body{padding:10px}.sheet{grid-template-columns:repeat(4,minmax(0,1fr))}}</style></head>
<body><main><header><small>FAULTMINE / ART-TEMP-005 / Draft #54</small><h1>Seat Count: Minus One</h1><p>Five chairs. Four illuminated seats and an absent, crossed-out centre. The ordered frames below are actual archived Windows CPU/WIC canonical PNG bytes. This viewer is <strong>presentation only</strong>; the original images remain unmodified.</p></header>
<img id="image" width="336" height="224" alt="The Parliament of Absence: four chairs beside an empty crossed-out centre"><div class="controls"><button id="prev">Previous</button><button id="play" aria-pressed="false">Play</button><button id="next">Next</button><button id="reset">Reset</button><label><input id="loop" type="checkbox" checked>Loop</label><output id="count" aria-live="polite">Frame 00 / 31</output></div><div class="controls"><label for="seek">Exact frame</label><input id="seek" type="range" min="0" max="31" value="0"></div><div id="identity" aria-label="Native output identity"></div>
<p class="notice">10 frames/second as authored. Space: play/pause; left/right: step; Home/End: seek; Escape: pause. No interpolation, proxy pixels, external assets, network, or telemetry. FM-012 GUI export/cancellation and physical Windows owner acceptance remain pending.</p><h2>32-frame contact sequence</h2><div id="sheet" class="sheet"></div></main>
<script>'use strict';const data=__FRAMES__;let n=0,t=null;const frame=document.querySelector('#image'),play=document.querySelector('#play'),seek=document.querySelector('#seek'),count=document.querySelector('#count'),identity=document.querySelector('#identity');const tiles=data.map((f,i)=>{let b=document.createElement('button');b.setAttribute('aria-label','Select frame '+i);let im=document.createElement('img');im.src=f.src;im.alt='';im.loading='lazy';b.append(im);let s=document.createElement('span');s.textContent=String(i).padStart(2,'0');b.append(s);document.querySelector('#sheet').append(b);b.addEventListener('click',()=>show(i));return b});function show(k){n=k;frame.src=data[n].src;frame.alt='Original canonical CPU output, frame '+n;seek.value=n;count.textContent='Frame '+String(n).padStart(2,'0')+' / 31';identity.textContent='Source: cb8df081f6f79c6ec75e974dc0f20cb227aa79e97c17dcaeadf554b311eda342 | Genome: e8f70bc1b738b5557d2c0e20beb8d1af49b4c1c0894c4d8bbe1758a11acc3850 | frame '+n+' | output '+data[n].id+' | encoded PNG SHA-256 '+data[n].sha;tiles.forEach((b,i)=>b.classList.toggle('active',i===n))}function pause(){if(t!==null)clearInterval(t);t=null;play.textContent='Play';play.setAttribute('aria-pressed','false')}function toggle(){if(t!==null){pause();return}t=setInterval(()=>{if(n===31){if(document.querySelector('#loop').checked)show(0);else pause()}else show(n+1)},100);play.textContent='Pause';play.setAttribute('aria-pressed','true')}function step(d){pause();show(Math.max(0,Math.min(31,n+d)))}document.querySelector('#prev').onclick=()=>step(-1);document.querySelector('#next').onclick=()=>step(1);document.querySelector('#reset').onclick=()=>{pause();show(0)};play.onclick=toggle;seek.oninput=()=>{pause();show(Number(seek.value))};document.onkeydown=e=>{if(e.altKey||e.ctrlKey||e.metaKey)return;if(['BUTTON','INPUT'].includes(e.target.tagName)&&[' ','ArrowLeft','ArrowRight'].includes(e.key))return;const m={'ArrowLeft':()=>step(-1),'ArrowRight':()=>step(1),'Home':()=>{pause();show(0)},'End':()=>{pause();show(31)},' ':toggle,'Escape':pause};if(m[e.key]){e.preventDefault();m[e.key]()}};window.onpagehide=pause;show(0);</script></body></html>'''

if __name__ == '__main__':
    if len(sys.argv) not in (2,3):
        raise SystemExit('usage: python review_native.py NATIVE-EVALUATION.zip [OPEN-SEAT-REVIEW.html]')
    build(Path(sys.argv[1]),Path(sys.argv[2] if len(sys.argv)==3 else 'OPEN-SEAT-REVIEW.html'))
