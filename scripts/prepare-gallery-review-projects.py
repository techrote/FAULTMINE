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
# Native WIC PNG pinned by the Windows-qualified Shearwing #53 evidence.
WINNER_PNG_SHA256 = "41a2e7863eecd7ef51d460dd7d7909b220b915ee60941cd2077d5df0bb2a4cf5"
WINNER_CANONICAL_RGBA8_ID = "6f149f34cb9eca441c4206de1b5e7b0f035cab3b2ad958e3401f5a1cef2d9c50"


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
    # Batch project opens descendant #0, but the selected native artwork is
    # #65. Preserve its full lineage and provide a separate selected-first copy.
    label = "04-shearwing-WINNER-65"
    source_rel = WORKS[3][2]
    original_rel = WORKS[3][1]
    genome_rel = "gallery/evolution/05-shearwing-telemetry/curated/shearwing-65.genome.json"
    png_rel = "gallery/evolution/05-shearwing-telemetry/curated/shearwing-65.png"
    genome_path, png_path = package_root / genome_rel, package_root / png_rel
    if not genome_path.is_file() or not png_path.is_file():
        raise ValueError(f"{label}: curated genome or native PNG missing")
    if sha256(png_path.read_bytes()) != WINNER_PNG_SHA256:
        raise ValueError(f"{label}: archived native PNG differs from verified Windows evidence")
    curated_genome = load_strict(genome_path)
    authored = load_strict(package_root / original_rel)
    members = authored["lineage"]["specimens"]
    if not isinstance(members, list):
        raise ValueError(f"{label}: authored lineage is missing")
    addressed = [
        specimen for specimen in members
        if isinstance(specimen, dict)
        and isinstance(specimen.get("derivation"), dict)
        and specimen["derivation"].get("descendant_index") == 65
    ]
    if len(addressed) != 1:
        raise ValueError(f"{label}: expected exactly one lineage descendant #65")
    winner = addressed[0]
    selected_id = winner.get("genome_identity")
    if (winner.get("genome") != curated_genome
            or winner.get("source_identity") != authored["source"]["identity"]
            or not isinstance(selected_id, str) or not HEX64.fullmatch(selected_id)):
        raise ValueError(f"{label}: curated genome, source or lineage identity mismatch")
    selected_project = load_strict(package_root / original_rel)
    selected_project["source"]["path"] = source_rel
    selected_project["genome"] = curated_genome
    selected_project["lineage"]["active_genome_identity"] = selected_id
    payload = (json.dumps(selected_project, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")
    reconstructed = json.loads(payload)
    reconstructed["source"]["path"] = authored["source"]["path"]
    reconstructed["genome"] = authored["genome"]
    reconstructed["lineage"]["active_genome_identity"] = authored["lineage"]["active_genome_identity"]
    if reconstructed != authored:
        raise ValueError(f"{label}: selected overlay modified fields beyond active genome, active lineage and source path")
    dest = output / (label + ".fmproj")
    write_new_or_same(dest, payload)
    selected_record = {
        "label": label, "project": dest.relative_to(package_root).as_posix(),
        "portable_project_sha256": sha256(payload),
        "authored_project": original_rel, "authored_project_sha256": sha256((package_root / original_rel).read_bytes()),
        "curated_genome": genome_rel, "curated_genome_encoded_sha256": sha256(genome_path.read_bytes()),
        "native_png": png_rel, "native_png_encoded_sha256": WINNER_PNG_SHA256,
        "native_decoded_rgba8_identity_from_windows_verification": WINNER_CANONICAL_RGBA8_ID,
        "source_normalized_identity_from_project": authored["source"]["identity"],
        "descendant_index": 65, "selected_genome_identity_from_lineage": selected_id,
        "canonical_authority": False,
    }
    readme = ("FAULTMINE FOUR-FAMILY REVIEW PROJECTS — PORTABLE COPIES\n\n"
              "From the extracted package root double-click Run FAULTMINE.cmd.\n"
              "In FAULTMINE press Ctrl+Shift+O and open a .fmproj from this folder.\n"
              "01 Angle of Departure; 02 Silent Bell; 03 Seat Count Minus One; 04 Shearwing.\n\n"
              "These copies use paths relative to the package root, where the shipped\n"
              "launcher sets the process working directory. Authored projects are unchanged.\n"
              "Source identities, genomes, root seeds, lineage and canonical pixels\n"
              "were not recalculated. A relink prompt means physical GUI acceptance\n"
              "is still needed; never accept different-content source pixels.\n"
              "04-shearwing.fmproj opens the full batch lineage at #0.\n"
              "04-shearwing-WINNER-65.fmproj opens curated descendant #65 directly,\n"
              "retaining the complete authored batch lineage. Use it for owner review.\n"
              "Native CLI replay does not qualify GUI export/cancellation.\n")
    write_new_or_same(output / "START-HERE.txt", readme.encode("utf-8"))
    manifest = {"format_version": 1, "kind": "portable-review-project-path-overlays",
                "canonical_authority": False, "works": records,
                "selected_review_project": selected_record}
    write_new_or_same(output / "provenance.json",
                      (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    print(f"PASS: {len(records)} portable project copies + verified winner #65; authored source/genome/lineage unchanged")
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
