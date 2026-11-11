#!/usr/bin/env python3
"""Read-only, stdlib-only audit of a completed Aperture Ossuary native mine.

Independently decodes selected PNGs (optionally every candidate), computes
FAULTMINE image identities and checks address/selection records. Does not
execute the C++ fault engine or replace native Windows qualification.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

HERE = Path(__file__).resolve().parent
SRC_SHA256 = '46dc8809a764257cfb61fbec1f85663e64211080a4611606f85fc340ea44ab09'
SRC_ID = '0023ceb14294cf69b7db062a1afc255ebaa8cc6e2a0119f4bb81d91be053125a'
PARENT_SHA256 = '01217a9e77ea4cd446b7f6142ab6ea71db9bedbcca48425a02d60ea15849524d'
SEED = '4d13b09ec72a8851'
COUNT = 384
SELECT = 12
THRESHOLD = 8192

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def rgba_identity(w: int, h: int, rgba: bytes) -> str:
    assert len(rgba) == w * h * 4
    return sha256(b'FAULTMINE-SOURCE-RGBA8-v1' + struct.pack('<II', w, h) + rgba)

def decode_bmp(b: bytes) -> tuple[int,int,bytes]:
    assert b[:2] == b'BM' and len(b) >= 54, 'invalid BMP'
    w,h,planes,bpp,compression = struct.unpack_from('<iiHHI', b, 18)
    assert (w,h,planes,bpp,compression) == (384,240,1,24,0), 'unexpected source geometry/format'
    offset=struct.unpack_from('<I', b, 10)[0]
    stride = (w*3+3)&~3
    rgba=bytearray()
    for row in range(h-1,-1,-1):
        start=offset+row*stride
        for p in range(start,start+w*3,3):
            rgba.extend((b[p+2],b[p+1],b[p],255))
    return w,h,bytes(rgba)

def decode_png(data: bytes) -> tuple[int,int,bytes]:
    assert data.startswith(b'\x89PNG\r\n\x1a\n'), 'invalid PNG signature'
    off=8; ihdr=None; compressed=bytearray(); saw_end=False
    while off<len(data):
        assert off+12<=len(data), 'truncated PNG chunk'
        length=int.from_bytes(data[off:off+4],'big'); tag=data[off+4:off+8]
        end=off+12+length
        assert end<=len(data), 'truncated PNG chunk data'
        payload=data[off+8:off+8+length]
        crc=int.from_bytes(data[off+8+length:end],'big')
        assert (zlib.crc32(tag+payload)&0xffffffff)==crc, 'bad PNG CRC'
        if tag==b'IHDR':
            assert ihdr is None and length==13
            ihdr=struct.unpack('>IIBBBBB',payload)
        elif tag==b'IDAT':
            compressed.extend(payload)
        elif tag==b'IEND':
            saw_end=True
            break
        off=end
    assert ihdr is not None and saw_end, 'incomplete PNG'
    w,h,depth,colour,compression,filter_method,interlace=ihdr
    assert depth==8 and colour in (2,6) and compression==filter_method==interlace==0, 'unsupported PNG format: require 8-bit RGB/RGBA noninterlaced'
    bpp=4 if colour==6 else 3
    assert 0<w<=16384 and 0<h<=16384 and w*h*4<=512*1024*1024, 'PNG resource ceiling'
    raw=zlib.decompress(bytes(compressed)); stride=w*bpp
    assert len(raw)==h*(stride+1), 'PNG decoded byte size mismatch'
    prev=bytearray(stride); result=bytearray(); pos=0
    for _ in range(h):
        f=raw[pos];pos+=1
        line=bytearray(raw[pos:pos+stride]);pos+=stride
        assert f in (0,1,2,3,4), 'unknown PNG row filter'
        for i in range(stride):
            a=line[i-bpp] if i>=bpp else 0
            b=prev[i];c=prev[i-bpp] if i>=bpp else 0
            if f==1: predictor=a
            elif f==2: predictor=b
            elif f==3: predictor=(a+b)//2
            elif f==4:
                p=a+b-c; da=abs(p-a);db=abs(p-b);dc=abs(p-c)
                predictor=a if da<=db and da<=dc else b if db<=dc else c
            else: predictor=0
            line[i]=(line[i]+predictor)&255
        if bpp==4: result.extend(line)
        else:
            for i in range(0,len(line),3): result.extend((*line[i:i+3],255))
        prev=line
    return w,h,bytes(result)

def audit(folder: Path, all_candidates: bool) -> dict:
    source=(HERE/'source.bmp').read_bytes()
    parent=(HERE/'parent-genome.json').read_bytes()
    assert sha256(source)==SRC_SHA256, 'source BMP bytes changed'
    assert sha256(parent)==PARENT_SHA256, 'parent genome bytes changed'
    w,h,pixels=decode_bmp(source)
    assert rgba_identity(w,h,pixels)==SRC_ID, 'source RGBA identity mismatch'
    m=json.loads((folder/'batch.fmbatch.json').read_text('utf-8'))
    expected={'manifest_version':1,'descriptor_version':1,'mutation_policy_version':1,'source_identity':SRC_ID,'mutation_seed':SEED,'radius':'high','index_begin':0,'index_end_exclusive':COUNT,'frame_index':0,'render_mode':'canonical','near_duplicate_threshold':THRESHOLD,'requested_selection_count':SELECT,'complete':True,'cancelled':False}
    for k,v in expected.items():
        assert m.get(k)==v, f'manifest.{k}: expected {v!r}; got {m.get(k)!r}'
    assert m['parent_genome']==parent.decode('utf-8').removesuffix('\n'), 'parent canonical content mismatch'
    assert m['parent_genome_identity']==PARENT_SHA256, 'parent genome identity mismatch'
    assert len(m['candidates'])==COUNT and not m['failures'], 'incomplete/failed candidate set'
    assert (folder/'batch.fmproj').is_file() and (folder/'selected-contact-sheet.png').is_file(), 'missing GUI project/contact sheet'
    candidates=m['candidates']
    indices=[c['descendant_index'] for c in candidates]
    assert indices==list(range(COUNT)), 'non-contiguous candidate indices'
    selected=[c for c in candidates if c['selected']]
    assert len(selected)==SELECT, f'expected {SELECT} selections, got {len(selected)}'
    assert len({c['genome_identity'] for c in selected})==len(selected), 'duplicate selected genome'
    assert len({c['pixel_identity'] for c in selected})==len(selected), 'duplicate selected canonical pixels'
    for c in candidates:
        assert len(c['descriptor'])==24, 'descriptor shape mismatch'
        assert c['pixel_representative_index']<=c['descendant_index'], 'pixel representative after candidate'
        assert c['near_representative_index']<=c['descendant_index'], 'near representative after candidate'
    for c in selected:
        assert c['near_representative_index']==c['descendant_index'], 'selected entry is not a near-duplicate representative'
    to_check=candidates if all_candidates else selected
    for c in to_check:
        i=c['descendant_index']
        stem=f'candidate_{i:020d}.png'
        path=folder/('candidates' if all_candidates else 'selected')/stem
        assert path.is_file(), f'missing canonical output {path}'
        iw,ih,rgba=decode_png(path.read_bytes())
        assert (iw,ih)==(w,h), f'wrong dimensions: {path}'
        assert rgba_identity(iw,ih,rgba)==c['pixel_identity'], f'identity mismatch: {path}'
    return {'source_identity':SRC_ID,'parent_genome_identity':PARENT_SHA256,'seed':SEED,'requested':COUNT,'completed':len(candidates),'selected':len(selected),'unique_genomes':len({c['genome_identity'] for c in candidates}),'unique_pixels':len({c['pixel_identity'] for c in candidates}),'selected_indices':[c['descendant_index'] for c in selected],'verified_pngs':len(to_check),'all_candidate_caches_checked':all_candidates,'native_replay_independently_verified':False}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('folder',type=Path,help='completed FAULTMINE-batch --out directory')
    p.add_argument('--all',action='store_true',help='decode and independently hash all 384 candidate cache PNGs')
    args=p.parse_args()
    result=audit(args.folder,args.all)
    print(json.dumps(result,indent=2))
if __name__=='__main__':
    main()
