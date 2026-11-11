#!/usr/bin/env python3
"""Independent, stdlib-only preflight of ART-ORIG-005; no proxy rendering."""
import argparse,hashlib,json,struct,zlib
from pathlib import Path
# Assertions enforce every pinned byte/pixel/lineage invariant below. Under -O
# Python removes them; never report a false PASS in an optimized interpreter.
if not __debug__:
    raise SystemExit('STOP: optimized Python bypasses verifier assertions; rerun without -O/-OO')
D=Path(__file__).resolve().parent
SI='db8754258a9a788d36f4ac466fc538b021725c0cbc7d293c13f8b4c4a005a108'
GI='f3493de53d45a4013c64488f16fb9b3d1e666f66113b2e996aca2068b4525da4'
S='f7d396be000cd153f29f1b25110162627d0dbf5726be51ea39f35d3d2b005f84'
P='14880df2f490aed709f998d1d52e004eb344fd9c4245ee9f97f53ed20160bfbe'
SEED='c18e70d453b29a64'
OI='981bf2f2bb64f69cb8da54a59a343af8b3b9c0c51828bccddff924973b672979'
OH='fb3272681ad20147035e9209ed767d1b2d6e4ea26b3a44c7407cf1b27feb07d4'
W,H=320,224

def sha(b): return hashlib.sha256(b).hexdigest()
def strict_pairs(pairs):
    out={}
    for k,v in pairs:
        if k in out: raise ValueError('duplicate JSON field '+k)
        out[k]=v
    return out

def load_json(name):
    return json.loads((D/name).read_bytes(),object_pairs_hook=strict_pairs)

def png_pixels(blob):
    assert blob.startswith(b'\x89PNG\r\n\x1a\n'), 'PNG signature'
    off=8;shape=None;pal=[];idat=[];finished=False
    while off<len(blob):
        assert off+12<=len(blob),'truncated PNG chunk'
        n=struct.unpack_from('>I',blob,off)[0]
        kind=blob[off+4:off+8];assert off+n+12<=len(blob),'PNG chunk overflow'
        payload=blob[off+8:off+8+n]
        assert struct.unpack_from('>I',blob,off+8+n)[0]==zlib.crc32(kind+payload),'PNG CRC mismatch'
        off+=n+12
        if kind==b'IHDR':
            assert shape is None and len(payload)==13
            shape=struct.unpack('>IIBBBBB',payload)
            assert shape[2]==8 and shape[3] in (3,6) and shape[4:]==(0,0,0),'unsupported PNG encoding'
        elif kind==b'PLTE':
            assert len(payload)%3==0 and len(payload)>0
            pal=[payload[i:i+3] for i in range(0,len(payload),3)]
        elif kind==b'IDAT': idat.append(payload)
        elif kind==b'tRNS': raise AssertionError('opacity is required')
        elif kind==b'IEND':
            assert not payload and off==len(blob)
            finished=True
    assert finished and shape and idat
    w,h,depth,colour,*_=shape
    if colour==3:assert pal and len(pal)<=256
    stride=w*(1 if colour==3 else 4)
    packed=zlib.decompress(b''.join(idat))
    assert len(packed)==h*(stride+1),'decompressed byte length'
    prev=bytearray(stride);rgba=bytearray()
    for y in range(h):
        start=y*(stride+1);flt=packed[start];row=bytearray(packed[start+1:start+stride+1]);assert flt in range(5)
        for x in range(stride):
            a=row[x-1 if colour==3 else x-4] if x>=(1 if colour==3 else 4) else 0
            b=prev[x]
            c=prev[x-1 if colour==3 else x-4] if x>=(1 if colour==3 else 4) else 0
            if flt==1: p=a
            elif flt==2: p=b
            elif flt==3: p=(a+b)//2
            elif flt==4:
                t=a+b-c
                da,db,dc=abs(t-a),abs(t-b),abs(t-c)
                p=a if da<=db and da<=dc else b if db<=dc else c
            else:p=0
            row[x]=(row[x]+p)&255
        if colour==3:
            for i in row:
                assert i<len(pal),'palette index out of range'
                rgba.extend(pal[i]);rgba.append(255)
        else:rgba.extend(row)
        prev=row
    return w,h,bytes(rgba)

def source_id(w,h,rgba):
    return sha(b'FAULTMINE-SOURCE-RGBA8-v1'+struct.pack('<II',w,h)+rgba)

