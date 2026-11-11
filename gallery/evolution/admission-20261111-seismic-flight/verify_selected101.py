#!/usr/bin/env python3
"""Read-only qualification of the selected-first #57 reviewer project.

python gallery/evolution/admission-20261111-seismic-flight/verify_selected101.py
python gallery/evolution/admission-20261111-seismic-flight/verify_selected101.py --author-root PATH_TO_06-SEISMIC-DEBT

Author root must come from the exact PR #57 head. The review file is not an
admitted artwork or evidence of physical Win32 GUI acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib

HERE = Path(__file__).resolve().parent
WINNER = "aebb39166b6c61da4d847b3302021f393406c9de038df136b8097c1c28650acf"
FIRST = "326b3a89f555939c710d8943881b392600633ea80f5da6dedca493fb22f52802"
SOURCE_ID = "298051e0e8c048f36f4ac830baedf989e3daf4907fa360d75f59c8e3450dec88"
SOURCE_PATH = "gallery/evolution/06-seismic-debt/source.bmp"
SOURCE_SHA = "c751316e7615d53cf8a43e79e3a8395ec63f49392e7d16e37be16ad5f100f875"
PNG_SHA = "19629f3ccf873f8eb39cac7f8729c58ddb6a71e27b07039720f0766f32a9b337"
PROJECT_BLOB = "1c7a49bf47cd5d5b9fe55d4911220ebac7ed527c"
GENOME_BLOB = "6802cb60fdab63af7d27692becd963a50b2b59a7"

def check(ok, error):
    if not ok:
        raise ValueError(error)

def no_dupes(pairs):
    obj = {}
    for k, v in pairs:
        check(k not in obj, "duplicate JSON key " + k)
        obj[k] = v
    return obj

def parse(raw):
    return json.loads(raw.decode("utf-8"), object_pairs_hook=no_dupes,
                      parse_constant=lambda c: (_ for _ in ()).throw(ValueError("nonfinite " + c)))

def gitblob(raw):
    return hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()

def png(raw):
    check(raw.startswith(b"\x89PNG\r\n\x1a\n"), "not PNG")
    offset, dims, ended = 8, None, False
    while offset < len(raw):
        check(offset + 12 <= len(raw), "PNG truncated header")
        size = struct.unpack_from(">I", raw, offset)[0]
        check(size < 16 * 1024 * 1024 and offset + size + 12 <= len(raw), "PNG malformed length")
        kind, data = raw[offset+4:offset+8], raw[offset+8:offset+8+size]
        crc = struct.unpack_from(">I", raw, offset+8+size)[0]
        check(crc == zlib.crc32(kind + data) & 0xffffffff, "PNG CRC failure")
        if offset == 8:
            check(kind == b"IHDR" and size == 13, "PNG missing IHDR")
            dims = struct.unpack_from(">II", data)
        offset += size + 12
        if kind == b"IEND":
            check(offset == len(raw), "PNG trailing bytes")
            ended = True
            break
    check(ended and dims == (384, 240), "PNG dimension/IEND mismatch")

def review_check(project):
    check(project.get("project_version") == 2, "not project v2")
    check(set(project) == {"project_version", "source", "genome", "locks", "lineage", "session", "ui"},
          "unexpected project fields")
    check(project["source"] == {"identity": SOURCE_ID, "path": SOURCE_PATH}, "wrong portable source")
    lineage = project["lineage"]
    check(lineage["active_genome_identity"] == WINNER, "review opens wrong genome")
    specimens = lineage["specimens"]
    check(isinstance(specimens, list) and len(specimens) == 14, "wrong specimen inventory")
    matches = [s for s in specimens if s.get("genome_identity") == WINNER]
    check(len(matches) == 1, "winner not uniquely retained")
    s = matches[0]
    check(s["genome"] == project["genome"] and s["source_identity"] == SOURCE_ID,
          "selected genome/source mismatch")
    check(s["favourite"] is True, "winner not a favourite")
    d = s["derivation"]
    check(d["kind"] == "mutation" and d["descendant_index"] == 101
          and d["seed"] == "6ac9f2e430bd7185" and d["radius"] == "high"
          and d["policy_version"] == 1, "wrong indexed mutation")
    check(project["genome"]["schema_version"] == 1
          and project["genome"]["root_seed"] == "0123456789abcdef", "wrong genome contract")
    return len(specimens)

def author_check(root, selected):
    raw = (root / "curated/batch.fmproj").read_bytes()
    check(gitblob(raw) == PROJECT_BLOB, "original project Git blob changed")
    author = parse(raw)
    check(author["lineage"]["active_genome_identity"] == FIRST, "original batch starting point changed")
    check(author["source"]["identity"] == SOURCE_ID, "author source identity mismatch")
    author["source"]["path"] = SOURCE_PATH
    author["genome"] = selected["genome"]
    author["lineage"]["active_genome_identity"] = WINNER
    check(author == selected, "review copy changed more than source path/genome/active ID")
    check(hashlib.sha256((root / "source.bmp").read_bytes()).hexdigest() == SOURCE_SHA,
          "original authored BMP changed")
    raw = (root / "curated/the-broken-register.png").read_bytes()
    check(hashlib.sha256(raw).hexdigest() == PNG_SHA, "original native winner bytes changed")
    png(raw)
    genome = (root / "curated/the-broken-register.genome.json").read_bytes()
    check(gitblob(genome) == GENOME_BLOB and parse(genome) == selected["genome"],
          "original curated genome changed")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--author-root", type=Path,
                        help="gallery/evolution/06-seismic-debt from exact author PR #57")
    args = parser.parse_args()
    review = parse((HERE / "seismic-101-selected-first.fmproj").read_bytes())
    count = review_check(review)
    print(f"PASS: active Seismic #101, {count} retained specimens, pinned source identity")
    if args.author_root:
        author_check(args.author_root.resolve(strict=True), review)
        print("PASS: byte-pinned authored project, original BMP/PNG/genome, only three allowed review deltas")
    else:
        print("LIMITATION: pass --author-root for source/PNG/project byte comparisons")
    print("NOT QUALIFIED: physical Win32 source load, FM-012 export/cancel or owner artistic acceptance")

if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, TypeError, OSError, IndexError) as ex:
        sys.exit("FAIL: " + str(ex))
