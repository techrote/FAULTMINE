#!/usr/bin/env python3
"""Read-only source/genome/project verifier. Does not claim a native render."""
import hashlib, json, struct, zlib, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = "b20301c86bbaf0bb1f886fce9fe4f70017286bc0f13b734e261d3a982f049435"
SID = "05ec476c708b7b569ce4c192b1d32d5270d5fb057619c238dc3d46c40a713b69"
GID = "58f440d271d56e81e94de2f395931de1774af8a92d64627dad9514a41722e313"
SEED = "a17e08c4d3ad1e02"
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
    if sys.flags.optimize:
        raise SystemExit('STOP: optimized Python disables assertion checks; run without -O/-OO')
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
    pr,_=canonical(HERE/"witness-relay.fmproj")
    assert pr["project_version"]==2 and pr["source"]=={"path":"gallery/originals/02-witness-relay/source.png","identity":SID}
    assert pr["genome"]==g and pr["locks"]==[]
    assert pr["lineage"]["active_genome_identity"]==GID
    assert len(pr["lineage"]["specimens"])==1
    node=pr["lineage"]["specimens"][0]
    assert node["genome_identity"]==GID and node["source_identity"]==SID and node["genome"]==g
    assert node["derivation"]==dict(kind="manual-root",parents=[],policy_version=0,seed=None,descendant_index=None,radius=None)
    assert sha((HERE/"artwork.json").read_bytes())=="415faafe7d8a04cec05580c712851a61af84bc2c7b1a517b356e5a6eaec18907", "original manifest bytes changed"
    art=json.loads((HERE/"artwork.json").read_text(encoding="utf8"))
    assert art["source_encoding_sha256"]==SRC and art["source_identity"]==SID
    assert art["genome_identity"]==GID and art["root_seed"]==SEED and art["explicit_frame"]==0
    assert art["native_render_status"]=="verified-windows-native-cpu-release-debug"
    assert art["canonical_output_identity"]=="34009717b2be5aec243ce405d608c5bda001933b8f898cedf4e153caf2ba728c" and art["canonical_output_file"]=="canonical-evidence/witness-relay-canonical.png"
    assert art["canonical_png_encoding_sha256"]=="e132120a1515a8a70f5e504a6993788ff500ce8aa60f85d531c5eee0d9a1c67a" and art["native_receipt_file"]=="canonical-evidence/native-replay-receipt.json"
    print("PASS: 384x256 indexed source PNG CRC, decoded RGBA8 source identity, canonical genome, typed stack and project-v2 lineage")
    evidence=HERE/"canonical-evidence"
    receipt=evidence/"native-replay-receipt.json"
    canonical_png=evidence/"witness-relay-canonical.png"
    assert receipt.is_file() and canonical_png.is_file(), "committed Windows native PNG and receipt are required"
    if receipt.is_file():
        assert sha(receipt.read_bytes())=="78a1a41e9ba73ecace073a7f8b2fbfb2b4e0a603b5598a17e0374e7d07ccc841", "Windows receipt changed"
        r=json.loads(receipt.read_text(encoding="utf-8"))
        assert (r["frame"],r["root_seed"],r["source_identity"],r["genome_identity"],r["output_identity"],r["encoded_png_sha256"],r["release_verify"],r["debug_verify"]) == (0,SEED,SID,GID,"34009717b2be5aec243ce405d608c5bda001933b8f898cedf4e153caf2ba728c","e132120a1515a8a70f5e504a6993788ff500ce8aa60f85d531c5eee0d9a1c67a","passed","passed"), "archived native evidence differs"
        if canonical_png.is_file():
            assert sha(canonical_png.read_bytes())=="e132120a1515a8a70f5e504a6993788ff500ce8aa60f85d531c5eee0d9a1c67a", "archived native PNG changed"
            print("PASS: byte-exact archived Windows canonical PNG/receipt; physical GUI/FM-012 and aesthetic acceptance still pending")
        else:
            print("PENDING: native PNG not in author branch; use the full owner review ZIP for actual pixel verification")
    else:
        print("PENDING: native evidence absent; Windows and GUI/artistic verification needed")

if __name__=="__main__": main()
