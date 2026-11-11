#!/usr/bin/env python3
"""Produce one self-contained Angle-versus-Seat native-frame timing review.

The supplied ZIP must contain the pinned Windows CPU/WIC frame evidence for
ART-TEMP-004 and ART-TEMP-005. Pixels and frame order are immutable; the HTML
is a noncanonical presentation aid, never an FM-012 export or GUI acceptance.
Requires Python standard library only. Never overwrites an existing file.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
import json
from pathlib import Path
import struct
from zipfile import ZipFile
import zlib

DOMAIN = b'FAULTMINE-SOURCE-RGBA8-v1'
SPECS = (
    dict(key='angle', folder='angle-04-release', basename='angle-departure',
         count=36, fps=12, width=320, height=192,
         receipt='fedc650318aedf9b6f1cbe5809bd5e0f648c4a25fffcac8092e3ced396ec6b5c',
         source='332cf105a6625bf25949b68b053f4c8ac748b16a9aa9b86b734ee683c459b497',
         genome='851824bb7e806b75f0e819ddb9b1f902e763e5a2700421fdee9cc5951a55b680',
         seed='b64a7c291d0ee305',
         replay=[35, 0, 19, 7, 28]),
    dict(key='seat', folder='seat-05-release', basename='seat-count',
         count=32, fps=10, width=336, height=224,
         receipt='1ee290b15ffb8a07d4416b51185d223935c321bfea356ddc62e750ab12f4b590',
         source='cb8df081f6f79c6ec75e974dc0f20cb227aa79e97c17dcaeadf554b311eda342',
         genome='e8f70bc1b738b5557d2c0e20beb8d1af49b4c1c0894c4d8bbe1758a11acc3850',
         seed='9a5d7e63c204b811',
         replay=[31, 0, 18, 5, 24]),
)
PNG_MAGIC = b'\x89PNG\r\n\x1a\n'


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def decode_rgba(raw: bytes, width: int, height: int) -> bytes:
    """Independently decode pinned RGBA8, noninterlaced PNG with CRC checks."""
    if not raw.startswith(PNG_MAGIC):
        raise ValueError('Invalid native PNG signature')
    off, chunks, header, finished = 8, bytearray(), None, False
    while off < len(raw):
        if off+12 > len(raw):
            raise ValueError('Truncated PNG chunk')
        length = int.from_bytes(raw[off:off+4], 'big')
        tag = raw[off+4:off+8]
        end = off+12+length
        if end > len(raw):
            raise ValueError('Truncated PNG chunk data')
        data = raw[off+8:off+8+length]
        if (zlib.crc32(tag+data) & 0xffffffff) != int.from_bytes(raw[end-4:end], 'big'):
            raise ValueError('Native PNG CRC mismatch')
        if tag == b'IHDR':
            if header is not None:
                raise ValueError('Multiple PNG headers')
            header = struct.unpack('>IIBBBBB', data)
        elif tag == b'IDAT':
            chunks.extend(data)
        elif tag == b'IEND':
            if length or end != len(raw):
                raise ValueError('Invalid PNG trailer')
            finished = True
            break
        off = end
    if header != (width,height,8,6,0,0,0) or not finished:
        raise ValueError('Unexpected canonical PNG format, dimensions or trailer')
    stride = width*4
    expanded = zlib.decompress(bytes(chunks))
    if len(expanded) != height*(stride+1):
        raise ValueError('Invalid PNG scanline count')
    preceding = bytearray(stride)
    pixels = bytearray()
    for row in range(height):
        pos = row*(stride+1)
        filt = expanded[pos]
        current = bytearray(expanded[pos+1:pos+1+stride])
        if filt not in (0,1,2,3,4):
            raise ValueError('Unsupported PNG filter')
        for x in range(stride):
            a = current[x-4] if x>=4 else 0
            b = preceding[x]
            c = preceding[x-4] if x>=4 else 0
            if filt == 0: pred=0
            elif filt == 1: pred=a
            elif filt == 2: pred=b
            elif filt == 3: pred=(a+b)//2
            else:
                p=a+b-c
                pa,pb,pc=abs(p-a),abs(p-b),abs(p-c)
                pred=a if pa<=pb and pa<=pc else b if pb<=pc else c
            current[x] = (current[x]+pred)&255
        pixels.extend(current)
        preceding=current
    return bytes(pixels)


def unique(names: list[str], path: str) -> str:
    matches = [n for n in names if n.endswith('/'+path)]
    if len(matches) != 1:
        raise ValueError(f'Expected exactly one Windows native artifact {path}, found {len(matches)}')
    return matches[0]


def read_family(z: ZipFile, spec: dict) -> list[dict]:
    names = z.namelist()
    base = 'CANONICAL-REVIEW-EVIDENCE/'+spec['folder']+'/'
    raw = z.read(unique(names, base+'native-cli-replay-audit.json'))
    if sha(raw) != spec['receipt']:
        raise ValueError(f"{spec['key']} Windows-native audit receipt changed")
    record = json.loads(raw)
    for field, wanted in {
        'audit_version':1, 'kind':'authored-native-cli-replay-not-fm012',
        'complete':True,'native_configuration':'Release',
        'source_identity':spec['source'],'genome_identity':spec['genome'],
        'root_seed':spec['seed'],'frame_begin':0,
        'frame_end_exclusive':spec['count'],
        'semantic_rate':f"{spec['fps']}/1",
        'out_of_order_replay':spec['replay'],
    }.items():
        if record.get(field) != wanted:
            raise ValueError(f"{spec['key']} Windows-native provenance invalid: {field}")
    rows = record.get('frames',[])
    if len(rows) != spec['count']:
        raise ValueError(f"{spec['key']} frame count invalid")
    seen = set()
    images = []
    for index, row in enumerate(rows):
        name = f"{spec['basename']}_{index:06d}.png"
        if row.get('frame') != index or row.get('file') != name:
            raise ValueError(f"{spec['key']} frame order invalid: {index}")
        data = z.read(unique(names, base+name))
        if sha(data) != row.get('encoded_png_sha256'):
            raise ValueError(f"{spec['key']} encoded transport mismatch: {index}")
        rgba = decode_rgba(data, spec['width'], spec['height'])
        ident = sha(DOMAIN+struct.pack('<II',spec['width'],spec['height'])+rgba)
        if ident != row.get('output_identity'):
            raise ValueError(f"{spec['key']} decoded canonical RGBA8 mismatch: {index}")
        if ident in seen:
            raise ValueError(f"{spec['key']} duplicate canonical identity: {index}")
        seen.add(ident)
        images.append({'url':'data:image/png;base64,'+base64.b64encode(data).decode('ascii'),
                       'id':ident,'index':index})
    return images


def build(archive: Path, output: Path) -> dict:
    with ZipFile(archive) as z:
        images = {spec['key']:read_family(z,spec) for spec in SPECS}
    payload = json.dumps(images, separators=(',',':'))
    page = HTML.replace('/*__NATIVE_IMAGES__*/', 'const images = '+payload+';')
    with output.open('x', encoding='utf-8', newline='\n') as f:
        f.write(page)
    result = {'sha256':sha(page.encode()), 'angle_frames':len(images['angle']),
              'seat_frames':len(images['seat']),'duration_ms':3200}
    print('PASS: 68/68 native Windows encoded + decoded pixel identities, chronological ordering and pinned provenance')
    print('Noncanonical offline dual-playback HTML:', output, 'SHA-256:', result['sha256'])
    return result


HTML = r'''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; connect-src 'none'; img-src data:; style-src 'unsafe-inline'; script-src 'unsafe-inline'">
<title>FAULTMINE | Angle vs Seat | Native temporal admission</title>
<style>
:root{color-scheme:dark}body{font:16px/1.45 system-ui,sans-serif;background:#0e131a;color:#edf1f7;max-width:1150px;margin:auto;padding:16px}
h1{font-size:1.45rem;margin:.4rem 0}.deck{color:#becbd9;max-width:74ch}h2{font-size:1.15rem;margin:0 0 .4rem}.panels{display:grid;grid-template-columns:1fr 1fr;gap:16px}
article{min-width:0;border:1px solid #526275;padding:12px;background:#151e28}article img.hero{width:100%;height:auto;display:block;image-rendering:pixelated;aspect-ratio:5/3;object-fit:contain;background:#06090e}
article p{margin:.6rem 0;color:#bfcbda} .contacts{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:4px}
.contacts button{display:block;min-width:0;padding:2px;text-align:left;border:1px solid #526275;background:#101824;cursor:pointer}
.contacts button[aria-current="true"]{border-color:#fac26d;box-shadow:inset 0 0 0 1px #fac26d}
.contacts img{display:block;width:100%;height:auto;image-rendering:pixelated}
.contacts small{display:block;font:11px/1.4 ui-monospace,monospace;color:#d8e2ee}
.controls{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin:16px 0}button,select{font:inherit;color:#fff;background:#263849;border:1px solid #78899d;padding:8px;border-radius:3px}button:focus-visible,select:focus-visible,input:focus-visible{outline:3px solid #fac26d;outline-offset:2px}
input[type=range]{width:min(100%,550px);accent-color:#fac26d}#time{font-variant-numeric:tabular-nums;font-family:ui-monospace,monospace}.micro{font-size:.85rem;color:#aabdd0}.contract{border-top:1px solid #506070;padding-top:10px;color:#c1cada}
@media(max-width:720px){.panels{grid-template-columns:1fr}body{padding:9px}}
</style></head><body><header><small>FAULTMINE · ART-TEMP-004 #50 ↔ ART-TEMP-005 #54 · BOTH UNADMITTED</small><h1>Cartographic hinge / Missing chair</h1>
<p class="deck">A simultaneous comparison of two different temporal compositions using <strong>unchanged genuine Windows CPU/WIC canonical frames</strong>. Both run at their documented semantic rates, but this synchronized browser player, controls and contact strips are <strong>presentation only</strong> — not engine output, a substitute for a Win32 project test, or human artistic approval.</p></header>
<div class="panels"><article><h2>Angle of Departure <small>· 12/1 fps · 36 frames</small></h2><img class="hero" id="angle" width="320" height="192" alt="Angle of Departure canonical frame 0"><p id="angleIndex" aria-live="off">Frame 0 / 35</p><div class="contacts" id="angleContact"></div><p>Interlocking cyan, coral and pale cartographic leaves encircle a black diamond hinge. The void persists while planes drift and split.</p></article>
<article><h2>Seat Count: Minus One <small>· 10/1 fps · 32 frames</small></h2><img class="hero" id="seat" width="336" height="224" alt="Seat Count canonical frame 0"><p id="seatIndex" aria-live="off">Frame 0 / 31</p><div class="contacts" id="seatContact"></div><p>Four bright chairs surround a crossed-out absent fifth. Late chromatic shear is repetitive and erodes the absent centre: native RGB change 26→27 = 7.84%, 27→28 = 4.48%, then 28→29 = 42.85%.</p></article></div>
<div class="controls"><button type="button" id="play" aria-pressed="false">Play both</button><button type="button" id="reset">Restart</button><label for="speed">Review speed</label><select id="speed"><option value="1">1× authored timing</option><option value="0.5">0.5× slowed presentation</option></select><label for="seatCadence">Seat cadence</label><select id="seatCadence"><option value="authored">Authored 32 frames · 10 fps</option><option value="study">Editorial 19-frame study · held endpoints</option></select><label><input type="checkbox" id="loop">Loop</label><output id="time">0 / 3200 ms</output></div>
<label for="seek">Synchronized playback time (ms)</label><input id="seek" type="range" min="0" max="3200" step="50" value="0">
<p class="micro">Space: play/pause · Left/Right: step 100ms · Home/End: bounds · Escape: stop. Hidden tab, navigation, scrub and reset cancel playback. Authored Seat plays all 32 at 10 fps. The optional 19-frame study uses original native indices (not FM-012/10 fps), borrowing the author’s published cut, with an extra 95ms terminal hold to match 3200ms. Angle stays at its original 12 fps. At 3000–3200ms Angle holds its final native frame. Contact-strip tiles compare equal timestamps, not equal frame numbers; choose a tile to seek both players. All tiles update when Seat cadence changes. No invented intermediate frames, interpolation or audio.</p>
<p class="contract">Verification: 36 Angle + 32 Seat Windows-native original PNGs checked against both pinned original Release CLI receipts, encoded SHA-256 and decoded canonical RGBA8 identities. Exact engine source/genome/seed/explicit integer frame inputs remain immutable. Angle seed <code>b64a7c291d0ee305</code>; Seat seed <code>9a5d7e63c204b811</code>. Owner: <strong>admit / revise / reject Seat</strong> after watching actual motion beside Angle. Integration: physically check Win32 project import/relink and FM-012 completed/cancelled sequence export. These physical gates are not covered by this presentation.</p>
<script>
'use strict';
/*__NATIVE_IMAGES__*/
const duration=3200, totals={angle:36, seat:32}, rates={angle:12,seat:10};
const imgs={angle:document.getElementById('angle'),seat:document.getElementById('seat')};
const labels={angle:document.getElementById('angleIndex'),seat:document.getElementById('seatIndex')};
const play=document.getElementById('play'),seek=document.getElementById('seek'),speed=document.getElementById('speed'),time=document.getElementById('time'),loop=document.getElementById('loop');
let ms=0, handle=null, origin=0;
const seatCadence=document.getElementById('seatCadence');
const seatStudy=[0,1,2,4,6,8,9,12,13,14,18,19,21,23,24,27,28,29,31];
const seatStudyMs=seatStudy.map(i=>i===0?500:i===31?545:i<9?155:115);
function seatAt(t){if(seatCadence.value==='authored')return Math.min(31,Math.floor(t/100));let remaining=t;for(let i=0;i<seatStudy.length;i++){if(remaining<seatStudyMs[i])return seatStudy[i];remaining-=seatStudyMs[i]}return 31;}
function frameAt(type,t){return type==='seat'?seatAt(t):Math.min(totals[type]-1,Math.floor(t*rates[type]/1000));}
// Time-aligned contact frames reveal how the optional editorial cadence changes
// the exposure of the SAME immutable original Windows-native images.
const contactTimes=[0,450,900,1350,1800,2250,2700,3200];
let lastContactSelection=-1;
function drawContacts(){for(const type of ['angle','seat']){const strip=document.getElementById(type+'Contact');strip.replaceChildren();for(const t of contactTimes){const index=frameAt(type,t);const button=document.createElement('button');button.type='button';button.dataset.time=String(t);button.setAttribute('aria-label',type+' at '+t+' milliseconds: native frame '+index);const im=document.createElement('img');im.src=images[type][index].url;im.alt='';im.loading='lazy';const meta=document.createElement('small');meta.textContent=t+'ms · #'+String(index).padStart(2,'0');button.append(im,meta);button.onclick=()=>select(t);strip.append(button)}}lastContactSelection=-1;}
function show(t){ms=Math.max(0,Math.min(duration,t));for(const type of ['angle','seat']){const index=frameAt(type,ms);imgs[type].src=images[type][index].url;imgs[type].alt=type+' original canonical frame '+index;imgs[type].dataset.frame=String(index);labels[type].textContent='Frame '+String(index).padStart(2,'0')+' / '+(totals[type]-1)+' · '+(type==='seat'&&seatCadence.value==='study'?'EDITORIAL 19-frame selection':'authored '+rates[type]+'/1 fps')}const active=contactTimes.reduce((best,t,index)=>ms>=t?index:best,0);if(active!==lastContactSelection){for(const type of ['angle','seat']){const buttons=document.getElementById(type+'Contact').children;for(let i=0;i<buttons.length;i++)buttons[i].setAttribute('aria-current',i===active?'true':'false')}lastContactSelection=active}seek.value=String(Math.round(ms/50)*50);time.textContent=Math.round(ms)+' / '+duration+' ms'}
function stop(){if(handle!==null)cancelAnimationFrame(handle);handle=null;play.textContent='Play both';play.setAttribute('aria-pressed','false')}
function tick(clock){if(handle===null)return;const t=(clock-origin)*Number(speed.value);if(t>=duration){show(duration);if(loop.checked){origin=clock;show(0)}else{stop();return}}else show(t);handle=requestAnimationFrame(tick)}
function toggle(){if(handle!==null){stop();return}if(ms>=duration)show(0);origin=performance.now()-ms/Number(speed.value);play.textContent='Pause';play.setAttribute('aria-pressed','true');handle=requestAnimationFrame(tick)}
function select(t){stop();show(t)}
drawContacts();
play.onclick=toggle;document.getElementById('reset').onclick=()=>select(0);seek.oninput=()=>select(Number(seek.value));seatCadence.onchange=()=>{drawContacts();select(ms)};speed.onchange=()=>{if(handle!==null){stop();toggle()}};
document.addEventListener('keydown',event=>{if(event.altKey||event.ctrlKey||event.metaKey)return;const el=event.target.tagName;if(event.key!=='Escape'&&(el==='INPUT'||el==='SELECT'))return;if(el==='BUTTON'&&event.key===' ')return;const keys={' ':toggle,'ArrowLeft':()=>select(ms-100),'ArrowRight':()=>select(ms+100),'Home':()=>select(0),'End':()=>select(duration),'Escape':stop};if(keys[event.key]){event.preventDefault();keys[event.key]()}});
document.addEventListener('visibilitychange',()=>{if(document.hidden)stop()});window.addEventListener('pagehide',stop);show(0);
</script></body></html>'''

if __name__ == '__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('four_family_review_zip',type=Path)
    ap.add_argument('out_html',type=Path)
    args=ap.parse_args()
    build(args.four_family_review_zip,args.out_html)
