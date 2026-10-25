"""Review tools protect existing apps without requiring old local evidence."""
import importlib.util
import argparse
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("protected_apps", ROOT / "tools/playable_qa/protected_apps.py")
apps = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(apps)


def load_tool(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / f"tools/playable_qa/{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


profile = load_tool("profile")
with patch.dict(sys.modules, {"profile": profile, "protected_apps": apps}):
    visual = load_tool("visual_stress")
    with patch.dict(sys.modules, {"visual_stress": visual}):
        engagement = load_tool("engagement_proof")


class ProtectedAppContracts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="protected app review ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.roots = [self.root / "build/dev", self.root / "build/evidence"]
        self.output = self.root / "build/evidence/new-review"

    def bundle(self, path):
        executable = path / "Contents/MacOS/player"
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"opaque native executable")
        return path

    def protect(self, extra=(), manifest=None):
        return apps.ProtectedApps(self.roots, list(extra), self.output, manifest)

    def manifest(self, document):
        path = self.root / "canonical.json"
        path.write_text(json.dumps(document))
        return path

    def test_fresh_checkout_and_normal_build_need_no_old_previews_or_reports(self):
        (self.roots[0] / "assets").mkdir(parents=True)
        (self.roots[0] / "human_resources_player").write_bytes(b"normal build")
        protected = self.protect()
        self.assertEqual(protected.document(), {"schema": 1, "bundles": []})
        self.assertTrue(protected.verify()["unchanged"])
        canonical = self.manifest(protected.document())
        self.assertTrue(self.protect(manifest=canonical).verify()["unchanged"])

    def test_build_only_wrapper_records_protection_without_historical_evidence(self):
        args = argparse.Namespace(output=self.output, protected_app=[], protected_app_manifest=None, build_only=True)
        with patch.object(engagement, "ROOT", self.root), patch.object(visual, "build", return_value=(self.root / "private-tool", {"fixture_validation": "CPU-only pass"})) as build:
            result = engagement.run(args)
        build.assert_called_once()
        self.assertEqual(result["protected_app_count"], 0)
        self.assertTrue(result["protected_apps_unchanged"])
        report = json.loads((self.output / "qualified.json").read_text())
        self.assertTrue(report["protected_apps"]["unchanged"])
        self.assertIn("tools/playable_qa/engagement_proof.py", report["producer_before"])
        self.assertIn("tools/playable_qa/engagement_proof.cpp", report["producer_before"])

    def test_wrapper_retains_failed_protection_even_after_capture_writes_qualified(self):
        app = self.bundle(self.roots[0] / "Original.app")
        args = argparse.Namespace(output=self.output, protected_app=[], protected_app_manifest=None)

        def changed_capture(_, output):
            (output / "qualified.json").write_text(json.dumps({"source_stable": True}))
            (app / "Contents/MacOS/player").write_bytes(b"unexpected replacement")
            return {}

        with patch.object(engagement, "ROOT", self.root), patch.object(engagement, "capture", side_effect=changed_capture):
            with self.assertRaisesRegex(RuntimeError, "Protected apps"):
                engagement.run(args)
        self.assertFalse(json.loads((self.output / "qualified.json").read_text())["protected_apps"]["unchanged"])
        self.assertFalse(json.loads((self.output / "protection.json").read_text())["protected_apps"]["unchanged"])

    def test_wrapper_records_discovery_root_replaced_by_file(self):
        self.roots[0].mkdir(parents=True)
        args = argparse.Namespace(output=self.output, protected_app=[], protected_app_manifest=None)

        def replace_root(_, output):
            (output / "qualified.json").write_text(json.dumps({"source_stable": True}))
            self.roots[0].rmdir()
            self.roots[0].write_bytes(b"no longer a directory")
            return {}

        with patch.object(engagement, "ROOT", self.root), patch.object(engagement, "capture", side_effect=replace_root):
            with self.assertRaisesRegex(RuntimeError, "Protected apps"):
                engagement.run(args)
        for name in ("qualified.json", "protection.json"):
            report = json.loads((self.output / name).read_text())["protected_apps"]
            self.assertFalse(report["unchanged"])
            self.assertIn(str(self.roots[0]), report["errors"][0])

    def test_wrapper_records_missing_or_unreadable_final_producer(self):
        args = argparse.Namespace(output=self.output, protected_app=[], protected_app_manifest=None)
        real_digest = engagement.digest
        hashing_after = False

        def captured(_, output):
            nonlocal hashing_after
            (output / "qualified.json").write_text(json.dumps({"source_stable": True}))
            hashing_after = True
            return {}

        for error in (FileNotFoundError("producer removed"), PermissionError("producer unreadable")):
            with self.subTest(error=type(error).__name__):
                args.output = self.output / type(error).__name__
                hashing_after = False

                def failing_digest(path):
                    if hashing_after and path.name == "engagement_proof.cpp":
                        raise error
                    return real_digest(path)

                with patch.object(engagement, "ROOT", self.root), patch.object(engagement, "capture", side_effect=captured), patch.object(engagement, "digest", side_effect=failing_digest):
                    with self.assertRaisesRegex(RuntimeError, "review producers"):
                        engagement.run(args)
                for name in ("qualified.json", "protection.json"):
                    report = json.loads((args.output / name).read_text())
                    self.assertFalse(report["producers_stable"])
                    self.assertIsNone(report["producer_after"]["tools/playable_qa/engagement_proof.cpp"])
                    self.assertEqual(report["producer_errors"][0]["path"], str(engagement.SOURCE))
                    self.assertIn(str(error), report["producer_errors"][0]["error"])

    def test_all_discovered_and_explicit_bundles_are_protected(self):
        paths = [self.bundle(self.roots[0] / "Human Resources.app"),
                 self.bundle(self.roots[1] / "old review/Flight Preview.app"),
                 self.bundle(self.root / "outside/Other.app")]
        account = self.root / "save/account.bin"
        account.parent.mkdir()
        account.write_bytes(b"must remain untouched")
        protected = self.protect([paths[2]])
        self.assertEqual(len(protected.document()["bundles"]), 3)
        self.assertTrue(protected.verify()["unchanged"])
        (paths[1] / "Contents/MacOS/player").write_bytes(b"changed executable")
        self.assertFalse(protected.verify()["unchanged"])
        self.assertEqual(account.read_bytes(), b"must remain untouched")

    def test_added_deleted_and_symlink_changes_fail_the_complete_inventory(self):
        path = self.bundle(self.roots[0] / "Original.app")
        alias = path / "Contents/current"
        alias.symlink_to("MacOS/player")
        protected = self.protect()
        alias.unlink()
        alias.symlink_to("different-target")
        self.assertFalse(protected.verify()["unchanged"])
        alias.unlink()
        alias.symlink_to("MacOS/player")
        added = self.bundle(self.roots[1] / "New.app")
        self.assertFalse(protected.verify()["unchanged"])
        (added / "Contents/MacOS/player").unlink()
        self.assertFalse(protected.verify()["unchanged"])
        (path / "Contents/MacOS/player").unlink()
        self.assertTrue(protected.verify()["errors"])

    def test_unreadable_discovery_root_is_not_treated_as_no_apps(self):
        with patch.object(Path, "stat", side_effect=PermissionError("unreadable inventory root")):
            with self.assertRaises(PermissionError):
                apps.discover_apps(self.roots)

    def test_supplied_canonical_inventory_is_exact_and_rechecked(self):
        self.bundle(self.roots[0] / "Original.app")
        original = self.protect().document()
        canonical = self.manifest(original)
        self.assertTrue(self.protect(manifest=canonical).verify()["unchanged"])
        original["bundles"][0]["files"]["Contents/MacOS/player"] = "0" * 64
        canonical.write_text(json.dumps(original))
        with self.assertRaisesRegex(ValueError, "canonical manifest differs"):
            self.protect(manifest=canonical)
        canonical = self.manifest(self.protect().document())
        protected = self.protect(manifest=canonical)
        canonical.unlink()
        self.assertFalse(protected.verify()["unchanged"])
        with self.assertRaises(FileNotFoundError):
            self.protect(manifest=canonical)

    def test_canonical_cannot_omit_existing_bundles_or_reference_missing_apps(self):
        self.bundle(self.roots[0] / "Original.app")
        with self.assertRaisesRegex(ValueError, "complete discovered"):
            self.protect(manifest=self.manifest({"schema": 1, "bundles": []}))
        with self.assertRaisesRegex(ValueError, "existing ordinary"):
            self.protect([self.root / "Missing.app"])
        document = self.protect().document()
        document["bundles"][0]["path"] = str(self.root / "Missing.app")
        with self.assertRaisesRegex(ValueError, "existing ordinary"):
            self.protect(manifest=self.manifest(document))

    def test_unsafe_or_ambiguous_manifest_and_output_are_rejected(self):
        path = self.bundle(self.roots[0] / "Original.app")
        document = self.protect().document()
        for name, value in (("../outside", "0" * 64), ("/absolute", "0" * 64), ("Contents/MacOS/player", "not-a-digest")):
            invalid = json.loads(json.dumps(document))
            invalid["bundles"][0]["files"] = {name: value}
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.protect(manifest=self.manifest(invalid))
        document["bundles"].append(document["bundles"][0])
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            self.protect(manifest=self.manifest(document))
        self.output = path / "new-proof"
        with self.assertRaisesRegex(ValueError, "outside every protected"):
            self.protect()
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
