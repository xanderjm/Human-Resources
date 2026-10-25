"""Optional text packages retain transitive notices without source trees."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from game_support.packaging import stage_desktop_package


class RuntimeNoticeContracts(unittest.TestCase):
    def test_transitive_notices_pass_but_source_material_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, notice in (("SDL", "LICENSE.txt"), ("SDL_ttf", "external/freetype/LICENSE.TXT")):
                path = root / "external" / name / notice
                path.parent.mkdir(parents=True)
                path.write_text("runtime license\n")
            (root / "dependencies.lock.json").write_text(json.dumps({"dependencies": [
                {"name": name, "path": f"external/{name}"} for name in ("SDL", "SDL_ttf")]}))
            (root / "THIRD_PARTY_NOTICES.md").write_text("runtime notices\n")
            player = root / "build/release/sample_player"
            for kind in ("shaders", "runtime"):
                assets = player.parent / "assets" / kind
                assets.mkdir(parents=True)
                (assets / "package-manifest.json").write_text(json.dumps({
                    "schema_version": 1, "kind": kind, "files": []}))
            player.write_bytes(b"test executable")
            project = {"foundation_version": "test", "features": ["dynamic-text"],
                       "identity": {"slug": "sample", "version": "0.1.0"}}
            stage = stage_desktop_package(root, project, "macos", player)
            self.assertTrue((stage / "licenses/SDL_ttf/external__freetype__LICENSE.TXT").is_file())
            command = [sys.executable, "-B", str(ROOT / "tools/generators/audit_runtime_package.py"),
                       "--package-root", str(stage), "--require-manifest"]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            (stage / "licenses/source.cpp").write_text("not a license")
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("forbidden material", result.stdout)


if __name__ == "__main__":
    unittest.main()
