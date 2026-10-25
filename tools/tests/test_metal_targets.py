"""Simulator and device shader libraries must use their own Metal targets."""

from pathlib import Path
import argparse
import json
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "generators"))
import compile_shaders


class MetalTargetTests(unittest.TestCase):
    def test_sdk_and_target_survive_direct_and_fallback_compilation(self):
        for sdk, target in (("iphoneos", "air64-apple-ios17.0"),
                            ("iphonesimulator", "air64-apple-ios17.0-simulator"),
                            ("macosx", "air64-apple-macos14.0")):
            for fallback in (False, True):
                with self.subTest(sdk=sdk, fallback=fallback), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    output = root / "shader.metallib"
                    output.with_suffix(".air").touch()
                    commands = []

                    def attempt(command):
                        commands.append(command)
                        return subprocess.CompletedProcess(command, 1 if fallback else 0, "", "")

                    with patch.object(compile_shaders.shutil, "which", return_value="/usr/bin/xcrun"), \
                            patch.object(compile_shaders, "try_run", side_effect=attempt), \
                            patch.object(compile_shaders, "mounted_metal_compiler", return_value=Path("/tool/metal")), \
                            patch.object(compile_shaders.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "/sdk\n")), \
                            patch.object(compile_shaders, "run", side_effect=commands.append):
                        compile_shaders.compile_metallib(root / "shader.metal", output, sdk, "17.0", root / "cache")

                    compilations = [command for command in commands if "-c" in command]
                    self.assertEqual(len(compilations), 2 if fallback else 1)
                    for command in compilations:
                        self.assertEqual(command[command.index("-target") + 1], target)
                    self.assertIn(sdk, compilations[0])
                    self.assertFalse(output.with_suffix(".air").exists())

    def test_ios_sdk_selection_reaches_the_compiler_and_keeps_package_identity(self):
        for sdk in ("iphoneos", "iphonesimulator"):
            with self.subTest(sdk=sdk), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / "source"
                source.mkdir()
                (source / "shader.hlsl").write_text("source")
                (source / "shader_manifest.json").write_text(json.dumps({"programs": [{
                    "name": "ui", "source": "shader.hlsl", "stages": [{"stage": "fragment", "entrypoint": "main"}]
                }]}))
                args = argparse.Namespace(source_root=source, output_root=root / "out", include_output=root / "include",
                    shadercross=Path("unused"), platform="ios", apple_ios_sdk=sdk, apple_deployment_target="18.2")
                calls = []

                def cross(_exe, _source, _includes, _stage, _entry, destination, output):
                    output.parent.mkdir(parents=True, exist_ok=True)
                    output.write_text("{}" if destination == "JSON" else "metal source")

                def metal(msl, output, selected_sdk, deployment, cache):
                    calls.append((selected_sdk, deployment))
                    output.parent.mkdir(parents=True, exist_ok=True)
                    output.write_bytes(b"compiled metal")

                with patch.object(compile_shaders, "arguments", return_value=args), \
                        patch.object(compile_shaders, "shadercross", side_effect=cross), \
                        patch.object(compile_shaders, "compile_metallib", side_effect=metal):
                    self.assertEqual(compile_shaders.main(), 0)
                self.assertEqual(calls, [(sdk, "18.2")])
                manifest = json.loads((args.output_root / "package-manifest.json").read_text())
                self.assertEqual(manifest["artifacts"][0]["backend"], "metal-ios")
                self.assertEqual(manifest["artifacts"][0]["path"], "metal/ios/ui.fragment.metallib")


if __name__ == "__main__":
    unittest.main()
