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


def parse_canonical_json(raw, label):
    # Git stores canonical LF bytes. A default Git-for-Windows checkout may
    # translate them to CRLF; validate the canonical LF semantic bytes without
    # accepting a changed field, extra blank line, bare CR or duplicate key.
    normalized = raw.replace(b"\r\n", b"\n")
    assert b"\r" not in normalized, f"unexpected carriage return: {label}"

    def no_duplicate_keys(pairs):
        result = {}
        for key, value in pairs:
            assert key not in result, f"duplicate JSON field {key}: {label}"
            result[key] = value
        return result

    obj = json.loads(normalized.decode("utf-8"), object_pairs_hook=no_duplicate_keys)
    expected = (json.dumps(obj, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")
    assert normalized == expected, f"noncanonical JSON: {label}"
    return obj, normalized


def exact_json(path):
    return parse_canonical_json(path.read_bytes(), str(path))


def main():
    # Verify both working-tree newline representations against the same LF
    # canonical identity, and reject bare CR/duplicate-field corruption.
    sample = b'{"a":1,"b":2}\\n'.replace(b"\\n", b"\n")
    assert parse_canonical_json(sample, "LF")[1] == sample
    assert parse_canonical_json(sample.replace(b"\n", b"\r\n"), "CRLF")[1] == sample
    for malformed in (b'{"a":1}\\r\\n\\n'.replace(b"\\r", b"\r").replace(b"\\n", b"\n"),
                      b'{"a":1,"a":2}\n'):
        try:
            parse_canonical_json(malformed, "negative fixture")
        except (AssertionError, ValueError):
            pass
        else:
            raise AssertionError("malformed fixture unexpectedly accepted")
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
    assert art["native_render_status"] == "windows-native-cpu-decoded-replay-verified"
    assert art["canonical_output_identity"] == "171e395bac997e35d740f400cbbce3e9fdb556056f5be7fd45eef5d158536c7c"
    assert art["canonical_output_file"] == "moth-rom-canonical.png"
    assert art["archived_png_encoded_sha256"] == "59488382c0ad776ee00dc1419d11ba1ddbf79871b161059037a6f4234af33088"
    assert art["native_png_encoded_sha256"] == "70f8ed386a18f8cdb76212ddf7ec0799b8dfba8ab19f2b3d24f0e511ff0f8b81"
    assert (art["native_evidence_run"], art["native_evidence_artifact"]) == (42939377682, 13820331534)
    from verify_canonical import verify as verify_archived_pixels
    assert verify_archived_pixels() == art["canonical_output_identity"]
    print("PASS: source BMP encoding and normalized RGBA8 identity")
    print("PASS: LF-canonical genome SHA256 (CRLF checkout safe), seven typed operators, project-v2 lineage and authoring manifest")
    print("PASS: committed decoded RGBA8 canonical PNG identity matches independently recorded native Windows output")
    print("PENDING: exact-head Windows verification of newly committed PNG, GUI/FM-012 and independent artistic acceptance")


if __name__ == "__main__":
    main()
