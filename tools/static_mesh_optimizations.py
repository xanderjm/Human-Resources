"""Checked authored exports for the two reviewed static foliage meshes.

Normal cooking uses only the standard library here. Optional authoring replay
uses the existing pinned Blender/art Python and writes under ignored build/.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
RECIPE = 'assets/production/static-mesh-optimizations.json'
ASSETS = {'crop-seedling': (1700, 425), 'grass-tuft': (1814, 453)}
TOOLS = {'tools/static_mesh_optimizations.py', 'tools/build_static_mesh_optimizations.py',
         'tools/normalize_glb.py', 'tools/art-toolchain.json'}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_path(root: Path, name: str) -> Path:
    relative = Path(name)
    if relative.is_absolute() or '..' in relative.parts or '\\' in name:
        raise ValueError('Static mesh source must be a repository-relative path')
    path = root / relative
    if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(root.resolve()):
        raise ValueError('Static mesh source missing or outside repository: ' + name)
    return path


def checked_recipe(root: Path = ROOT) -> dict:
    recipe = json.loads(source_path(root, RECIPE).read_text())
    if set(recipe) != {'schema', 'method', 'tools', 'models'} or recipe['schema'] != 1:
        raise ValueError('Static mesh recipe schema must match the reviewed contract')
    method = recipe['method']
    if method != {'name': 'exact-position-weld-collapse', 'ratio': 0.25,
                  'triangulate': True, 'blender': '4.5.15',
                  'corner_normal_tolerance': 0.001, 'bounds': 'original-axis-affine'}:
        raise ValueError('Static mesh method differs from the reviewed export')
    if set(recipe['tools']) != TOOLS:
        raise ValueError('Static mesh recipe tool owners differ')
    for name, expected in recipe['tools'].items():
        if digest(source_path(root, name)) != expected:
            raise ValueError('Static mesh authoring tool hash mismatch: ' + name)
    toolchain = json.loads(source_path(root, 'tools/art-toolchain.json').read_text())
    if toolchain['blender_version'] != method['blender']:
        raise ValueError('Static mesh Blender pin differs from existing art toolchain')
    rows = recipe['models']
    if not isinstance(rows, list) or len(rows) != len(ASSETS) or {row['id'] for row in rows} != ASSETS.keys():
        raise ValueError('Static mesh recipe must contain exactly crop-seedling and grass-tuft once')
    expected_keys = {'id', 'source', 'source_sha256', 'runtime_source', 'runtime_source_sha256',
                     'triangles_before', 'triangles_after', 'cooked_sha256',
                     'texture_payload_sha256', 'topology', 'review'}
    for row in rows:
        asset = row['id']
        if set(row) != expected_keys or (row['triangles_before'], row['triangles_after']) != ASSETS[asset]:
            raise ValueError('Static mesh model contract differs: ' + asset)
        if row['source'] != f'assets/models/{asset}/{asset}.glb' or row['runtime_source'] != f'assets/models/{asset}/{asset}-runtime.glb':
            raise ValueError('Static mesh source paths differ from explicit reviewed exports')
        for key in ('source', 'runtime_source'):
            if digest(source_path(root, row[key])) != row[key + '_sha256']:
                raise ValueError('Static mesh source hash mismatch: ' + row[key])
    return recipe


def validate_cooked(recipe: dict, asset: str, output: Path) -> None:
    row = next(row for row in recipe['models'] if row['id'] == asset)
    data = (output / (asset + '.hrm')).read_bytes()
    if hashlib.sha256(data).hexdigest() != row['cooked_sha256']:
        raise ValueError('Static mesh cook differs from reviewed geometry: ' + asset)
    magic, vertices, indices, width, height = struct.unpack_from('<4sIIII', data)
    texture = data[20 + vertices * 56 + indices * 4:]
    if (magic != b'HRM1' or indices != row['triangles_after'] * 3 or
            len(texture) != width * height * 4 or
            hashlib.sha256(texture).hexdigest() != row['texture_payload_sha256']):
        raise ValueError('Static mesh cook texture/count differs: ' + asset)


def provenance(recipe: dict, asset: str, root: Path = ROOT) -> dict:
    row = next(row for row in recipe['models'] if row['id'] == asset)
    return {'canonical_source': row['source'], 'canonical_source_sha256': row['source_sha256'],
            'reviewed_runtime_source': row['runtime_source'],
            'reviewed_runtime_source_sha256': row['runtime_source_sha256'],
            'recipe': RECIPE, 'recipe_sha256': digest(root / RECIPE),
            'method': recipe['method'], 'authoring_tool_sha256': recipe['tools']}


def replay(output: Path, root: Path = ROOT) -> None:
    # Replays never publish or overwrite either reviewed source export.
    checked_recipe(root)
    destination = output.resolve()
    build = (root / 'build').resolve()
    if not destination.is_relative_to(build) or destination == build:
        raise ValueError('Static mesh replay output must be inside ignored build/')
    destination.mkdir(parents=True, exist_ok=False)
    from art_tools import art_python, blender_executable
    script = root / 'tools/build_static_mesh_optimizations.py'
    subprocess.run([str(blender_executable(root=root)), '--background', '--factory-startup',
                    '--python-exit-code', '1', '--python', str(script), '--',
                    '--root', str(root), '--output', str(destination)], check=True)
    subprocess.run([str(art_python(root)), str(script), '--assemble',
                    '--root', str(root), '--output', str(destination)], check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replay', type=Path, help='Fresh ignored build/ directory for optional exact authoring replay')
    args = parser.parse_args()
    if args.replay:
        replay(args.replay)
    else:
        checked_recipe()
        print('Reviewed crop/grass source exports and recipe hashes match')


if __name__ == '__main__':
    main()