def verify(canonical=None):
    image=(D/'source.png').read_bytes()
    assert sha(image)==S,'source file SHA drift'
    w,h,rgba=png_pixels(image)
    assert (w,h)==(W,H),'source dimensions'
    assert source_id(w,h,rgba)==SI,'normalized source mismatch'
    gbytes=(D/'genome.json').read_bytes()
    assert sha(gbytes)==GI,'canonical genome SHA drift'
    genome=load_json('genome.json')
    assert (json.dumps(genome,separators=(',',':'),ensure_ascii=False)+'\n').encode()==gbytes,'noncanonical genome encoding'
    assert genome['root_seed']==SEED
    assert (genome['schema_version'],genome['engine_contract_version'])==(1,1)
    assert len(genome['operators'])==7
    ids=[]
    for i,o in enumerate(genome['operators'],1):
        assert o['instance_id']==('4745535455524535'+f'{i:016x}')
        assert o['type_version']==1 and o['enabled'] is True
        assert list(o['parameters'])==sorted(o['parameters']),'param serialization order'
        for p in o['parameters'].values():
            assert set(p)=={'kind','value'} and list(p)==['kind','value']
            assert p['kind'] in {'i64','u64','string','bool'}
        ids.append(o['instance_id'])
    assert len(ids)==len(set(ids))
    pbytes=(D/'the-unclaimed-gesture.fmproj').read_bytes()
    assert sha(pbytes)==P,'canonical project SHA drift'
    project=load_json('the-unclaimed-gesture.fmproj')
    assert (json.dumps(project,separators=(',',':'),ensure_ascii=False)+'\n').encode()==pbytes
    assert project['project_version']==2
    assert project['source']=={'path':'gallery/originals/05-the-unclaimed-gesture/source.png','identity':SI}
    assert project['genome']==genome and project['locks']==[]
    lineage=project['lineage']
    assert lineage['active_genome_identity']==GI and len(lineage['specimens'])==1
    spec=lineage['specimens'][0]
    assert (spec['genome_identity'],spec['source_identity'],spec['genome'])==(GI,SI,genome)
    assert spec['derivation']=={'kind':'manual-root','parents':[],'policy_version':0,'seed':None,'descendant_index':None,'radius':None}
    assert spec['favourite'] is True and spec['creation_ordinal']==0
    a=load_json('artwork.json')
    assert a['artwork_id']=='ART-ORIG-005' and a['artwork_manifest_version']==1
    assert (a['source_encoding_sha256'],a['source_identity'],a['genome_identity'],a['project_encoding_sha256'])==(S,SI,GI,P)
    assert (a['root_seed'],a['explicit_frame'],a['operator_count'])==(SEED,0,7)
    assert a['source_dimensions']==[W,H]
    assert a['native_render_status']=='windows-native-cli-release-debug-replayed-archived'
    assert a['canonical_output_identity']==OI and a['canonical_output_encoded_sha256']==OH
    assert a['canonical_output_file']=='unclaimed-gesture-canonical.png', 'Archived canonical artwork path is required'
    assert a['visual_evidence']=='archived-native-cpu-image-inspected-not-owner-accepted'
    evidence=a['canonical_evidence']
    assert evidence=={'run_id':42990696670,'artifact_id':13841447863,
        'artifact_member':'original-05-native/unclaimed-gesture-canonical.png',
        'receipt_member':'original-05-native/native-replay-receipt.json',
        'pinned_author_head':'962e55a3906e256e0a9ad407fdd56c44ecdbce53',
        'retention':'Exact Windows-native encoded PNG committed unmodified to artwork subtree and independently retrievable from the original Windows Actions artifact'}
    assert sha((D/'native-replay-receipt.json').read_bytes())=='09371ca0192c7f13115c88a4fea22682e5521d9905412ffba2e0d48826b17631', 'Original Windows receipt transport changed'
    receipt=load_json('native-replay-receipt.json')
    assert receipt=={'audit_version':1,'kind':'author-cli-replay-not-FM012',
        'engine_contract_version':1,'source_identity':SI,'genome_identity':GI,
        'root_seed':SEED,'frame':0,'output_identity':OI,'encoded_png_sha256':OH,
        'release_verify':'passed','debug_verify':'passed',
        'output_file':'unclaimed-gesture-canonical.png'}, 'Native receipt drift'
    # The committed native proof is mandatory on every run; --canonical provides an additional comparison.
    archived=D/a['canonical_output_file']
    assert archived.is_file(), 'Missing committed Windows-native canonical PNG'
    native=archived.read_bytes()
    def check_native(blob):
        assert sha(blob)==OH, 'Native PNG container SHA mismatch'
        assert blob.startswith(b'\x89PNG\r\n\x1a\n')
        width,height,depth,colour,compress,filter_method,interlace=struct.unpack_from('>IIBBBBB',blob,16)
        assert (width,height,depth,colour,compress,filter_method,interlace)==(W,H,8,6,0,0,0), 'Expected native straight RGBA8 PNG'
        w,h,rgba=png_pixels(blob)
        assert (w,h)==(W,H) and source_id(w,h,rgba)==OI, 'Native decoded pixel identity mismatch'
        damaged=bytearray(blob);damaged[200]^=1
        try:png_pixels(damaged)
        except (AssertionError,ValueError,zlib.error):pass
        else:raise AssertionError('Corrupted native PNG was accepted')
    check_native(native)
    if canonical is not None:
        external=Path(canonical).read_bytes()
        assert external==native, 'External native proof is not byte-identical to the committed PNG'
        check_native(external)
    print('PASS: committed original Windows-native PNG SHA, strict CRC/decode, canonical RGBA8 output identity and corruption rejection')
    print('PASS: source PNG, canonical genome/project bytes, typed stack, root seed and lineage. Physical Win32 GUI/art acceptance remains pending.')

def negative():
    payload=bytearray((D/'source.png').read_bytes());payload[-22]^=1
    try:png_pixels(payload)
    except (AssertionError,zlib.error):pass
    else:raise AssertionError('corrupt PNG unexpectedly accepted')
    try:json.loads('{"schema_version":1,"schema_version":2}',object_pairs_hook=strict_pairs)
    except ValueError:pass
    else:raise AssertionError('duplicate JSON unexpectedly accepted')
    # Regression for PYTHONOPTIMIZE/-O: the verifier must fail instead of
    # silently skipping all asserts and printing a false acceptance.
    import subprocess,sys
    optimized=subprocess.run([sys.executable,'-O',str(Path(__file__).resolve()),'--self-test'],capture_output=True,text=True,check=False)
    assert optimized.returncode!=0 and 'optimized Python bypasses verifier assertions' in (optimized.stdout+optimized.stderr), 'optimized Python must fail closed'
    print('PASS: deliberate damaged PNG, duplicate JSON keys and optimized interpreter rejected')
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--self-test',action='store_true');ap.add_argument('--canonical');args=ap.parse_args()
    verify(args.canonical)
    if args.self_test:negative()
