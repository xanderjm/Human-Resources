"""Cooked files have producers, including after incremental output deletion."""

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja"), "requires CMake and Ninja")
class RuntimeBuildGraphTests(unittest.TestCase):
    def test_file_consumer_and_deleted_output_rebuild_in_one_invocation(self):
        with tempfile.TemporaryDirectory(prefix="runtime build graph ") as directory:
            source = Path(directory)
            # Execute the generated product's actual CMake graph and cooker.
            # An interface SDL target permits configuration without building a GPU client.
            for name in ("cmake", "native", "tools/generators", "tools/style-preview/shaders", "shaders"):
                shutil.copytree(ROOT / name, source / name)
            for name in ("tools/cook_product_content.py", "tools/cook_style_preview.py", "tools/normalize_glb.py",
                         "tools/cook_infantry_animation.py", "tools/build_infantry_animation.py",
                         "tools/validate_infantry_animation.py", "tools/cook_vehicle_content.py",
                         "tools/glb_geometry_repair.py", "tools/audit_glb.py", "tools/art_tools.py",
                         "tools/art-toolchain.json", "tools/art-requirements.txt"):
                shutil.copyfile(ROOT / name, source / name)
            (source / "assets/production").mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / "assets/production/inventory.json", source / "assets/production/inventory.json")
            for name in ("vehicle-weapons.json", "assemblies.json"):
                shutil.copyfile(ROOT / "assets/production" / name, source / "assets/production" / name)
            shutil.copytree(ROOT / "design/balance", source / "design/balance")
            # This consumed geometry lives under graphics, outside models/*.glb.
            # Keep the exact product dependency in the isolated producer graph.
            shield = source / "assets/graphics/fx-shield-shell/unit-sphere.glb"
            shield.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / "assets/graphics/fx-shield-shell/unit-sphere.glb", shield)
            for name in ("CMakeLists.txt", "project.json"):
                shutil.copyfile(ROOT / name, source / name)
            for dependency, target in (("SDL", "SDL3::SDL3"), ("SDL_ttf", "SDL3_ttf::SDL3_ttf"),
                                       ("flatbuffers", "FlatBuffers::FlatBuffers")):
                path = source / "external" / dependency
                path.mkdir(parents=True)
                (path / "CMakeLists.txt").write_text(f"add_library({target} INTERFACE IMPORTED GLOBAL)\n")
            project = json.loads((source / "project.json").read_text())
            authored = source / "assets/runtime/images/probe.bin"
            authored.parent.mkdir(parents=True)
            authored.write_bytes(b"first")
            with (source / "CMakeLists.txt").open("a") as cmake:
                cmake.write('''
add_custom_target(runtime_file_consumer DEPENDS "${GAME_GENERATED_RUNTIME_DIR}/images/probe.bin")
''')
                cmake.write(f'add_dependencies(runtime_file_consumer {project["identity"]["slug"]}_runtime_assets)\n')
            build = source / "build"

            def run(*args):
                result = subprocess.run(args, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            run("cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                "-DGAME_SHADER_PLATFORM=linux", f"-DGAME_SHADERCROSS_EXECUTABLE={Path(sys.executable).as_posix()}",
                f"-DGAME_FLATC_EXECUTABLE={Path(sys.executable).as_posix()}")
            cooked = build / "generated/assets/runtime/images/probe.bin"
            for payload in (b"first", b"second"):
                authored.write_bytes(payload)
                run("cmake", "--build", str(build), "--target", "runtime_file_consumer")
                self.assertEqual(cooked.read_bytes(), payload)
            manifest = build / "generated/assets/runtime/package-manifest.json"
            previous_mtime = manifest.stat().st_mtime_ns
            # The fixture deliberately has no product recipe: exercise the real
            # producer edge without importing art tools or editing the real GLB.
            shield.write_bytes(shield.read_bytes() + b"dependency probe")
            run("cmake", "--build", str(build), "--target", "runtime_file_consumer")
            self.assertGreater(manifest.stat().st_mtime_ns, previous_mtime,
                               "a changed consumed graphics GLB must recook on the first rebuild")
            cooked.unlink()
            run("cmake", "--build", str(build), "--target", "runtime_file_consumer")
            self.assertEqual(cooked.read_bytes(), b"second", "missing cooked output must be regenerated on the first rebuild")


if __name__ == "__main__":
    unittest.main()
