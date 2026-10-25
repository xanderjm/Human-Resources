"""Cook isolated equipped infantry through the existing bounded HRA1 encoder.

This entry point never regenerates or publishes the six legacy character exports.
It snapshots validated infantry inputs, adapts report vocabulary in that private
snapshot, and uses the same HRA/HRM encoder and recoverable publication contract.
Technical cooking does not grant animation-art or shipping approval.
"""
from __future__ import annotations

import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

import numpy as np

from art_tools import blender_executable
import cook_character_animation as encoder
from animation.runtime_reference import world_at
from normalize_glb import matrix, unpack
from transaction_character_animation import locked_output, publish, staging

ROOT = Path(__file__).resolve().parents[1]
RECIPE = ROOT / 'assets/animation/infantry.json'
ROLE_IDS = ('rifleman', 'shotgunner', 'rocketeer')
REQUIRED_CLIPS = frozenset((
    'idle', 'walk', 'run', 'alert', 'scan', 'startle', 'hit_react',
    'beam_lift', 'beam_struggle', 'beam_release', 'aim_enter', 'aim', 'fire', 'aim_exit',
))


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_path(name):
    path = ROOT / name
    if Path(name).is_absolute() or '..' in Path(name).parts or not path.resolve().is_relative_to(ROOT):
        raise ValueError('Infantry source must remain inside the repository')
    return path


def checked_recipe():
    recipe = json.loads(RECIPE.read_text())
    variants = recipe['variants']
    if recipe['schema_version'] != 1 or recipe['fps'] != 30:
        raise ValueError('Unsupported infantry recipe')
    if len(variants) != 3 or {v['role'] for v in variants} != set(ROLE_IDS):
        raise ValueError('Expected three distinct infantry roles')
    if {v['id'] for v in variants} != {'infantry-' + role + '-body' for role in ROLE_IDS}:
        raise ValueError('Infantry role/model identity mismatch')
    if set(recipe['clips']) != REQUIRED_CLIPS:
        raise ValueError('Infantry clip contract mismatch')
    if digest(source_path(recipe['fit_source'])) != recipe['fit_sha256']:
        raise ValueError('Infantry fit recipe changed')
    return recipe


def validated_inputs(recipe, export_root):
    """Hash the exact read closure before/after snapshot; reject stale producers."""
    report_path = export_root / 'reports/infantry-validation.json'
    validation = json.loads(report_path.read_text())
    if validation['status'] != 'passed':
        raise ValueError('Infantry technical validation has not passed')
    checked = {r['id']: r for r in validation['characters']}
    if len(validation['characters']) != 3 or set(checked) != {v['id'] for v in recipe['variants']}:
        raise ValueError('Infantry validation must cover each role exactly once')
    manifest_path = export_root / 'producer-manifest.json'
    manifest = json.loads(manifest_path.read_text())
    published = {r['id']: r for r in manifest['variants']}
    if manifest['schema'] != 1 or manifest['status'] != 'validated-frozen-authoring-set' or set(published) != set(checked):
        raise ValueError('Infantry producer manifest is not a complete validated set')
    paths = {RECIPE, source_path(recipe['fit_source']), report_path, manifest_path,
             ROOT / 'assets/production/weapon-sockets.json',
             ROOT / 'tools/validate_infantry_animation.py'}
    if validation['validator_sha256'] != digest(ROOT / 'tools/validate_infantry_animation.py'):
        raise ValueError('Infantry validator changed')
    if validation['fit_sha256'] != recipe['fit_sha256']:
        raise ValueError('Infantry validation fit mismatch')
    if validation['weapon_sockets_sha256'] != digest(ROOT / 'assets/production/weapon-sockets.json'):
        raise ValueError('Infantry weapon measurements changed')
    reports = {}
    for variant in recipe['variants']:
        identity = variant['id']
        path = export_root / 'exports' / (identity + '.glb')
        report_file = export_root / 'reports' / (identity + '.json')
        report = json.loads(report_file.read_text())
        if (published[identity]['body_export_sha256'] != digest(path) or
                published[identity]['report_sha256'] != digest(report_file)):
            raise ValueError('Stale infantry producer manifest: ' + identity)
        result = checked.get(identity, {})
        if report['id'] != identity or report['config'] != variant:
            raise ValueError('Stale infantry authoring configuration: ' + identity)
        if result.get('status') != 'passed' or result.get('sha256') != digest(path):
            raise ValueError('Stale infantry validation/export: ' + identity)
        if {c['name'] for c in report['clips']} != REQUIRED_CLIPS or len(report['clips']) != 14:
            raise ValueError('Missing or duplicate infantry clips: ' + identity)
        if not report['authoring_inputs']:
            raise ValueError('Missing infantry authoring closure: ' + identity)
        for prefix in ('body', 'weapon'):
            name = report[prefix + '_source']
            if name != variant[prefix + '_source'] or digest(source_path(name)) != report[prefix + '_source_sha256']:
                raise ValueError('Infantry source mismatch: ' + identity)
            paths.add(source_path(name))
        for name, expected in report['authoring_inputs'].items():
            path_input = source_path(name)
            if digest(path_input) != expected:
                raise ValueError('Stale infantry authoring input: ' + name)
            paths.add(path_input)
        paths.update((path, report_file))
        reports[identity] = report
    return reports, {path: digest(path) for path in sorted(paths)}


