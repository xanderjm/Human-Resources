"""Shader edits must rebuild reflection consumers in the same incremental build."""

from pathlib import Path
import json
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ShaderBuildGraphTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja"), "requires CMake and Ninja")
    def test_shader_edit_rebuilds_unchanged_cpp_consumer(self):
        with tempfile.TemporaryDirectory(prefix="shader-build-graph-") as directory:
            source = Path(directory)
            (source / "shaders/src").mkdir(parents=True)
            (source / "tools/generators").mkdir(parents=True)
            shader = source / "shaders/src/ui.hlsl"
            shader.write_text("1")
            (source / "shaders/shader_manifest.json").write_text(json.dumps({"programs": [
                {"name": "ui", "stages": [{"stage": "fragment"}]}]}))
            (source / "tools/generators/source_inventory.py").touch()
            # Isolate dependency scheduling from SDKs and shader compiler availability.
            (source / "tools/generators/compile_shaders.py").write_text('''
import argparse
from pathlib import Path
p = argparse.ArgumentParser()
for name in ("source-root", "output-root", "include-output"):
    p.add_argument("--" + name, type=Path)
a, _ = p.parse_known_args()
a.include_output.mkdir(parents=True, exist_ok=True)
count = (a.source_root / "src/ui.hlsl").read_text()
(a.include_output / "shader_manifest.generated.h").write_text("constexpr int samplers = " + count + ";")
for name in ("package-manifest.json", "reflection/ui.fragment.json", "spirv/ui.fragment.spv"):
    output = a.output_root / name
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(count)
''')
            (source / "main.cpp").write_text('#include <shader_manifest.generated.h>\n#include <cstdio>\nint main() { std::printf("%d", samplers); }\n')
            (source / "CMakeLists.txt").write_text(f'''
cmake_minimum_required(VERSION 3.27)
project(ShaderGraph LANGUAGES CXX)
set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")
set(GAME_SHADERCROSS_EXECUTABLE "{Path(sys.executable).as_posix()}")
set(GAME_SHADER_PLATFORM linux)
set(GAME_GENERATED_ROOT "${{CMAKE_BINARY_DIR}}/generated")
set(GAME_GENERATED_SHADER_DIR "${{GAME_GENERATED_ROOT}}/assets/shaders")
set(GAME_GENERATED_SHADER_INCLUDE_DIR "${{GAME_GENERATED_ROOT}}/shaders/include")
include("{(ROOT / 'cmake/Shaders.cmake').as_posix()}")
game_add_shaders(shaders shader_files)
add_executable(consumer main.cpp)
add_dependencies(consumer shaders)
target_include_directories(consumer PRIVATE "${{GAME_GENERATED_SHADER_INCLUDE_DIR}}")
''')
            build = source / "build"

            def run(*args):
                result = subprocess.run(args, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result.stdout

            run("cmake", "-S", str(source), "-B", str(build), "-G", "Ninja")
            executable = build / ("consumer.exe" if sys.platform == "win32" else "consumer")
            run("cmake", "--build", str(build))
            self.assertEqual(run(str(executable)), "1")
            shader.write_text("2")
            run("cmake", "--build", str(build))
            self.assertEqual(run(str(executable)), "2", "new shader bindings must reach the binary on the first rebuild")
            (build / "generated/shaders/include/shader_manifest.generated.h").unlink()
            run("cmake", "--build", str(build))
            self.assertEqual(run(str(executable)), "2", "missing reflection header is regenerated with its consumers")


if __name__ == "__main__":
    unittest.main()
