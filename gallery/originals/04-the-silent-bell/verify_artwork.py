#!/usr/bin/env python3
"""Independent read-only ART-ORIG-004 input/provenance audit. Never renders canonical faults."""
import hashlib, json, struct, zlib
from pathlib import Path
D=Path(__file__).resolve().parent
S='be6f9c19e0b24544e1fc8883f55a041e06fd309c52bea5771c0bbff8b730da04'
SI='275bc5c264376ac81442ca3d5b399cff01b33449c21906be2dc140fe8085e747'
G='dc0340692b48259ed77fbf751ed692360c0345945bbb3b50f975ec0c3d8bde61'
SEED='9f6b31ca0d42e857'
P='gallery/originals/04-the-silent-bell/source.png'
def sha(b):return hashlib.sha256(b).hexdigest()
def strict(pairs):
    out={}
    for k,v in pairs:
        if k in out:raise ValueError('duplicate JSON key '+k)
        out[k]=v
    return out
def compact(name):
    raw=(D/name).read_bytes()
    v=json.loads(raw,object_pairs_hook=strict)
    assert raw==(json.dumps(v,separators=(',',':'),ensure_ascii=False)+'\n').encode(),name+' not canonical compact LF JSON'
    return v,raw
def unpack_png(raw):
    assert raw[:8]==b'\x89PNG\r\n\x1a\n'
    pos=8;pal=None;frags=[];width=height=depth=ctype=None;end=False
    while pos<len(raw):
        assert pos+12<=len(raw)
        length=struct.unpack_from('>I',raw,pos)[0];kind=raw[pos+4:pos+8]
        assert pos+12+length<=len(raw)
        data=raw[pos+8:pos+8+length];crc=struct.unpack_from('>I',raw,pos+8+length)[0]
        assert zlib.crc32(kind+data)==crc,'PNG CRC error'
        pos+=length+12
        if kind==b'IHDR':
            assert len(data)==13
            width,height,depth,ctype,comp,filtering,interlace=struct.unpack('>IIBBBBB',data)
            assert (width,height,depth,ctype,comp,filtering,interlace)==(384,256,4,3,0,0,0)
        elif kind==b'PLTE':pal=[tuple(data[i:i+3]) for i in range(0,len(data),3)]
        elif kind==b'IDAT':frags.append(data)
        elif kind==b'IEND':assert pos==len(raw);end=True
        elif kind==b'tRNS':raise ValueError('opaque source must not have transparency')
    assert end and pal and frags
    scan=zlib.decompress(b''.join(frags));stride=(width+1)//2
    assert len(scan)==(stride+1)*height
    prev=bytearray(stride);rgba=bytearray()
    for y in range(height):
        start=y*(stride+1);f=scan[start];row=bytearray(scan[start+1:start+1+stride]);assert f<=4
        for x in range(stride):
            a=row[x-1] if x else 0;b=prev[x];c=prev[x-1] if x else 0
            if f==1:row[x]=(row[x]+a)&255
            elif f==2:row[x]=(row[x]+b)&255
            elif f==3:row[x]=(row[x]+((a+b)//2))&255
            elif f==4:
                p=a+b-c;da=abs(p-a);db=abs(p-b);dc=abs(p-c)
                row[x]=(row[x]+(a if da<=db and da<=dc else b if db<=dc else c))&255
        for x in range(width):
            idx=(row[x//2]>>4) if x%2==0 else row[x//2]&15
            rgba.extend(pal[idx]);rgba.append(255)
        prev=row
    return sha(b'FAULTMINE-SOURCE-RGBA8-v1'+struct.pack('<II',width,height)+rgba)
def main():
    raw=(D/'source.png').read_bytes()
    assert sha(raw)==S and unpack_png(raw)==SI,'source bytes or RGBA changed'
    genome,gb=compact('genome.json')
    assert sha(gb)==G and (genome['schema_version'],genome['engine_contract_version'],genome['root_seed'])==(1,1,SEED)
    ops=genome['operators'];assert len(ops)==7 and len({x['instance_id'] for x in ops})==7
    assert [x['type_id'] for x in ops]==['fault.band-repeat','fault.address-burst','fault.channel-offset','fault.scanline-jitter','fault.bit-burst','colour.quantize','colour.dither-ordered']
    assert all(x['type_version']==1 and x['enabled'] and list(x['parameters'])==sorted(x['parameters']) for x in ops)
    pr,_=compact('the-silent-bell.fmproj')
    assert pr['project_version']==2 and pr['source']=={'path':P,'identity':SI} and pr['genome']==genome and pr['locks']==[]
    ln=pr['lineage'];assert ln['active_genome_identity']==G and len(ln['specimens'])==1
    one=ln['specimens'][0];assert (one['genome_identity'],one['source_identity'],one['genome'])==(G,SI,genome)
    assert one['derivation']=={'kind':'manual-root','parents':[],'policy_version':0,'seed':None,'descendant_index':None,'radius':None}
    manifest,_=compact('artwork.json')
    assert (manifest['source_encoding_sha256'],manifest['source_identity'],manifest['genome_identity'],manifest['root_seed'],manifest['explicit_frame'])==(S,SI,G,SEED,0)
    assert manifest['native_render_status']=='pending-supported-windows-cpu' and manifest['canonical_output_identity'] is None
    changed=bytearray(raw);changed[100]^=1
    try:unpack_png(changed)
    except (AssertionError,ValueError,zlib.error):pass
    else:raise AssertionError('corrupted source accepted')
    try:json.loads('{"x":1,"x":2}',object_pairs_hook=strict)
    except ValueError:pass
    else:raise AssertionError('duplicate-key JSON accepted')
    print('PASS: indexed PNG CRC/filter/4-bit decode, exact RGBA source identity, genome v1 hash and project v2 lineage')
    print('PASS: corrupted PNG and duplicate JSON rejected; authored source only, no canonical render claimed')
if __name__=='__main__':main()
