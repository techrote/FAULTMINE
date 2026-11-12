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
# Editorial presentation indices, never a modification of native frame semantics.
STUDY_FRAMES = (0, 1, 2, 4, 6, 8, 9, 12, 13, 14, 18, 19, 21, 23, 24, 27, 28, 29, 31)


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
    page = (HTML.replace('__FRAMES__', json.dumps(frames, separators=(',',':')))
            .replace('__STUDY_FRAMES__', json.dumps(STUDY_FRAMES, separators=(',',':'))))
    target.write_text(page, encoding='utf-8')
    print('PASS: 32/32 original native Release PNGs and pinned provenance verified')
    print('review (presentation only):', target)
    print('review SHA-256:', hashlib.sha256(page.encode('utf-8')).hexdigest())

HTML = '''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; connect-src 'none'; img-src data:; style-src 'unsafe-inline'; script-src 'unsafe-inline'">
<title>FAULTMINE | Seat Count Minus One | 32 native frames</title>
<style>body{font:16px system-ui,sans-serif;background:#0b1016;color:#eaf0f8;max-width:960px;margin:auto;padding:20px}h1{font-size:1.6rem}p{line-height:1.5;color:#b8c7d6}main{display:grid;gap:12px}#image{width:min(100%,700px);height:auto;aspect-ratio:3/2;object-fit:contain;display:block;margin:auto;image-rendering:pixelated;border:1px solid #506070}button{background:#1e3041;color:#fff;padding:10px;border:1px solid #7b8d9d;border-radius:3px;cursor:pointer}button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid #f8c260;outline-offset:2px}.controls{display:flex;gap:8px;flex-wrap:wrap;align-items:center}select{background:#1e3041;color:#fff;border:1px solid #7b8d9d;padding:10px;max-width:100%}input[type=range]{flex:1;min-width:130px;accent-color:#f8c260}.sheet{display:grid;grid-template-columns:repeat(8,minmax(0,1fr));gap:5px}.sheet button{padding:0;border:2px solid transparent}.sheet button.active{border-color:#f8c260}.sheet img{width:100%;display:block;image-rendering:pixelated}.sheet span{display:block;font-size:.75rem}#identity{font:12px/1.6 ui-monospace,monospace;word-break:break-all;color:#a6b8c9}.notice{border-left:3px solid #f8c260;padding:10px;background:#14202b}@media(max-width:520px){body{padding:10px}.sheet{grid-template-columns:repeat(4,minmax(0,1fr))}}</style></head>
<body><main><header><small>FAULTMINE / ART-TEMP-005 / Draft #54</small><h1>Seat Count: Minus One</h1><p>Five chairs. Four illuminated seats and an absent, crossed-out centre. The ordered frames below are actual archived Windows CPU/WIC canonical PNG bytes. This viewer is <strong>presentation only</strong>; the original images remain unmodified.</p></header>
<img id="image" width="336" height="224" alt="The Parliament of Absence: four chairs beside an empty crossed-out centre"><div class="controls"><button id="prev">Previous</button><button id="play" aria-pressed="false">Play</button><button id="next">Next</button><button id="reset">Reset</button><label><input id="loop" type="checkbox" checked>Loop</label><output id="count" aria-live="polite">Frame 00 / 31</output></div><div class="controls"><label for="cadence">Playback</label><select id="cadence"><option value="authored">Authored sequence · 32 frames · 10 fps</option><option value="study">Editorial study · selected native frames</option></select><output id="cut-status" aria-live="polite">All 32 native frames, sequentially</output></div><div class="controls"><label for="seek">Exact frame</label><input id="seek" type="range" min="0" max="31" value="0"></div><div id="identity" aria-label="Native output identity"></div>
<p class="notice"><strong>Authored</strong> mode plays all 32 original frames in order at 10 fps. Optional <strong>Editorial study</strong> makes the opening and final absence more readable by holding the endpoints and skipping repetitive intervening frames; it is <em>not</em> the engine's canonical 10/1 temporal cadence. Manual stepping, scrub and contact tiles always address the exact original frame indices. Space: play/pause; left/right: step; Home/End: seek; Escape: pause. No interpolation, proxy pixels, external assets, network, or telemetry. FM-012 GUI export/cancellation and physical Windows owner acceptance remain pending.</p><h2>32-frame contact sequence</h2><div id="sheet" class="sheet"></div></main>
<script>'use strict';
const data=__FRAMES__;
const studyFrames=__STUDY_FRAMES__;
let n=0,timer=null;
const frame=document.querySelector('#image'),play=document.querySelector('#play'),seek=document.querySelector('#seek'),count=document.querySelector('#count'),identity=document.querySelector('#identity'),cadence=document.querySelector('#cadence'),status=document.querySelector('#cut-status'),loop=document.querySelector('#loop');
const tiles=data.map((f,i)=>{let b=document.createElement('button');b.setAttribute('aria-label','Select frame '+i);let im=document.createElement('img');im.src=f.src;im.alt='';im.loading='lazy';b.append(im);let s=document.createElement('span');s.textContent=String(i).padStart(2,'0');b.append(s);document.querySelector('#sheet').append(b);b.addEventListener('click',()=>stepTo(i));return b});
function show(k){n=k;frame.src=data[n].src;frame.alt='Original canonical CPU output, frame '+n;seek.value=n;count.textContent='Frame '+String(n).padStart(2,'0')+' / 31';identity.textContent='Source: cb8df081f6f79c6ec75e974dc0f20cb227aa79e97c17dcaeadf554b311eda342 | Genome: e8f70bc1b738b5557d2c0e20beb8d1af49b4c1c0894c4d8bbe1758a11acc3850 | frame '+n+' | output '+data[n].id+' | encoded PNG SHA-256 '+data[n].sha;tiles.forEach((b,i)=>b.classList.toggle('active',i===n))}
function pause(){if(timer!==null)clearTimeout(timer);timer=null;play.textContent='Play';play.setAttribute('aria-pressed','false')}
function nextIndex(){if(cadence.value==='authored')return n===31?null:n+1;return studyFrames.find(k=>k>n)??null}
function exposure(){if(cadence.value==='authored')return 100;if(n===0)return 500;if(n===31)return 450;return n<9?155:115}
function advance(){timer=null;const k=nextIndex();if(k===null){if(loop.checked)show(0);else{pause();return}}else show(k);timer=setTimeout(advance,exposure())}
function toggle(){if(timer!==null){pause();return}play.textContent='Pause';play.setAttribute('aria-pressed','true');timer=setTimeout(advance,exposure())}
function stepTo(k){pause();show(Math.max(0,Math.min(31,k)))}
document.querySelector('#prev').onclick=()=>stepTo(n-1);document.querySelector('#next').onclick=()=>stepTo(n+1);document.querySelector('#reset').onclick=()=>stepTo(0);play.onclick=toggle;seek.oninput=()=>stepTo(Number(seek.value));cadence.onchange=()=>{pause();status.textContent=cadence.value==='authored'?'All 32 native frames, sequentially':'19 selected indices, 0/31 held; review edit only'};
document.onkeydown=e=>{if(e.altKey||e.ctrlKey||e.metaKey)return;if((e.key===' '&&['BUTTON','INPUT','SELECT'].includes(e.target.tagName))||(['INPUT','SELECT'].includes(e.target.tagName)&&['ArrowLeft','ArrowRight'].includes(e.key)))return;const m={'ArrowLeft':()=>stepTo(n-1),'ArrowRight':()=>stepTo(n+1),'Home':()=>stepTo(0),'End':()=>stepTo(31),' ':toggle,'Escape':pause};if(m[e.key]){e.preventDefault();m[e.key]()}};
document.addEventListener('visibilitychange',()=>{if(document.hidden)pause()});window.onpagehide=pause;show(0);
</script></body></html>'''

if __name__ == '__main__':
    if len(sys.argv) not in (2,3):
        raise SystemExit('usage: python review_native.py NATIVE-EVALUATION.zip [OPEN-SEAT-REVIEW.html]')
    build(Path(sys.argv[1]),Path(sys.argv[2] if len(sys.argv)==3 else 'OPEN-SEAT-REVIEW.html'))
