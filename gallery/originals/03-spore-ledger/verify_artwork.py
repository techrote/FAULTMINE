#!/usr/bin/env python3
"""Read-only source/genome/project plus archived native CPU output verifier."""
import hashlib, json, struct, zlib
from pathlib import Path

if not __debug__:
    raise SystemExit("Optimized Python (-O/-OO) disables artwork assertions; verification refused.")

HERE = Path(__file__).resolve().parent
SRC = "229cf7c953ce5650aec86df9a606b8fa338d4b210085c0d6d938ff6b932f5021"
SID = "8afaccd3d00d080b330397f9ab78674832e67ec52c7e65464542d5214d181bc5"
GID = "bd645afc85a5f8200a03b273ae2e6c1b5f975384cbe5152ebd9d8620eb206fec"
SEED = "c6e15af039bd2471"
def sha(b): return hashlib.sha256(b).hexdigest()

def canonical(path):
    raw = path.read_bytes().replace(b"\r\n",b"\n")
    assert b"\r" not in raw
    def nodup(pairs):
        out = {}
        for k,v in pairs:
            assert k not in out, "duplicate JSON key "+k
            out[k] = v
        return out
    value = json.loads(raw.decode("utf-8"),object_pairs_hook=nodup)
    assert raw == (json.dumps(value,ensure_ascii=False,separators=(",",":"))+"\n").encode("utf-8"), path
    return value,raw

def png_rgba(b):
    assert b[:8] == b"\x89PNG\r\n\x1a\n"
    off=8; palette=None; data=bytearray(); dimensions=None; ended=False
    while off < len(b):
        assert off+12 <= len(b)
        n=struct.unpack_from(">I",b,off)[0]
        tag=b[off+4:off+8]
        assert off+12+n <= len(b)
        chunk=b[off+8:off+8+n]
        assert (zlib.crc32(tag+chunk)&0xffffffff)==struct.unpack_from(">I",b,off+8+n)[0],tag
        off+=n+12
        if tag==b"IHDR":
            assert dimensions is None and n==13
            w,h,depth,colour,comp,fil,inter=struct.unpack(">IIBBBBB",chunk)
            assert (w,h,depth,colour,comp,fil,inter)==(384,256,4,3,0,0,0)
            dimensions=(w,h)
        elif tag==b"PLTE":
            assert n%3==0 and n<=48
            palette=[tuple(chunk[i:i+3]) for i in range(0,n,3)]
        elif tag==b"IDAT": data.extend(chunk)
        elif tag==b"IEND":
            assert n==0 and off==len(b)
            ended=True
            break
        else: assert tag not in (b"tRNS",b"gAMA",b"iCCP"), "unexpected colour metadata"
    assert dimensions==(384,256) and ended and palette is not None
    decompressor=zlib.decompressobj()
    raw=decompressor.decompress(bytes(data))+decompressor.flush()
    assert decompressor.eof and not decompressor.unused_data
    stride=192; assert len(raw)==(stride+1)*256
    rgba=bytearray(); prev=bytearray(stride); pos=0
    for y in range(256):
        mode=raw[pos];pos+=1;row=bytearray(raw[pos:pos+stride]);pos+=stride
        assert mode in range(5)
        for x in range(stride):
            a=row[x-1] if x else 0; b=prev[x]; c=prev[x-1] if x else 0
            if mode==1: p=a
            elif mode==2: p=b
            elif mode==3: p=(a+b)//2
            elif mode==4:
                q=a+b-c; da=abs(q-a);db=abs(q-b);dc=abs(q-c)
                p=a if da<=db and da<=dc else b if db<=dc else c
            else:p=0
            row[x]=(row[x]+p)&255
        for v in row:
            for idx in (v>>4,v&15):
                assert idx < len(palette)
                rgba.extend((*palette[idx],255))
        prev=row
    assert len(rgba)==384*256*4
    return rgba

def main():
    if sha((HERE/"artwork.json").read_bytes()) != "cf9cee83bc76fc05ea7613c451a119037713d06b7d958258f767cec9b5bfd7ba":
        raise ValueError("Complete artwork manifest SHA-256 changed")
    if sha((HERE/"spore-ledger.fmproj").read_bytes()) != "065615737bc1b59d25bfff03a0c55b99977957c7fe4232cd060a23efbe0717c5":
        raise ValueError("Authored project bytes changed")
    if sha((HERE/"canonical-evidence/native-replay-receipt.json").read_bytes()) != "e7b4335168d4a908d9e20bbc1abfeaad5470dbc42cc27f4522b3aeb0fe967d7c":
        raise ValueError("Native replay receipt bytes changed")
    b=(HERE/"source.png").read_bytes()
    assert sha(b)==SRC, "source encoding changed"
    rgba=png_rgba(b)
    # Regression: a modified palette byte must fail the original PNG CRC.
    damaged=bytearray(b)
    damaged[60]^=1
    try:
        png_rgba(damaged)
    except (AssertionError, ValueError, IndexError):
        pass
    else:
        raise AssertionError("corrupted PNG unexpectedly accepted")
    assert sha(b"FAULTMINE-SOURCE-RGBA8-v1"+struct.pack("<II",384,256)+rgba)==SID, "normalized source changed"
    g,raw=canonical(HERE/"genome.json")
    assert sha(raw)==GID, "canonical genome changed"
    assert (g["schema_version"],g["engine_contract_version"],g["root_seed"])==(1,1,SEED)
    assert [o["type_id"] for o in g["operators"]]==[
      "fault.band-repeat","fault.address-burst","fault.channel-offset",
      "fault.bit-burst","colour.quantize","colour.dither-ordered"]
    assert len({o["instance_id"] for o in g["operators"]})==6
    for o in g["operators"]:
        assert list(o)==["instance_id","type_id","type_version","enabled","parameters"]
        assert len(o["instance_id"])==32 and o["type_version"]==1 and o["enabled"] is True
        assert list(o["parameters"])==sorted(o["parameters"])
        for p in o["parameters"].values(): assert list(p)==["kind","value"]
    pr,_=canonical(HERE/"spore-ledger.fmproj")
    assert pr["project_version"]==2 and pr["source"]=={"path":"gallery/originals/03-spore-ledger/source.png","identity":SID}
    assert pr["genome"]==g and pr["locks"]==[]
    assert pr["lineage"]["active_genome_identity"]==GID
    assert len(pr["lineage"]["specimens"])==1
    node=pr["lineage"]["specimens"][0]
    assert node["genome_identity"]==GID and node["source_identity"]==SID and node["genome"]==g
    assert node["derivation"]==dict(kind="manual-root",parents=[],policy_version=0,seed=None,descendant_index=None,radius=None)
    art=json.loads((HERE/"artwork.json").read_text(encoding="utf8"))
    assert art["source_encoding_sha256"]==SRC and art["source_identity"]==SID
    assert art["genome_identity"]==GID and art["root_seed"]==SEED and art["explicit_frame"]==0
    from verify_native import main as verify_native_main
    verify_native_main()
    print("PASS: 384x256 indexed source PNG CRC, decoded RGBA8 source identity, canonical genome, typed stack, project-v2 lineage and archived native output")

if __name__=="__main__": main()
