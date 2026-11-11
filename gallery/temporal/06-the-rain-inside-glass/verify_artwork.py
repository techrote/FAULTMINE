#!/usr/bin/env python3
"""Input-only preflight for ART-TEMP-006. Never asserts native output."""
import hashlib, json, pathlib, re, struct
P=pathlib.Path(__file__).resolve().parent
SHA='82180916e814d2738eb8c41e1bf3c745022b219cca788c5467819e83e380fba3'
SRC='60352c93cb2322e3d40d2fd77b9793e68cd0cd0314b4e75996113411bd718fe9'
GID='c76d773802faa8e8de5291497dc06e73c277150b65a5779f17dbebaa7d61b189'
TYPES=['fault.channel-offset','temporal.phase-drift','temporal.tearing-phase','temporal.partial-refresh','temporal.trail','temporal.feedback-displace','temporal.channel-phase','colour.quantize','temporal.timeline-rate']
def sha(b):return hashlib.sha256(b).hexdigest()
def pairs(ps):
    d={}
    for k,v in ps:
        if k in d:raise ValueError('duplicate key: '+k)
        d[k]=v
    return d
def parsed(name,compact=False):
    raw=(P/name).read_bytes()
    obj=json.loads(raw.decode('utf8'),object_pairs_hook=pairs)
    if compact:assert raw==(json.dumps(obj,separators=(',',':'),ensure_ascii=False)+'\n').encode(),name
    return obj,raw
def decoded_id(raw):
    if len(raw)<54 or raw[:2]!=b'BM':raise ValueError('invalid BMP')
    n,=struct.unpack_from('<I',raw,2);offset,=struct.unpack_from('<I',raw,10)
    hsize,w,h,planes,bpp,comp=struct.unpack_from('<IiiHHI',raw,14)
    if (n,offset,hsize,w,h,planes,bpp,comp)!=(len(raw),54,40,256,176,1,24,0):raise ValueError('BMP header')
    stride=(w*3+3)&~3
    if len(raw)!=54+stride*h:raise ValueError('BMP length')
    px=bytearray()
    for y in range(h):
        row=raw[54+(h-1-y)*stride:54+(h-y)*stride]
        for x in range(w):
            b,g,r=row[3*x:3*x+3];px.extend((r,g,b,255))
    return sha(b'FAULTMINE-SOURCE-RGBA8-v1'+struct.pack('<II',w,h)+px)
def run():
    raw=(P/'source.bmp').read_bytes()
    assert sha(raw)==SHA and decoded_id(raw)==SRC
    g,gb=parsed('genome.json',True)
    assert sha(gb)==GID
    assert (g['schema_version'],g['engine_contract_version'],g['root_seed'])==(1,1,'4c6f8a21d3e507b9')
    ops=g['operators']
    assert [o['type_id'] for o in ops]==TYPES
    assert len({o['instance_id'] for o in ops})==9
    assert all(re.fullmatch('[0-9a-f]{32}',o['instance_id']) and o['type_version']==1 and o['enabled'] is True and list(o['parameters'])==sorted(o['parameters']) for o in ops)
    assert ops[4]['parameters']['decay_256']['value']==204
    assert ops[5]['type_id']=='temporal.feedback-displace'
    assert ops[6]['parameters']['shape']['value']=='sample-hold'
    assert ops[-1]['parameters']=={'rate_den':{'kind':'u64','value':1},'rate_num':{'kind':'u64','value':8}}
    proj,_=parsed('rain-inside-glass.fmproj',True)
    assert proj['project_version']==2 and proj['genome']==g and proj['locks']==[]
    assert proj['source']=={'path':'gallery/temporal/06-the-rain-inside-glass/source.bmp','identity':SRC}
    lineage=proj['lineage'];assert lineage['active_genome_identity']==GID and len(lineage['specimens'])==1
    sp=lineage['specimens'][0]
    assert sp['genome_identity']==GID and sp['source_identity']==SRC and sp['genome']==g
    assert sp['derivation']=={'kind':'manual-root','parents':[],'policy_version':0,'seed':None,'descendant_index':None,'radius':None}
    meta,_=parsed('artwork.json')
    assert (meta['source_encoding_sha256'],meta['source_identity'],meta['genome_identity'],meta['root_seed'])==(SHA,SRC,GID,'4c6f8a21d3e507b9')
    assert (meta['frame_begin'],meta['frame_end_exclusive'],meta['rate_num'],meta['rate_den'])==(0,24,8,1)
    assert meta['recommended_key_frames']==[0,3,6,9,12,15,18,21,23]
    assert meta['canonical_frame_status']=='pending-native-windows-cpu-render'
    try:decoded_id(raw[:-1])
    except ValueError:pass
    else:raise AssertionError('truncated BMP accepted')
    damaged=bytearray(raw);damaged[1024]^=1
    assert sha(damaged)!=SHA and decoded_id(damaged)!=SRC
    try:json.loads('{"a":1,"a":2}',object_pairs_hook=pairs)
    except ValueError:pass
    else:raise AssertionError('duplicate JSON accepted')
    print('PASS: source BMP/RGBA, canonical genome, project v2, lineage and 24-frame identity metadata; tamper rejections')
    print('NOT TESTED: native temporal rendering/frames, cross-build identity, GUI FM-012 export, visual quality')
if __name__=='__main__':run()
