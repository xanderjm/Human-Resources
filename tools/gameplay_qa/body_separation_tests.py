#!/usr/bin/env python3
"""Compile focused native body QA independently; no CMake/production mutation."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/gameplay-qa/biological-overlap"
OUT.mkdir(parents=True, exist_ok=True)
SOURCES = sorted((ROOT / "native/human_resources_gameplay/src").glob("*.cpp"))
if not (ROOT / "native/human_resources_gameplay/src/body_separation.cpp").exists():
    sys.exit("Production separation helper is not ready yet.")
HEADER = OUT / "include/human_resources/gameplay/tuning.h"
subprocess.run([sys.executable, str(ROOT / "native/human_resources_gameplay/tools/generate_tuning.py"),
                "--root", str(ROOT), "--output", str(HEADER)], check=True)
TEST = ROOT / "native/tests/body_separation_tests.cpp"
INPUTS = SOURCES + [TEST, Path(__file__), ROOT / "native/human_resources_gameplay/src/run_state.h",
    ROOT / "native/human_resources_gameplay/runtime-spatial.json", HEADER]
INPUTS += sorted((ROOT / "native/human_resources_gameplay/include").rglob("*.h"))

def hashes():
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in INPUTS}

before = hashes()
includes = ["native/human_resources_gameplay/include", "native/human_resources_gameplay/src", "native/tests"]
command = [shutil.which("c++"), "-std=c++20", "-O2", "-fno-access-control"]
command += ["-I" + str(OUT / "include")] + ["-I" + str(ROOT / p) for p in includes]
command += [str(p) for p in SOURCES + [TEST]] + ["-o", str(OUT / "body-separation-tests")]
subprocess.run(command, check=True)
result = subprocess.run([str(OUT / "body-separation-tests")], text=True, capture_output=True)
(OUT / "body-separation-tests.log").write_text(result.stdout + result.stderr)
print(result.stdout + result.stderr, end="")
after = hashes()
(OUT / "body-separation-test-hashes.json").write_text(json.dumps({"stable_during_build_and_run": before == after, "sources": before}, indent=2) + "\n")
if before != after:
    sys.exit("Concurrent source edits detected; rerun for stable evidence.")
sys.exit(result.returncode)
