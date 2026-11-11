"""Regression tests for package-only, provenance-preserving gallery path overlays."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

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


if __name__ == "__main__":
    unittest.main()
