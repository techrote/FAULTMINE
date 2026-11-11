"""Regression tests for package-only, provenance-preserving gallery path overlays."""
import importlib.util
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "prepare-gallery-review-projects.py"
spec = importlib.util.spec_from_file_location("gallery_review", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReviewProjectTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.originals = {}
        for label, project_rel, source_rel in module.WORKS:
            path, source = self.root / project_rel, self.root / source_rel
            path.parent.mkdir(parents=True, exist_ok=True)
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_bytes(b"fake encoded image for path-only isolated tests")
            original_source = ("D:\\a\\FAULTMINE\\FAULTMINE\\" + source_rel.replace("/", "\\")) if label == "04-shearwing" else source_rel
            doc = {"project_version": 2,
                   "source": {"path": original_source, "identity": "a" * 64},
                   "genome": {"schema_version": 1, "engine_contract_version": 1,
                              "root_seed": "0123456789abcdef", "operators": []},
                   "locks": [], "lineage": {"active_genome_identity": "b" * 64, "specimens": []},
                   "session": {"proxy_enabled": True}, "ui": {"zoom_milli": 1000}}
            path.write_text(json.dumps(doc), encoding="utf-8")
            self.originals[path] = path.read_bytes()

        # Fixture PNG bytes are not canonical pixels. Native CI verifies
        # the actual curated PNG, while these tests protect overlay semantics.
        shear_path = self.root / module.WORKS[3][1]
        shear = json.loads(shear_path.read_text())
        self.selected_genome = copy.deepcopy(shear["genome"])
        self.selected_genome["root_seed"] = "fedcba9876543210"
        self.selected_identity = "c" * 64
        shear["lineage"]["specimens"] = [{
            "genome_identity": self.selected_identity,
            "source_identity": shear["source"]["identity"],
            "genome": self.selected_genome,
            "derivation": {"kind": "mutation", "descendant_index": 65}
        }]
        shear_path.write_text(json.dumps(shear))
        self.originals[shear_path] = shear_path.read_bytes()
        self.curated_dir = self.root / "gallery/evolution/05-shearwing-telemetry/curated"
        self.curated_dir.mkdir(parents=True, exist_ok=True)
        self.genome_path = self.curated_dir / "shearwing-65.genome.json"
        self.genome_path.write_text(json.dumps(self.selected_genome))
        self.png_path = self.curated_dir / "shearwing-65.png"
        self.png_path.write_bytes(b"fake native PNG only for isolated path tests")
        patcher = mock.patch.object(module, "WINNER_PNG_SHA256",
                                    module.sha256(self.png_path.read_bytes()))
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_four_copies_preserve_every_field_except_path(self):
        module.create(self.root)
        output = self.root / "REVIEW-PROJECTS"
        records = json.loads((output / "provenance.json").read_text())["works"]
        self.assertEqual(len(records), 4)
        for label, project_rel, source_rel in module.WORKS:
            authored = json.loads((self.root / project_rel).read_text())
            portable = json.loads((output / f"{label}.fmproj").read_text())
            self.assertEqual(portable["source"]["path"], source_rel)
            portable["source"]["path"] = authored["source"]["path"]
            self.assertEqual(portable, authored)
        selected = json.loads((output / "04-shearwing-WINNER-65.fmproj").read_text())
        authored = json.loads((self.root / module.WORKS[3][1]).read_text())
        self.assertEqual(selected["genome"], self.selected_genome)
        self.assertEqual(selected["lineage"]["active_genome_identity"], self.selected_identity)
        self.assertEqual(selected["lineage"]["specimens"], authored["lineage"]["specimens"])
        self.assertEqual(selected["source"]["path"], module.WORKS[3][2])
        self.assertEqual(records[-1]["label"], "04-shearwing")
        self.assertEqual(json.loads((output / "provenance.json").read_text())
                         ["selected_review_project"]["descendant_index"], 65)
        self.assertTrue(all(path.read_bytes() == data for path, data in self.originals.items()))
        module.create(self.root)  # idempotent exact byte re-run

    def test_missing_source_rejected(self):
        (self.root / module.WORKS[0][2]).unlink()
        with self.assertRaisesRegex(ValueError, "required project/source missing"):
            module.create(self.root)

    def test_invalid_normalized_identity_rejected(self):
        path = self.root / module.WORKS[0][1]
        doc = json.loads(path.read_text())
        doc["source"]["identity"] = "not-a-hash"
        path.write_text(json.dumps(doc))
        with self.assertRaisesRegex(ValueError, "missing/invalid"):
            module.create(self.root)

    def test_duplicate_json_key_rejected(self):
        path = self.root / module.WORKS[0][1]
        path.write_text(path.read_text().replace('"project_version": 2,',
                        '"project_version": 2, "project_version": 2,', 1))
        with self.assertRaisesRegex(ValueError, "duplicate JSON field"):
            module.create(self.root)

    def test_unexpected_author_path_rejected(self):
        path = self.root / module.WORKS[0][1]
        doc = json.loads(path.read_text())
        doc["source"]["path"] = "gallery/unrelated/source.bmp"
        path.write_text(json.dumps(doc))
        with self.assertRaisesRegex(ValueError, "unexpected author source path"):
            module.create(self.root)

    def test_changed_existing_overlay_rejected(self):
        module.create(self.root)
        (self.root / "REVIEW-PROJECTS" / "01-angle.fmproj").write_text("tampered")
        with self.assertRaisesRegex(ValueError, "refusing to overwrite"):
            module.create(self.root)


    def test_wrong_curated_png_rejected(self):
        self.png_path.write_bytes(b"changed native pixels")
        with self.assertRaisesRegex(ValueError, "archived native PNG differs"):
            module.create(self.root)

    def test_missing_curated_genome_rejected(self):
        self.genome_path.unlink()
        with self.assertRaisesRegex(ValueError, "curated genome or native PNG missing"):
            module.create(self.root)

    def test_curated_genome_not_matching_lineage_rejected(self):
        doc = json.loads(self.genome_path.read_text())
        doc["root_seed"] = "0123456789abcdef"
        self.genome_path.write_text(json.dumps(doc))
        with self.assertRaisesRegex(ValueError, "curated genome, source or lineage identity mismatch"):
            module.create(self.root)

    def test_missing_or_duplicate_selected_lineage_rejected(self):
        path = self.root / module.WORKS[3][1]
        doc = json.loads(path.read_text())
        doc["lineage"]["specimens"].append(copy.deepcopy(doc["lineage"]["specimens"][0]))
        path.write_text(json.dumps(doc))
        with self.assertRaisesRegex(ValueError, "exactly one lineage descendant"):
            module.create(self.root)

    def test_modified_existing_winner_overlay_rejected(self):
        module.create(self.root)
        (self.root / "REVIEW-PROJECTS" / "04-shearwing-WINNER-65.fmproj").write_text("tampered")
        with self.assertRaisesRegex(ValueError, "refusing to overwrite"):
            module.create(self.root)


if __name__ == "__main__":
    unittest.main()