def ensure_exports(recipe, export_root, blender):
    # Probe under the shared lock, then release it before invoking the producer,
    # which takes that same lock itself. The cook revalidates under its read lock.
    try:
        with locked_output(export_root):
            validated_inputs(recipe, export_root)
        return
    except (FileNotFoundError, KeyError, ValueError):
        blender = blender_executable(blender)
    subprocess.run([str(blender.resolve()), '--background', '--factory-startup', '--python-exit-code', '1',
                    '--python', str(ROOT / 'tools/build_infantry_animation.py'), '--',
                    '--output-root', str(export_root), '--validator-python', sys.executable], cwd=ROOT, check=True)


def socket_metadata(path, variant):
    """Actor-local raw-weapon delta and muzzle frames; all matrices row-major."""
    doc, chunks = unpack(path)
    blob = next(value for kind, value in chunks if kind == 0x004e4942)
    order, parents = encoder.hierarchy(doc)
    names = {node['name']: i for i, node in enumerate(doc['nodes'])}
    if len(names) != len(doc['nodes']):
        raise ValueError('Duplicate infantry node names')
    rest = [None] * len(order)
    for index in order:
        local = matrix(doc['nodes'][index])
        rest[index] = local if parents[index] < 0 else rest[parents[index]] @ local
    weapon = names['socket_weapon']
    if not np.allclose(rest[weapon], np.eye(4), atol=3e-5, rtol=0):
        raise ValueError('Raw weapon rest frame changed')
    aim = next(a for a in doc['animations'] if a['name'] == 'aim')
    world = world_at(doc, blob, aim, 0.0)
    delta = world[weapon] @ np.linalg.inv(rest[weapon])
    forward = delta[:3, :3] @ np.array([0., 0., 1.])
    forward /= np.linalg.norm(forward)
    markers = {}
    for name, position in variant['weapon_sockets'].items():
        if parents[names[name]] != weapon:
            raise ValueError('Weapon marker does not follow socket_weapon: ' + name)
        expected = delta @ np.r_[position, 1.]
        if np.linalg.norm(world[names[name]][:3, 3] - expected[:3]) >= 3e-5:
            raise ValueError('Weapon marker differs from raw source datum: ' + name)
        markers[name] = {'raw_local_m': position, 'aim_origin_m': expected[:3].tolist()}
    return {'id': variant['id'], 'role': variant['role'], 'weapon_id': variant['weapon_id'],
            'weapon_socket': 'socket_weapon', 'head_socket': 'socket_head',
            'aim_weapon_delta_row_major': delta.tolist(), 'aim_forward': forward.tolist(),
            'aim_yaw_radians': float(np.arctan2(forward[0], forward[2])),
            'aim_elevation_radians': float(np.arctan2(forward[1], np.hypot(forward[0], forward[2]))),
            'chest_pivot_m': world[names['chest']][:3, 3].tolist(), 'markers': markers}


def write_socket_reference(path, variant, clips, output):
    """HWS1 is test-only full socket playback, never a shipping content file."""
    doc, chunks = unpack(path)
    blob = next(value for kind, value in chunks if kind == 0x004e4942)
    names = ['socket_head', 'chest', 'socket_weapon', 'socket_palm.L', 'socket_palm.R',
             *variant['weapon_sockets']]
    indices = [next(i for i, node in enumerate(doc['nodes']) if node['name'] == name) for name in names]
    records = {clip['name']: clip for clip in clips}
    samples = sum(2 * records[a['name']]['frames'] - 1 for a in doc['animations'])
    payload = bytearray(struct.pack('<4sII', b'HWS1', samples, len(names)))
    for name in names:
        payload.extend(encoder.name_bytes(name))
    for clip, animation in enumerate(doc['animations']):
        record = records[animation['name']]
        for time in np.linspace(0, record['seconds'], 2 * record['frames'] - 1):
            time = float(np.float32(time))
            world = world_at(doc, blob, animation, time)
            payload.extend(struct.pack('<If', clip, time))
            for index in indices:
                payload.extend(np.asarray(world[index].ravel(order='F'), dtype='<f4').tobytes())
    (output / (variant['id'] + '.hws')).write_bytes(payload)


