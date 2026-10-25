#!/usr/bin/env python3
"""Read-only actual Android fixture proof against current portable authority.

All mutations use an isolated in-memory storage adapter and ignored output
copies. Never installs a fixture, runs a GPU client or invokes a shared build.
"""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/vehicle-assembly-audit/authority3-account-compatibility"
FIXTURES = ROOT / "build/evidence/mobile-release/authority2-android-ui"
EXPECTED = {
    "earned-budget-bank-terminal": "1f05aee307d1ef805403ed3c41557d869a04e2761febfaec76ccf2b597f6faae",
    "cold-wallet-rank": "81ab922a552a5d5f05c89e9ea4b73aef47954001826856a87c08fe1ebfe598f9",
    "before-purchase": "12c6e61b2d23bcd42293c42d4efdb708db52e039069ff86234210fd7ed53b35f",
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    copies = {}
    for stem, digest in EXPECTED.items():
        source = FIXTURES / (stem + ".bin")
        metadata = json.loads(source.with_suffix(".json").read_text())
        if sha(source) != digest or metadata["sha256"] != digest:
            raise ValueError("Actual Android evidence changed: " + stem)
        target = OUT / (stem + ".input.bin")
        target.write_bytes(source.read_bytes())
        copies[stem] = target
    if "--prepare-only" in sys.argv:
        print("Historical Android copies verified; no compilation or mutations performed.")
        return
    generated = OUT / "generated/human_resources/gameplay/tuning.h"
    subprocess.run([sys.executable, str(ROOT / "native/human_resources_gameplay/tools/generate_tuning.py"),
                    "--root", str(ROOT), "--output", str(generated)], check=True)
    owners = [ROOT / "native" / name for name in
              ("human_resources_gameplay", "human_resources_progression", "human_resources_storage")]
    sources = [p for owner in owners for p in sorted((owner / "src").glob("*.cpp"))]
    driver = ROOT / "tools/playable_qa/account_authority_upgrade.cpp"
    inputs = sources + [p for owner in owners for p in sorted(owner.rglob("*.h"))]
    inputs += [driver, Path(__file__), generated, ROOT / "native/tests/progression_test_backend.h",
               ROOT / "native/human_resources_gameplay/runtime-spatial.json",
               ROOT / "native/human_resources_gameplay/tools/generate_tuning.py",
               ROOT / "assets/production/vehicle-weapons.json"]
    inputs += [ROOT / "design/balance" / name for name in
               ("archero-ufo-system.json", "endless-upgrade-system.json", "endless-encounter-assumptions.json")]
    inputs += [FIXTURES / (stem + extension) for stem in EXPECTED for extension in (".bin", ".json")]

    def hashes():
        return {str(p.relative_to(ROOT)): sha(p) for p in inputs}

    before = hashes()
    results = {}
    for label, flags in (("optimized", ["-O2"]),
                         ("sanitizers", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])):
        binary = OUT / ("account-upgrade-" + label)
        command = [shutil.which("c++"), "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror", *flags]
        command += ["-I" + str(owner / "include") for owner in owners]
        command += ["-I" + str(OUT / "generated"), "-I" + str(ROOT / "native/tests")]
        command += [str(p) for p in sources + [driver]] + ["-o", str(binary)]
        compiled = subprocess.run(command, capture_output=True, text=True)
        (OUT / (label + "-build.log")).write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(compiled.stderr)
        cases = {}
        for stem, mode in (("earned-budget-bank-terminal", "continuation"), ("cold-wallet-rank", "settled")):
            prefix = OUT / (label + "-" + stem)
            run = subprocess.run([str(binary), str(copies[stem]), str(prefix), mode,
                                  str(copies["before-purchase"])], capture_output=True, text=True)
            prefix.with_suffix(".stdout").write_text(run.stdout)
            prefix.with_suffix(".stderr").write_text(run.stderr)
            if run.returncode:
                raise RuntimeError(stem + ": " + run.stderr)
            cases[stem] = json.loads(run.stdout)
        results[label] = {"command": command, "binary_sha256": sha(binary), "cases": cases}
        print(label, "actual positive pending and settled Android fixture cases PASS")
    after = hashes()
    original_unchanged = all(sha(FIXTURES / (stem + ".bin")) == digest for stem, digest in EXPECTED.items())
    outputs = {str(p.relative_to(ROOT)): sha(p) for p in OUT.glob("*.bin")}
    evidence = {"source_stable": before == after, "original_fixtures_unchanged": original_unchanged,
                "original_fixture_sha256": EXPECTED, "sources": before, "after_sources": after,
                "checks": results, "output_hashes": outputs,
                "scope": "Actual Android authority2 bytes validated against current portable authority3. Storage writes use isolated in-memory backend only; not a new Android/device run."}
    (OUT / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    if before != after or not original_unchanged:
        raise RuntimeError("Concurrent source/fixture changes invalidate this proof")
    print("Source stable; originals unchanged; exact backup and one-time historical 80CR bank verified.")


if __name__ == "__main__":
    main()
