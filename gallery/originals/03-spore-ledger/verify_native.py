#!/usr/bin/env python3
"""Read-only Windows CPU canonical PNG/receipt verifier; not FM-012 or GUI acceptance."""
import hashlib, json, struct, zlib
from pathlib import Path

ROOT=Path(__file__).resolve().parent
SOURCE="8afaccd3d00d080b330397f9ab78674832e67ec52c7e65464542d5214d181bc5"
GENOME="bd645afc85a5f8200a03b273ae2e6c1b5f975384cbe5152ebd9d8620eb206fec"
SEED="c6e15af039bd2471"
OUTPUT="8cf910493a00d66eb57970e23d3fc374fa9f581707546038692de2699eb0ed23"
ENCODED="a6fec5a377f00c2b0517f1f1c9411c94817a6abc11500fafee14563798f08959"
def sha(b): return hashlib.sha256(b).hexdigest()

def rgba8(b):
    assert b[:8]==b"\x89PNG\r\n\x1a\n"
    pos=8; idat=bytearray(); header=False; end=False
    while pos<len(b):
        assert pos+12<=len(b)
        n=struct.unpack_from(">I",b,pos)[0]; tag=b[pos+4:pos+8]
        assert n<=1048576 and pos+n+12<=len(b)
        payload=b[pos+8:pos+8+n]
        assert zlib.crc32(tag+payload)&0xffffffff==struct.unpack_from(">I",b,pos+8+n)[0]
        pos+=12+n
        if tag==b"IHDR":
            assert not header and pos==33 and struct.unpack(">IIBBBBB",payload)==(384,256,8,6,0,0,0)
            header=True
        elif tag==b"IDAT": assert header and not end; idat.extend(payload)
        elif tag==b"IEND": assert header and n==0 and pos==len(b); end=True
        else: assert tag in (b"sRGB",b"gAMA",b"pHYs",b"tEXt",b"iTXt")
    assert header and end and idat
    decoder=zlib.decompressobj(); src=decoder.decompress(bytes(idat),393473)+decoder.flush()
    assert decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail and len(src)==393472
    out=bytearray(); prev=bytearray(1536)
    for y in range(256):
        f=src[y*1537]; row=bytearray(src[y*1537+1:(y+1)*1537]); assert f in range(5)
        for x in range(1536):
            a=row[x-4] if x>=4 else 0; u=prev[x]; c=prev[x-4] if x>=4 else 0
            if f==1: p=a
            elif f==2: p=u
            elif f==3: p=(a+u)//2
            elif f==4:
                q=a+u-c; da,db,dc=abs(q-a),abs(q-u),abs(q-c)
                p=a if da<=db and da<=dc else u if db<=dc else c
            else:p=0
            row[x]=(row[x]+p)&255
        out.extend(row);prev=row
    assert len(out)==393216
    return out

def validate(png,receipt,art):
    assert sha(png)==ENCODED
    identity=sha(b"FAULTMINE-SOURCE-RGBA8-v1"+struct.pack("<II",384,256)+rgba8(png))
    assert identity==OUTPUT
    assert receipt==dict(audit_version=1,kind="author-cli-replay-not-FM012",source_identity=SOURCE,
        genome_identity=GENOME,root_seed=SEED,frame=0,engine_contract_version=1,
        output_identity=OUTPUT,encoded_png_sha256=ENCODED,release_verify="passed",
        debug_verify="passed",output_file="spore-ledger-canonical.png")
    assert art["native_render_status"]=="qualified-windows-native-cpu-debug-release"
    assert art["canonical_output_identity"]==OUTPUT
    assert art["canonical_output_file"]=="canonical-evidence/spore-ledger-canonical.png"
    assert art["canonical_png_sha256"]==ENCODED
    assert art["native_receipt_file"]=="canonical-evidence/native-replay-receipt.json"
    assert art["visual_evidence"]=="actual-windows-cpu-png-inspected"
    return identity

def main():
    proof=ROOT/"canonical-evidence"
    png=(proof/"spore-ledger-canonical.png").read_bytes()
    receipt=json.loads((proof/"native-replay-receipt.json").read_text(encoding="utf-8"))
    art=json.loads((ROOT/"artwork.json").read_text(encoding="utf-8"))
    result=validate(png,receipt,art)
    for data,record in ((png[:-1]+bytes([png[-1]^1]),receipt),(png,{**receipt,"frame":1})):
        try: validate(data,record,art)
        except (AssertionError,ValueError,zlib.error): pass
        else: raise AssertionError("forged/corrupt canonical proof accepted")
    print("PASS: archived Windows CPU PNG CRC, decoded RGBA8 identity, receipt, manifest, 2 negative tests",result)
    print("PENDING: physical native GUI/relink, FM-012 export and owner artistic acceptance")

if __name__=="__main__": main()
