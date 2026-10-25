"""Portable discovery and cold-export handoff, without rendering or publishing art."""
from pathlib import Path
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import art_tools
import bootstrap_art_tools as bootstrap


class ArtToolchainTests(unittest.TestCase):
    def test_requirements_and_discovery_share_the_same_version_contract(self):
        requirements = (ROOT / "tools/art-requirements.txt").read_text()
        for name, version in art_tools.PYTHON_PACKAGES.items():
            self.assertIn(name + "==" + version + " \\", requirements)

    def test_explicit_python_failure_never_uses_scoped_or_current_fallback(self):
        with patch.dict(os.environ, {"GAME_ART_PYTHON": "/missing-art-python"}), \
                patch.object(art_tools, "python_versions", side_effect=RuntimeError("wrong versions")) as probe:
            with self.assertRaisesRegex(RuntimeError, "wrong versions"):
                art_tools.art_python()
            self.assertEqual(probe.call_count, 1)

    def test_scoped_python_keeps_venv_symlink_path(self):
        with tempfile.TemporaryDirectory(prefix="art tool path ") as directory:
            root = Path(directory)
            with patch.dict(os.environ, {}, clear=True), patch.object(art_tools, "python_versions", return_value={}):
                self.assertEqual(art_tools.art_python(root), art_tools.scoped_python(root).absolute())

    def test_missing_wrong_and_explicit_blender_versions(self):
        with patch.dict(os.environ, {"GAME_BLENDER": "/explicit/blender"}), \
                patch.object(art_tools.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "Blender 4.4.0\n", "")) as probe:
            with self.assertRaisesRegex(RuntimeError, "Configured Blender"):
                art_tools.blender_executable()
            self.assertEqual(probe.call_count, 1)
        with patch.object(art_tools.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "warning\nBlender 4.5.15 LTS\n", "")):
            self.assertEqual(art_tools.blender_executable(Path("relative blender")), Path("relative blender").absolute())

    def test_host_archive_mapping_and_unsupported_host(self):
        contract = json.loads((ROOT / "tools/art-toolchain.json").read_text())
        for system, arch, ending in (("Windows", "AMD64", "windows-x64.zip"),
                                     ("Windows", "ARM64", "windows-arm64.zip"),
                                     ("Linux", "x86_64", "linux-x64.tar.xz"),
                                     ("Darwin", "arm64", "macos-arm64.dmg"),
                                     ("Darwin", "x86_64", "macos-x64.dmg")):
            name, digest = bootstrap.archive_for_host(contract, system, arch)
            self.assertTrue(name.endswith(ending))
            self.assertEqual(len(bytes.fromhex(digest)), 32)
        with self.assertRaisesRegex(RuntimeError, "No pinned Blender"):
            bootstrap.archive_for_host(contract, "Linux", "aarch64")

    def test_bad_download_is_deleted_before_extraction(self):
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(bootstrap.urllib.request, "urlopen", return_value=io.BytesIO(b"bad archive")):
            path = Path(directory) / "archive.zip"
            with self.assertRaisesRegex(RuntimeError, "SHA-256 mismatch"):
                bootstrap.checked_download("https://example.invalid/tool.zip", "0" * 64, path)
            self.assertFalse(path.exists())

    def test_cold_export_both_producers_use_explicit_blender_and_art_validator(self):
        # Run under the selected art Python, but intercept only the expensive
        # producer invocation. Both real cookers choose a genuinely absent,
        # isolated export root; no shared producer or source asset is touched.
        script = r'''
import json, pathlib, subprocess, sys, tempfile
from unittest.mock import patch
root = pathlib.Path(sys.argv[1]); sys.path.insert(0, str(root / 'tools'))
import cook_character_animation as legacy
import cook_infantry_animation as infantry
class ProducerSelected(Exception): pass
commands = []
def run(command, **kwargs):
    if '--version' in command:
        return subprocess.CompletedProcess(command, 0, 'Blender 4.5.15 LTS\n', '')
    commands.append(command)
    raise ProducerSelected()
with tempfile.TemporaryDirectory(prefix='art-cold-', dir=root / 'build') as directory:
    private = pathlib.Path(directory)
    legacy.EXPORTS = private / 'legacy/exports'
    config = json.loads((root / 'assets/animation/characters.json').read_text())
    with patch('subprocess.run', side_effect=run):
        for action in [lambda: legacy.ensure_exports(config, pathlib.Path('/explicit blender'), False),
                       lambda: infantry.ensure_exports(infantry.checked_recipe(), private / 'infantry', pathlib.Path('/explicit blender'))]:
            try: action()
            except ProducerSelected: pass
            else: raise AssertionError('cold producer was not selected')
    assert len(commands) == 2
    for command, producer in zip(commands, ['build_character_animation.py', 'build_infantry_animation.py']):
        assert command[0] == str(pathlib.Path('/explicit blender').absolute())
        assert str(root / 'tools' / producer) in command
        assert command[command.index('--validator-python') + 1] == sys.executable
        assert str(private) in command[command.index('--output-root') + 1]
    assert not list(private.rglob('*.glb'))
print('both cold producers selected explicit Blender and pinned art validator; no art exported')
'''
        result = subprocess.run([str(art_tools.art_python()), "-c", script, str(ROOT)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
