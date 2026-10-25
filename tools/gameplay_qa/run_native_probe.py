#!/usr/bin/env python3
"""Build independent native QA without modifying root CMake or runtime sources."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir', type=Path, default=Path('build/gameplay-qa'))
parser.add_argument('--freeze', type=Path)
args = parser.parse_args()
OUT = ROOT / args.output_dir
OUT.mkdir(parents=True, exist_ok=True)
freeze_path = ROOT / args.freeze if args.freeze else None
freeze_bytes = freeze_path.read_bytes() if freeze_path else None
freeze = json.loads(freeze_bytes) if freeze_bytes else None
def verify_freeze():
    if not freeze:
        return
    changed = [name for name, expected in freeze['source_sha256'].items()
               if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != expected]
    if freeze_path.read_bytes() != freeze_bytes or changed:
        sys.exit(f'Portable source freeze mismatch: {changed or [str(freeze_path)]}')
verify_freeze()
if freeze_bytes:
    (OUT / 'portable-source-freeze.json').write_bytes(freeze_bytes)
sources = sorted((ROOT / 'native/human_resources_gameplay/src').glob('*.cpp'))
sources += sorted((ROOT / 'native/human_resources_progression/src').glob('*.cpp'))
sources += [ROOT / 'native/human_resources_storage/src/storage.cpp', Path(__file__).with_name('native_probe.cpp')]
required = {'run.cpp', 'director.cpp', 'capture.cpp', 'combat.cpp', 'upgrades.cpp'}
if not required.issubset({p.name for p in sources}):
    sys.exit('Gameplay implementation is still being written; rerun when all five owners exist.')
subprocess.run([sys.executable, str(ROOT / 'native/human_resources_gameplay/tools/generate_tuning.py'),
                '--root', str(ROOT), '--output', str(OUT / 'include/human_resources/gameplay/tuning.h')], check=True)
includes = ['native/human_resources_gameplay/include', 'native/human_resources_gameplay/src',
            'native/human_resources_progression/include', 'native/human_resources_storage/include',
            'native/tests']
inputs = list(sources)
for owner in ('human_resources_gameplay', 'human_resources_progression', 'human_resources_storage'):
    inputs += list((ROOT / 'native' / owner).rglob('*.h'))
inputs += [ROOT / 'native/human_resources_gameplay/runtime-spatial.json',
           ROOT / 'native/human_resources_gameplay/tools/generate_tuning.py',
           ROOT / 'design/balance/archero-ufo-system.json', ROOT / 'design/balance/endless-upgrade-system.json',
           ROOT / 'design/balance/endless-encounter-assumptions.json']
inputs.append(Path(__file__))
def hashes():
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
before = hashes()
command = [shutil.which('c++'), '-std=c++20', '-O2', '-fno-access-control']
command += ['-I' + str(ROOT / p) for p in includes]
command += ['-I' + str(OUT / 'include')]
command += [str(p) for p in sources] + ['-o', str(OUT / 'native-probe')]
subprocess.run(command, check=True)
result = subprocess.run([str(OUT / 'native-probe')], text=True, capture_output=True)
(OUT / 'native-probe.log').write_text(result.stdout + result.stderr)
print(result.stdout, end='')
after = hashes()
(OUT / 'source-hashes.json').write_text(json.dumps({'stable_during_build_and_run': before == after, 'sources': before}, indent=2) + '\n')
if before != after:
    sys.exit('Concurrent implementation changed during probe; rerun against stable sources.')
verify_freeze()
sys.exit(result.returncode)
