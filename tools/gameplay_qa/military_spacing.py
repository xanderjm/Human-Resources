#!/usr/bin/env python3
"""Bounded public-Game military spacing trace and consumed-body measurement.

No injected health/positions/private state, GPU, app accounts, or shared build.
Compile optimized portable sources into a private evidence directory. Pair counts
use existing abstract player-contact envelopes so historical results can compare;
rigid ground-body envelopes are independently validated below. The neutral soldier
contact envelope intentionally does not claim to enclose all animated extremities.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def vertices(path):
    data = path.read_bytes()
    magic, count = struct.unpack_from('<4sI', data)
    if magic != b'HRM1':
        raise ValueError(f'Unsupported consumed mesh {path}')
    return [struct.unpack_from('<3f', data, 20 + 56 * i) for i in range(count)]


def measure(content):
    recipe = json.loads((ROOT / 'assets/production/assemblies.json').read_text())['assemblies']
    jeep = next(a for a in recipe if a['id'] == 'jeep-four-wheels')
    tank = next(a for a in recipe if a['id'] == 'tank-with-turret')
    paths = [content / (n + '.hrm') for n in ('jeep-chassis', 'jeep-wheel', 'tank-hull', 'tank-turret')]
    chassis, wheel, hull, turret = [vertices(p) for p in paths]
    # Chassis translation is only vertical. Wheel spin is bounded analytically:
    # a vertex's yz radius sweeps around the local X axis, then wheel socket yaw.
    chassis_radius = max(math.hypot(p[0], p[2]) for p in chassis)
    wheel_radius = max(math.hypot(abs(s['translation_m'][0]) + abs(p[0]),
        abs(s['translation_m'][2]) + math.hypot(p[1], p[2])) for s in jeep['sockets'] for p in wheel)
    offset = tank['sockets'][0]['translation_m']
    tank_radius = max(max(math.hypot(p[0], p[2]) for p in hull),
        math.hypot(offset[0], offset[2]) + max(math.hypot(p[0], p[2]) for p in turret))
    configured = json.loads((ROOT / 'native/human_resources_gameplay/runtime-spatial.json').read_text())['military_spacing']['body_radius']
    actual = [max(chassis_radius, wheel_radius), tank_radius, tank_radius * 1.6]
    if any(a > b for a, b in zip(actual, configured[1:])):
        raise ValueError(f'Configured military spacing excludes consumed rigid ground body: {actual} > {configured[1:]}')
    return {'scope': 'Ground chassis/wheel spin and hull/turret full-yaw envelope; not transient capture transforms or animated infantry extremities',
        'measured_vehicle_radii': actual, 'configured_radii': configured,
        'inputs': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--content', type=Path, default=ROOT / 'build/dev/assets/runtime/game')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/evidence/military-spacing/reproduced')
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'consumed-body-measurement.json').write_text(json.dumps(measure(args.content), indent=2) + '\n')
    header = args.output / 'include/human_resources/gameplay/tuning.h'
    subprocess.run([sys.executable, str(ROOT / 'native/human_resources_gameplay/tools/generate_tuning.py'), '--root', str(ROOT), '--output', str(header)], check=True)
    sources = sorted((ROOT / 'native/human_resources_gameplay/src').glob('*.cpp'))
    inputs = sources + list((ROOT / 'native/human_resources_gameplay/include').rglob('*.h')) + [ROOT / 'native/human_resources_gameplay/src/run_state.h', ROOT / 'native/human_resources_gameplay/runtime-spatial.json', Path(__file__), Path(__file__).with_suffix('.cpp'), header]
    hashes = lambda: {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    before = hashes()
    command = [shutil.which('c++'), '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'native/human_resources_gameplay/include'), '-I' + str(args.output / 'include'), str(Path(__file__).with_suffix('.cpp'))]
    binary = args.output / 'public-trace'
    subprocess.run(command + [str(p) for p in sources] + ['-o', str(binary)], check=True)
    subprocess.run([str(binary), str(args.output / 'trace')], check=True)
    after = hashes()
    report = {'stable_inputs': before == after, 'source_inputs': before, 'source_reference': 'https://www.red3d.com/cwr/steer/gdc99/', 'policies': 'Fresh public Game, seeds1/514/7841, idle and nearest-harvest pursuit with actual earned choices/Dash; synthetic policies do not qualify human fairness.'}
    (args.output / 'source-binding.json').write_text(json.dumps(report, indent=2) + '\n')
    if before != after:
        sys.exit('Source changed during trace; do not reuse result as coherent evidence.')
    print((args.output / 'trace-summary.csv').read_text(), end='')


if __name__ == '__main__':
    main()
