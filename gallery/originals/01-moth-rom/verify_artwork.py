#!/usr/bin/env python3
"""Read-only structural and identity verification; does not claim native canonical pixels."""
from pathlib import Path
import hashlib
import json
import struct

HERE = Path(__file__).resolve().parent
SOURCE_SHA = "41a2be8d45d83c4b15e09ce9d1846256cb89f71d48551f592d1e78f772670e48"
SOURCE_ID = "49aca43e0330c82dec3d579f42f6cca3728603e5155cec434bf0ee81303fff4e"
GENOME_ID = "fa4fda19e0701619a57b2c3eb0ee422ebf8f9f6911537ccc9ee912c125bf72e4"
ROOT_SEED = "0d07c1cada55e7a1"
SHA = lambda payload: hashlib.sha256(payload).hexdigest()


def normalized_bmp(payload):
    assert len(payload) >= 54 and payload[:2] == b"BM", "BMP signature"
    file_size, pixel_offset = struct.unpack_from("<IxxxxI", payload, 2)
    header_size, width, height, planes, bpp, compression = struct.unpack_from("<IiiHHI", payload, 14)
    assert (file_size, pixel_offset, header_size, width, height, planes, bpp, compression) == (
        len(payload), 54, 40, 384, 256, 1, 24, 0
    ), "BMP header contract"
    stride = (width * 3 + 3) & ~3
    assert len(payload) == 54 + height * stride, "BMP length"
    rgba = bytearray()
    for y in range(height):
        row = pixel_offset + (height - 1 - y) * stride
        for x in range(width):
            b, g, r = payload[row + 3*x:row + 3*x + 3]
            rgba.extend((r, g, b, 255))
    return width, height, rgba


def exact_json(path):
    raw = path.read_bytes()
    obj = json.loads(raw)
    assert raw == (json.dumps(obj, ensure_ascii=False, separators=(",", ":")) + "\n").encode(), f"noncanonical JSON: {path}"
    return obj, raw


def main():
    src = (HERE / "source.bmp").read_bytes()
    assert SHA(src) == SOURCE_SHA, "encoded source drift"
    w, h, rgba = normalized_bmp(src)
    source_identity = SHA(b"FAULTMINE-SOURCE-RGBA8-v1" + struct.pack("<II", w, h) + rgba)
    assert source_identity == SOURCE_ID, "normalized source drift"

    genome, raw_genome = exact_json(HERE / "genome.json")
    assert SHA(raw_genome) == GENOME_ID, "genome canonical identity drift"
    assert list(genome) == ["schema_version", "engine_contract_version", "root_seed", "operators"]
    assert (genome["schema_version"], genome["engine_contract_version"], genome["root_seed"]) == (1, 1, ROOT_SEED)
    operators = genome["operators"]
    assert len(operators) == 7
    types = ["fault.address-burst", "fault.channel-offset", "fault.bit-burst",
             "fault.scanline-jitter", "fault.bitplane-swap", "colour.quantize", "colour.dither-ordered"]
    assert [o["type_id"] for o in operators] == types
    assert len({o["instance_id"] for o in operators}) == len(operators)
    assert all(len(o["instance_id"]) == 32 and all(c in "0123456789abcdef" for c in o["instance_id"]) for o in operators)
    for o in operators:
        assert list(o) == ["instance_id", "type_id", "type_version", "enabled", "parameters"]
        assert o["type_version"] == 1 and o["enabled"] is True
        assert list(o["parameters"]) == sorted(o["parameters"])
        for p in o["parameters"].values():
            assert list(p) == ["kind", "value"]
            assert p["kind"] in {"string", "i64", "u64", "bool"}
            if p["kind"] == "u64":
                assert type(p["value"]) is int and 0 <= p["value"] < (1 << 64)
            elif p["kind"] == "i64":
                assert type(p["value"]) is int and -(1 << 63) <= p["value"] < (1 << 63)
            elif p["kind"] == "bool":
                assert type(p["value"]) is bool
            else:
                assert type(p["value"]) is str

    project, _ = exact_json(HERE / "moth-rom.fmproj")
    assert list(project) == ["project_version", "source", "genome", "locks", "lineage", "session", "ui"]
    assert project["project_version"] == 2 and project["locks"] == []
    assert project["source"] == {"path": "gallery/originals/01-moth-rom/source.bmp", "identity": SOURCE_ID}
    assert project["genome"] == genome
    lineage = project["lineage"]
    assert lineage["active_genome_identity"] == GENOME_ID and len(lineage["specimens"]) == 1
    node = lineage["specimens"][0]
    assert node["genome_identity"] == GENOME_ID and node["source_identity"] == SOURCE_ID
    assert node["genome"] == genome and node["favourite"] is True and node["creation_ordinal"] == 0
    assert node["derivation"] == {"kind":"manual-root","parents":[],"policy_version":0,
                                  "seed":None,"descendant_index":None,"radius":None}
    assert project["session"] == {"proxy_enabled":False, "proxy_max_width":1280, "proxy_max_height":960,
                                  "proxy_method_version":1, "selected_instance_id":""}
    assert project["ui"] == {"view_mode":"fit","zoom_milli":1000,"pan_x_milli":0,
                             "pan_y_milli":0,"show_before":False}

    art = json.loads((HERE / "artwork.json").read_text(encoding="utf-8"))
    assert art["artwork_manifest_version"] == 1
    assert art["source_encoding_sha256"] == SOURCE_SHA
    assert art["source_identity"] == SOURCE_ID and art["genome_identity"] == GENOME_ID
    assert art["root_seed"] == ROOT_SEED and art["source_dimensions"] == [w, h]
    assert art["explicit_frame"] == 0
    assert art["native_render_status"] == "pending-windows-execution"
    assert art["canonical_output_identity"] is None and art["canonical_output_file"] is None
    print("PASS: source BMP encoding and normalized RGBA8 identity")
    print("PASS: canonical genome SHA256, seven typed operators, project-v2 lineage and authoring manifest")
    print("NOT VERIFIED: native Windows WIC decode, registry acceptance, post-fault canonical PNG and exact replay")


if __name__ == "__main__":
    main()
