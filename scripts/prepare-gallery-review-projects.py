#!/usr/bin/env python3
"""Build portable copies of the four pinned Windows gallery review projects.

Builder-side Python stdlib only. The shipped ZIP needs no Python. Canonical
pixels, authored project files, genomes and lineage are never rewritten.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

WORKS = (
    ("01-angle", "gallery/temporal/04-the-angle-of-departure/the-angle-of-departure.fmproj", "gallery/temporal/04-the-angle-of-departure/source.bmp"),
    ("02-bell", "gallery/originals/04-the-silent-bell/the-silent-bell.fmproj", "gallery/originals/04-the-silent-bell/source.png"),
    ("03-seat", "gallery/temporal/05-seat-count-minus-one/seat-count-minus-one.fmproj", "gallery/temporal/05-seat-count-minus-one/source.bmp"),
    ("04-shearwing", "CANONICAL-REVIEW-EVIDENCE/shearwing-05/batch.fmproj", "gallery/evolution/05-shearwing-telemetry/source.bmp"),
)
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
HEX16 = re.compile(r"[0-9a-f]{16}\Z")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def unique_fields(pairs):
    output = {}
    for key, value in pairs:
        if key in output:
            raise ValueError(f"duplicate JSON field: {key}")
        output[key] = value
    return output


def load_strict(path):
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_fields,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(f"nonfinite JSON: {value}")))


def write_new_or_same(path, data):
    if path.exists():
        if path.read_bytes() != data:
            raise ValueError(f"refusing to overwrite changed review file: {path}")
    else:
        path.write_bytes(data)


def create(package_root):
    package_root = Path(package_root).resolve(strict=True)
    output = package_root / "REVIEW-PROJECTS"
    output.mkdir(exist_ok=True)
    records = []
    for label, original_rel, source_rel in WORKS:
        original = package_root / original_rel
        source = package_root / source_rel
        if not original.is_file() or not source.is_file():
            raise ValueError(f"{label}: required project/source missing: {original_rel} / {source_rel}")
        original_bytes = original.read_bytes()
        project = load_strict(original)
        if project.get("project_version") != 2 or not isinstance(project.get("source"), dict):
            raise ValueError(f"{label}: project schema v2 required")
        genome = project.get("genome")
        if not isinstance(genome, dict) or genome.get("schema_version") != 1 or not HEX16.fullmatch(str(genome.get("root_seed", ""))):
            raise ValueError(f"{label}: genome v1 / root seed required")
        source_record = project["source"]
        recorded_path = source_record.get("path")
        identity = source_record.get("identity")
        if not isinstance(recorded_path, str) or not isinstance(identity, str) or not HEX64.fullmatch(identity):
            raise ValueError(f"{label}: missing/invalid source path or normalized identity")
        # The native batch project has an original absolute CI runner path.
        # Other author-created projects must reference their exact gallery source.
        if label == "04-shearwing":
            if recorded_path.replace("\\", "/").lower().split("/gallery/")[-1] != source_rel.removeprefix("gallery/").lower():
                raise ValueError("Shearwing batch project references an unexpected source")
        elif recorded_path.replace("\\", "/") != source_rel:
            raise ValueError(f"{label}: unexpected author source path: {recorded_path}")
        project["source"]["path"] = source_rel
        payload = (json.dumps(project, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")
        reconstructed = json.loads(payload)
        reconstructed["source"]["path"] = recorded_path
        if reconstructed != load_strict(original):
            raise ValueError(f"{label}: portable project modified fields beyond source path")
        dest = output / (label + ".fmproj")
        write_new_or_same(dest, payload)
        if original.read_bytes() != original_bytes:
            raise ValueError(f"{label}: authored project changed during staging")
        records.append({"label": label, "project": dest.relative_to(package_root).as_posix(),
                        "authored_project": original_rel,
                        "authored_project_sha256": sha256(original_bytes),
                        "portable_project_sha256": sha256(payload),
                        "recorded_source_path": recorded_path,
                        "portable_source_path": source_rel,
                        "source_encoded_sha256": sha256(source.read_bytes()),
                        "source_normalized_identity_from_project": identity,
                        "root_seed": genome["root_seed"]})
    readme = ("FAULTMINE FOUR-FAMILY REVIEW PROJECTS — PORTABLE COPIES\n\n"
              "From the extracted package root double-click Run FAULTMINE.cmd.\n"
              "In FAULTMINE press Ctrl+Shift+O and open a .fmproj from this folder.\n"
              "01 Angle of Departure; 02 Silent Bell; 03 Seat Count Minus One; 04 Shearwing.\n\n"
              "These copies use paths relative to the package root, where the shipped\n"
              "launcher sets the process working directory. Authored projects are unchanged.\n"
              "Source identities, genomes, root seeds, lineage and canonical pixels\n"
              "were not recalculated. A relink prompt means physical GUI acceptance\n"
              "is still needed; never accept different-content source pixels.\n"
              "Shearwing opens a batch lineage, NOT automatically descendant #65.\n"
              "Native CLI replay does not qualify GUI export/cancellation.\n")
    write_new_or_same(output / "START-HERE.txt", readme.encode("utf-8"))
    manifest = {"format_version": 1, "kind": "portable-review-project-path-overlays",
                "canonical_authority": False, "works": records}
    write_new_or_same(output / "provenance.json",
                      (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    print(f"PASS: {len(records)} portable project copies; authored source/genome/lineage unchanged")
    for row in records:
        print(row["label"], row["portable_project_sha256"], row["source_normalized_identity_from_project"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-root", required=True, type=Path)
    args = parser.parse_args()
    try:
        create(args.package_root)
    except (ValueError, OSError, json.JSONDecodeError) as exc:
        parser.exit(1, f"FAIL: {exc}\n")