def cook(output, export_root, reference=None, blender=None):
    recipe = checked_recipe()
    output, export_root = output.resolve(), export_root.resolve()
    if not export_root.is_relative_to(ROOT):
        raise ValueError('Infantry export provenance must remain repository-relative')
    destinations = [output] + ([reference.resolve()] if reference else [])
    all_paths = [export_root, *destinations]
    if any(a == b or a.is_relative_to(b) or b.is_relative_to(a)
           for i, a in enumerate(all_paths) for b in all_paths[i + 1:]):
        raise ValueError('Infantry exports, cook and reference roots must be separate')
    ensure_exports(recipe, export_root, blender)
    with ExitStack() as stack:
        stack.enter_context(locked_output(export_root))
        for destination in sorted(destinations):
            stack.enter_context(locked_output(destination))
        reports, before = validated_inputs(recipe, export_root)
        # Keep the legacy encoder's required report vocabulary in a private copy.
        # Neither the producer's reports nor its global publication are rewritten.
        scratch_root = ROOT / 'build/infantry-native-snapshots'
        scratch_root.mkdir(parents=True, exist_ok=True)
        scratch = Path(stack.enter_context(tempfile.TemporaryDirectory(dir=scratch_root)))
        for directory in ('exports', 'reports'):
            (scratch / directory).mkdir()
        for variant in recipe['variants']:
            identity = variant['id']
            shutil.copyfile(export_root / 'exports' / (identity + '.glb'), scratch / 'exports' / (identity + '.glb'))
            report = dict(reports[identity])
            report['source'], report['source_sha256'] = report['body_source'], report['body_source_sha256']
            (scratch / 'reports' / (identity + '.json')).write_text(json.dumps(report))
        if any(digest(path) != expected for path, expected in before.items()):
            raise ValueError('Infantry inputs changed while snapshotting')
        cooked = stack.enter_context(staging(output))
        references = stack.enter_context(staging(reference.resolve())) if reference else None
        original_exports = encoder.EXPORTS
        records, sockets = [], []
        try:
            encoder.EXPORTS = scratch / 'exports'
            for variant in recipe['variants']:
                record = encoder.cook(variant, recipe, cooked, references)
                record['source'] = str((export_root / 'exports' / (variant['id'] + '.glb')).relative_to(ROOT))
                record['weapon_source'] = variant['weapon_source']
                record['weapon_source_sha256'] = digest(source_path(variant['weapon_source']))
                records.append(record)
                sockets.append(socket_metadata(encoder.EXPORTS / (variant['id'] + '.glb'), variant))
                if references:
                    write_socket_reference(encoder.EXPORTS / (variant['id'] + '.glb'), variant,
                                           reports[variant['id']]['clips'], references)
        finally:
            encoder.EXPORTS = original_exports
        if any(digest(path) != expected for path, expected in before.items()):
            raise ValueError('Infantry inputs changed during cooking')
        tools = [Path(__file__), Path(encoder.__file__), ROOT / "tools/art_tools.py", ROOT / "tools/art-toolchain.json", ROOT / "tools/art-requirements.txt", ROOT / 'tools/animation/runtime_reference.py',
                 ROOT / 'tools/cook_style_preview.py', ROOT / 'tools/normalize_glb.py',
                 ROOT / 'tools/transaction_character_animation.py']
        manifest = {'schema': 1, 'format': 'HRA1 + matching HRM1',
                    'scope': 'Technical infantry cook; no motion-art/native approval implied',
                    'models': records, 'sockets': sockets,
                    'source_sha256': {str(path.relative_to(ROOT)): value for path, value in before.items()},
                    'tool_sha256': {str(path.relative_to(ROOT)): digest(path) for path in tools}}
        (cooked / 'infantry-animation-provenance.json').write_text(json.dumps(manifest, indent=2) + '\n')
        names = sorted((path.relative_to(cooked) for path in cooked.iterdir()),
                       key=lambda path: path.name == 'infantry-animation-provenance.json')
        publish(output, cooked, names)
        if references:
            publish(reference.resolve(), references, [path.relative_to(references) for path in references.iterdir()])
        return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--export-root', type=Path, default=ROOT / 'build/animation/infantry')
    parser.add_argument('--validation-output', type=Path)
    parser.add_argument('--blender', type=Path)
    args = parser.parse_args()
    manifest = cook(args.output, args.export_root, args.validation_output, args.blender)
    print('Cooked', len(manifest['models']), 'isolated infantry animation pairs into', args.output)


if __name__ == '__main__':
    main()
