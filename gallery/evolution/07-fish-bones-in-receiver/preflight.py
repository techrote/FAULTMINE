#!/usr/bin/env python3
"""Independent stdlib preflight of the *source* and accepted parent, not a render claim."""
import hashlib
import json
import pathlib
import struct
import sys

HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[2]
SOURCE=HERE/'source.bmp'
PARENT=HERE/'parent-genome.json'
MAIN=ROOT/'examples'/'fm005-addressing-genome.json'
EXPECTED_BMP='ca6883cd3fc73526abdcba96671acf83f130005ed2b17d2f87e48c35b693f6cc'
EXPECTED_SOURCE='76352a4b6b859a3605568e8b3a9a7f09caeab66bda1418c290d075d54026e6af'
EXPECTED_BLOB='64b0756262db458e276861dad84edbd964571558'

def require(condition,description):
    if not condition:raise AssertionError(description)
    print('PASS:',description)

def decode_bmp(data):
    require(len(data)>=54 and data[:2]==b'BM','BMP signature and header')
    length,offset=struct.unpack_from('<I',data,2)[0],struct.unpack_from('<I',data,10)[0]
    dib,width,height,planes,bpp,compression=struct.unpack_from('<IiiHHI',data,14)
    require((dib,width,height,planes,bpp,compression,offset)==(40,384,256,1,24,0,54),'BMP 24-bit dimensions/format')
    stride=(width*3+3)&~3
    require(length==len(data)==offset+stride*height,'BMP exact encoded length')
    rgba=bytearray()
    for row in range(height-1,-1,-1):
        start=offset+row*stride
        for i in range(start,start+width*3,3):
            b,g,r=data[i:i+3];rgba.extend((r,g,b,255))
    return hashlib.sha256(b'FAULTMINE-SOURCE-RGBA8-v1'+struct.pack('<II',width,height)+rgba).hexdigest()

def parse_strict(data):
    def unique(pairs):
        result={}
        for key,value in pairs:
            if key in result:raise ValueError('duplicate JSON property: '+key)
            result[key]=value
        return result
    return json.loads(data.decode('utf-8'),object_pairs_hook=unique)

def main():
    import make_source
    made=bytes(make_source.out)
    require(hashlib.sha256(made).hexdigest()==EXPECTED_BMP,'authored BMP fixed SHA-256')
    require(decode_bmp(made)==EXPECTED_SOURCE,'independently decoded normalized RGBA8 source identity')
    changed=bytearray(made);changed[54+83*384*3+191*3]^=1
    require(decode_bmp(changed)!=EXPECTED_SOURCE,'single-pixel corruption changes source identity')
    if SOURCE.exists():
        require(SOURCE.read_bytes()==made,'materialized source BMP exactly matches constructor')
    else:
        print('SOURCE BMP NOT YET MATERIALIZED: run make_source.py --write')
    accepted=MAIN.read_bytes();snapshot=PARENT.read_bytes()
    require(accepted==snapshot,'immutable main parent equals copied genome bytes')
    gitblob=hashlib.sha1(b'blob '+str(len(accepted)).encode()+b'\0'+accepted).hexdigest()
    require(gitblob==EXPECTED_BLOB,'accepted parent Git blob identity')
    genome=parse_strict(snapshot)
    require(genome.get('schema_version')==1 and genome.get('engine_contract_version')==1 and
            genome.get('root_seed')=='0123456789abcdef','typed genome version/root seed')
    require([op.get('type_id') for op in genome['operators']]==[
      'fault.address-offset','fault.address-mask','fault.tile-permute','fault.address-burst'],
      'accepted parent operator sequence and exact topology')
    require(len({op['instance_id'] for op in genome['operators']})==4,'stable unique instance IDs')
    print('SOURCE-ONLY PREFLIGHT PASS. Native batch search, canonical candidates and visual selection NOT executed by this script.')

if __name__=='__main__':
    try: main()
    except (AssertionError,ValueError,OSError,KeyError,TypeError) as error:
        print('PRE-FLIGHT FAIL:',error,file=sys.stderr)
        sys.exit(1)
