#!/usr/bin/env python3
"""Structural/identity qualification only; does not emulate the native temporal renderer."""
from pathlib import Path
import json, hashlib, re, struct

HERE = Path(__file__).resolve().parent
SOURCE_SHA = "cbb58573b44b78a7a162289ede71eb541689d078b795960d0504888e3a8090de"
SOURCE_ID = "03502053b9d752dd2db92329108c2d1ed1c769e00db8a08f6ce7b9331992dc75"
GENOME_ID = "ac6651f58f3ee3b40d3852c6fa9a1435e5de2e6341cd7f5f894306d52695462c"
sha = lambda data: hashlib.sha256(data).hexdigest()

def normalized_bmp(payload):
    assert len(payload) >= 54 and payload[:2] == b"BM"
    size, offset = struct.unpack_from("<IxxxxI", payload, 2)
    header, width, height, planes, bpp, compression = struct.unpack_from("<IiiHHI", payload, 14)
    assert (size, offset, header, width, height, planes, bpp, compression) == (len(payload), 54, 40, 256, 144, 1, 24, 0)
    stride = (width*3+3) & ~3
    assert len(payload) == 54 + height*stride
    rgba = bytearray()
    for y in range(height):
        row = offset+(height-1-y)*stride
        for x in range(width):
            b,g,r = payload[row+3*x:row+3*x+3]
            rgba.extend((r,g,b,255))
    return width,height,rgba

def main():
    payload = (HERE/"source.bmp").read_bytes()
    assert sha(payload) == SOURCE_SHA, "source BMP encoding drift"
    w,h,rgba = normalized_bmp(payload)
    sid = sha(b"FAULTMINE-SOURCE-RGBA8-v1"+struct.pack("<II",w,h)+rgba)
    assert sid == SOURCE_ID, "normalized source identity drift"
    raw_genome = (HERE/"genome.json").read_bytes()
    genome = json.loads(raw_genome)
    # Git for Windows may translate canonical LF JSON to CRLF at checkout.
    # Compare exact canonical content after *only* CRLF translation, and hash the
    # LF bytes used by the portable genome identity contract; reject bare CR.
    canonical_genome = (json.dumps(genome,separators=(",",":"),ensure_ascii=False)+"\n").encode()
    assert raw_genome.replace(b"\r\n",b"\n") == canonical_genome, "noncanonical genome content"
    assert sha(canonical_genome) == GENOME_ID, "canonical genome identity drift"
    assert (genome["schema_version"],genome["engine_contract_version"],genome["root_seed"]) == (1,1,"a1b2c3d4e5f60718")
    operators = genome["operators"]
    assert len(operators) == 8
    assert len({x["instance_id"] for x in operators}) == 8
    assert all(re.fullmatch(r"[0-9a-f]{32}",x["instance_id"]) for x in operators)
    assert all(x["type_version"] == 1 and x["enabled"] for x in operators)
    assert [x["type_id"] for x in operators] == [
        "fault.row-offset","fault.channel-offset","temporal.tearing-phase",
        "temporal.channel-phase","temporal.partial-refresh","temporal.trail",
        "colour.quantize","temporal.timeline-rate"]
    assert all(v["kind"] in ("i64","u64","string","bool") for x in operators for v in x["parameters"].values())
    raw_project = (HERE/"signal-mausoleum.fmproj").read_bytes()
    project = json.loads(raw_project)
    canonical_project = (json.dumps(project,separators=(",",":"),ensure_ascii=False)+"\n").encode()
    assert raw_project.replace(b"\r\n",b"\n") == canonical_project, "noncanonical project content"
    assert project["project_version"] == 2
    assert project["source"] == {"path":"gallery/temporal/01-signal-mausoleum/source.bmp","identity":sid}
    assert project["genome"] == genome and project["locks"] == []
    lineage = project["lineage"]
    assert lineage["active_genome_identity"] == GENOME_ID and len(lineage["specimens"]) == 1
    root = lineage["specimens"][0]
    assert root["genome_identity"] == GENOME_ID and root["source_identity"] == sid
    assert root["genome"] == genome and root["creation_ordinal"] == 0
    assert root["derivation"] == {"kind":"manual-root","parents":[],"policy_version":0,"seed":None,"descendant_index":None,"radius":None}
    assert project["session"]["proxy_method_version"] == 1
    art = json.loads((HERE/"artwork.json").read_text(encoding="utf-8"))
    assert art["artwork_manifest_version"] == 1
    assert (art["source_identity"],art["genome_identity"]) == (SOURCE_ID,GENOME_ID)
    assert (art["frame_begin"],art["frame_end_exclusive"],art["rate_num"],art["rate_den"]) == (0,32,12,1)
    assert art["canonical_frame_status"] == "pending-windows-native-export"
    print("PASS: source BMP byte identity, normalized RGBA8 source identity, genome identity, v2 project lineage and authoring metadata")
    print("NOT VERIFIED: native WIC decode, native registry/schema validation, canonical frame replay, FM-012 export or GUI")

if __name__ == "__main__":
    main()
