#!/usr/bin/env python3
"""Build and record the real product input/camera regression, without GPU access."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "build/integration-qa"
SOURCES = (
    "native/tests/product_projection_tests.cpp",
    "native/human_resources_product/src/controls.cpp",
    "native/human_resources_product/src/scene.cpp",
    "native/human_resources_presentation/src/style_scene.cpp",
    "native/human_resources_gameplay/src/run.cpp",
    "native/human_resources_gameplay/runtime-spatial.json",
)


def hashes() -> dict[str, str]:
    return {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in SOURCES}


def main() -> int:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    before = hashes()
    build = subprocess.run(
        ["cmake", "--build", "build/dev", "--target", "human_resources_product_projection_tests"],
        cwd=ROOT, text=True, capture_output=True, check=False,
    )
    (OUTPUT / "projection-build.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        print(build.stdout + build.stderr)
        return build.returncode
    executable = ROOT / "build/dev/human_resources_product_projection_tests"
    run = subprocess.run([str(executable)], cwd=ROOT, text=True, capture_output=True, check=False)
    (OUTPUT / "projection.log").write_text(run.stdout + run.stderr)
    stable = before == hashes()
    report = {
        "scope": "macOS host SDL-free public Controls -> Game -> product scene -> actual camera projection",
        "exit_code": run.returncode,
        "source_stable": stable,
        "source_sha256": before,
        "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "claims": [
            "WASD/arrow semantic directions match projected ship screen directions",
            "upward floating-stick drag and controller axis move ship upward",
            "ahead actors appear above ship and forward locomotion faces correct direction",
            "beam, warning, laser and projectile planar geometry match actor/ship positions",
            "full saucer lateral footprint remains visible at 32 portrait movement/station extremes",
        ],
        "limits": "No rendered/native input, physical-device, performance or visual-quality qualification.",
    }
    (OUTPUT / "projection.json").write_text(json.dumps(report, indent=2) + "\n")
    print(run.stdout + run.stderr, end="")
    print(f"exit={run.returncode} source_stable={stable}")
    return run.returncode or (0 if stable else 1)


if __name__ == "__main__":
    sys.exit(main())
