"""Exercise failed cooking/publication and preserve the prior usable outputs."""

import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools/generators"))
from cook_runtime_assets import cook
from game_support.packaging import publish_desktop_package, stage_desktop_package
from stage_runtime_assets import mirror_tree


class AssetFailureContracts(unittest.TestCase):
    def test_removed_sources_disappear_and_failed_cook_preserves_previous(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "source", root / "output"
            source.mkdir()
            (source / "removed.bin").write_bytes(b"old")
            project = {"identity": {"slug": "sample"}, "foundation_version": "test"}
            cook(source, output, project)
            (source / "removed.bin").unlink()
            (source / "current.bin").write_bytes(b"current")
            cook(source, output, project)
            self.assertFalse((output / "removed.bin").exists())
            previous_manifest = (output / "package-manifest.json").read_bytes()
            with patch("cook_runtime_assets.shutil.copyfile", side_effect=OSError("disk full")):
                with self.assertRaises(OSError):
                    cook(source, output, project)
            self.assertEqual((output / "package-manifest.json").read_bytes(), previous_manifest)
            self.assertEqual((output / "current.bin").read_bytes(), b"current")

    def test_source_mutation_during_copy_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "source", root / "output"
            source.mkdir()
            (source / "current.bin").write_bytes(b"original")
            project = {"identity": {}, "foundation_version": "test"}
            cook(source, output, project)
            original_copy = shutil.copyfile

            def mutate(src, dst):
                Path(src).write_bytes(b"changed-during-copy")
                return original_copy(src, dst)

            with patch("cook_runtime_assets.shutil.copyfile", side_effect=mutate):
                with self.assertRaises(RuntimeError):
                    cook(source, output, project)
            self.assertEqual((output / "current.bin").read_bytes(), b"original")

    def test_replacing_existing_candidate_cannot_delete_previous_package(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            previous = root / "build/packages/macos/sample-1.0.0"
            previous.mkdir(parents=True)
            (previous / "sentinel").write_bytes(b"keep")
            with self.assertRaises(RuntimeError):
                stage_desktop_package(root, {"identity": {"slug": "sample", "version": "1.0.0"}},
                                      "macos", root / "missing")
            self.assertEqual((previous / "sentinel").read_bytes(), b"keep")

    def test_partial_package_publication_restores_both_previous_artifacts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            transaction = root / "transaction"
            candidate = transaction / "sample"
            candidate.mkdir(parents=True)
            (candidate / "app").write_bytes(b"new")
            archive = transaction / "sample.zip"
            archive.write_bytes(b"new archive")
            previous = root / "sample"
            previous.mkdir()
            (previous / "app").write_bytes(b"old")
            previous_archive = root / "sample.zip"
            previous_archive.write_bytes(b"old archive")
            original_replace = Path.replace

            def fail_archive(path, target):
                if path == archive:
                    raise OSError("publication interrupted")
                return original_replace(path, target)

            with patch.object(Path, "replace", fail_archive):
                with self.assertRaises(OSError):
                    publish_desktop_package(candidate, archive, previous, previous_archive)
            self.assertEqual((previous / "app").read_bytes(), b"old")
            self.assertEqual(previous_archive.read_bytes(), b"old archive")

    def test_failed_staging_install_restores_previous_tree(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "source", root / "output"
            source.mkdir()
            output.mkdir()
            (source / "new").write_bytes(b"new")
            (output / "old").write_bytes(b"old")
            original_replace = Path.replace

            def fail_install(path, target):
                if path.name == "next":
                    raise OSError("install interrupted")
                return original_replace(path, target)

            with patch.object(Path, "replace", fail_install):
                with self.assertRaises(OSError):
                    mirror_tree(source, output)
            self.assertEqual((output / "old").read_bytes(), b"old")


if __name__ == "__main__":
    unittest.main()
