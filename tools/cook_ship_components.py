"""Cook the ship's physical assemblies from its existing editable construction.

The original GLB supplies the embedded paint; the original authoring code supplies
closed named quad parts. New machinery uses that same lathe/material vocabulary.
Only ignored HRM output is written. No source GLB, texture or model is replaced.
"""
from __future__ import annotations

from copy import deepcopy
import hashlib
import io
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image

import build_ufo_modular as author
from cook_style_preview import write_mesh
from normalize_glb import unpack

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / 'assets/production/ship-components.json'
TAU = math.tau


def contract():
    value = json.loads(CONTRACT.read_text())
    if value['schema'] != 1 or value['kind'] != 'human-resources-ship-components':
        raise ValueError('Unsupported physical ship component recipe')
    if len(value['models']) != len(set(value['models'])):
        raise ValueError('Duplicate physical ship component')
    return value


def embedded_paint(source):
    doc, chunks = unpack(source)
    blob = next(data for tag, data in chunks if tag == 0x004e4942)
    images = {}
    for image in doc['images']:
        view = doc['bufferViews'][image['bufferView']]
        offset = view.get('byteOffset', 0)
        images[image['name']] = Image.open(io.BytesIO(blob[offset:offset + view['byteLength']])).convert('RGBA')
    return images


def extrude(name, polygon, depth, finish=7):
    """Closed convex XY section, extruded along Z; authored counterclockwise."""
    count = len(polygon)
    vertices = [[x, y, z] for z in (-depth / 2, depth / 2) for x, y in polygon]
    faces = [list(reversed(range(count))), list(range(count, count * 2))]
    faces += [[i, (i + 1) % count, (i + 1) % count + count, i + count] for i in range(count)]
    author.MESHES[name] = {'vertices': vertices, 'faces': faces,
                          'uvs': [[[.1, .1], [.9, .1], [.9, .9], [.1, .9]][:len(face)]
                                  if len(face) <= 4 else [[.5 + .45 * math.cos(i * TAU / len(face)), .5 + .45 * math.sin(i * TAU / len(face))] for i in range(len(face))]
                                  for face in faces],
                          'finishes': [finish] * len(faces), 'smooth': False}


def box(name, width, height, depth, finish=7, bevel=.04):
    x, y = width / 2, height / 2
    b = min(bevel, x * .45, y * .45)
    extrude(name, [(-x + b, -y), (x - b, -y), (x, -y + b), (x, y - b),
                   (x - b, y), (-x + b, y), (-x, y - b), (-x, -y + b)], depth, finish)


class Assembly:
    def __init__(self, paint):
        self.paint = paint
        self.groups = []

    def part(self, name, position=(0, 0, 0), scale=(1, 1, 1), rotation=None, finish=None, rotor=False):
        mesh = author.MESHES[name]
        p, n, uv, groups = author.mesh_arrays(mesh)
        linear = np.diag(scale)
        if rotation is not None:
            linear = np.asarray(rotation) @ linear
        p = p @ linear.T + position
        n = n @ np.linalg.inv(linear)
        n /= np.linalg.norm(n, axis=1)[:, None]
        for material, indices in groups.items():
            used, inverse = np.unique(indices, return_inverse=True)
            self.groups.append((p[used], n[used], uv[used], inverse, material if finish is None else finish, rotor))

    def write(self, output, asset):
        materials = list(dict.fromkeys(group[4] for group in self.groups))
        textured = [m for m in materials if 'texture' in author.MATERIALS[m]]
        # The ring retains its former 256px paint tile. Tiny machinery does not
        # duplicate the full 14-material/1024px saucer atlas per component.
        cells = max(1, math.ceil(math.sqrt(len(materials))))
        cell = (256 if asset.startswith(('ship-dome', 'ship-rotor-')) else 128) if textured else 4
        image = Image.new('RGBA', (cells * cell, cells * cell), 'white')
        for i, material in enumerate(materials):
            texture = author.MATERIALS[material].get('texture')
            if texture:
                tile = self.paint[Path(texture).stem].resize((cell - 8, cell - 8), Image.Resampling.LANCZOS)
                image.paste(Image.fromarray(np.pad(np.asarray(tile), ((4, 4), (4, 4), (0, 0)), mode='edge')),
                            ((i % cells) * cell, (i // cells) * cell))
        vertices, indices, offset = [], [], 0
        for p, n, uv, ind, material, rotor in self.groups:
            m = author.MATERIALS[material]
            index = materials.index(material)
            if cell > 4:
                uv = (np.clip(uv, 0, 1) * (cell - 8) + 4 + np.array([index % cells, index // cells]) * cell) / (cells * cell)
            else:
                uv = np.zeros_like(uv) + .5
            vertices.append(np.column_stack([p, n, uv, np.tile(m['color'], (len(p), 1)),
                                            np.full(len(p), max(m.get('emissive', [0]))), np.full(len(p), float(rotor))]))
            indices.extend((ind + offset).tolist())
            offset += len(p)
        vertices = np.concatenate(vertices)
        triangles = vertices[np.asarray(indices).reshape(-1, 3), :3]
        areas = np.linalg.norm(np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]), axis=1)
        if not np.isfinite(vertices).all() or np.any(areas < 1e-10):
            raise ValueError('Invalid physical ship geometry: ' + asset)
        write_mesh(output / (asset + '.hrm'), vertices, indices, image)
        return {'id': asset, 'source': contract()['source'], 'source_sha256': hashlib.sha256((ROOT / contract()['source']).read_bytes()).hexdigest(),
                'ship_component': True, 'triangles': len(indices) // 3,
                'bounds': [vertices[:, :3].min(axis=0).tolist(), vertices[:, :3].max(axis=0).tolist()],
                'texture_size': list(image.size), 'review': 'physical ship component; native review required'}


def rotation(axis, radians):
    return np.array([author.rotate(author.quat(axis, radians), v) for v in np.eye(3)]).T


def phase_canopy(point, rank):
    x, y, z = point
    # Retain the circular mounting flange. Only the upper shell elongates.
    taper = min(1, max(0, (y - .65) / .17))
    return [x * (1 - .04 * rank * taper), .615 + (y - .615) * (1 - .10 * rank),
            z * (1 + .18 * rank * taper)]


def geometry():
    author.make_geometry()
    silver = len(author.MATERIALS)
    author.MATERIALS.append({'name': 'Ship_Silver_Armour', 'color': [.54, .67, .78, 1], 'metallic': .8, 'roughness': .22})
    dark = len(author.MATERIALS)
    author.MATERIALS.append({'name': 'Ship_Recessed_Machinery', 'color': [.018, .032, .052, 1], 'metallic': .2, 'roughness': .7})
    # Fixtures are real closed housings, not flat glyphs or arbitrary line work.
    box('Support', .18, .16, 1, 7)
    box('Engine_Pod', .50, .30, .95, 7, .10)
    box('Engine_Inset', .35, .19, .72, dark, .06)
    box('Bay_Shell', .38, .18, .48, 7)
    box('Bay_Recess', .29, .04, .37, dark)
    box('Bay_Rail', .045, .11, .42, 8, .01)
    box('Repair_Body', .20, .10, .25, 6)
    box('Repair_Arm', .035, .04, .15, 8, .008)
    box('Winch_Jaw', .20, .22, .60, 8)
    box('Winch_Tooth', .30, .16, .16, 3)
    box('Rail_Casing', .25, .24, .72, 7)
    box('Rail_Channel', .12, .045, .60, dark, .01)
    box('Electrode', .045, .065, .36, silver, .008)
    author.lathe('Engine_Nozzle', [(.07, 0), (.15, 0), (.18, .06), (.18, .20), (.13, .27), (.07, .27)], 12, 8)
    author.lathe('Engine_Throat', [(0, .01), (.07, .01), (.07, .05), (0, .05)], 12, 2, closed=False)
    author.lathe('Winch_Housing', [(.22, 0), (.50, 0), (.57, .08), (.57, .25), (.44, .34), (.22, .34)], 8, 7)
    author.lathe('Winch_Pivot', [(0, 0), (.16, 0), (.16, .10), (.11, .15), (0, .15)], 12, 8, closed=False)
    author.lathe('Capture_Manifold', [(.18, 0), (.62, 0), (.70, .06), (.70, .13), (.59, .20), (.18, .20)], 24, 7)
    return silver, dark


def cook_components(output):
    recipe = contract()
    output.mkdir(parents=True, exist_ok=True)
    paint = embedded_paint(ROOT / recipe['source'])
    silver, dark = geometry()
    records = []
    def emit(asset, build):
        assembly = Assembly(paint)
        build(assembly)
        records.append(assembly.write(output, asset))
    emit('ship-dome', lambda a: a.part('Dome', position=(0, -.615, 0)))
    for phase in range(1, 4):
        name = f'Phase_Dome_{phase}'
        author.MESHES[name] = deepcopy(author.MESHES['Dome'])
        author.MESHES[name]['vertices'] = [phase_canopy(p, phase) for p in author.MESHES[name]['vertices']]
        emit(f'ship-dome-phase-{phase}', lambda a, n=name: a.part(n, position=(0, -.615, 0)))
    emit('ship-collar', lambda a: a.part('Dome_Collar', position=(0, -.58, 0)))
    for rank in range(4):
        name = f'Rotor_{rank}'
        if rank == 0:
            author.MESHES[name] = deepcopy(author.MESHES['Outer_Ring'])
        else:
            shoulder = [(.604,.207),(.80,0),(1.00,.142),(1.23,.18),(1.30,.22)]
            if rank == 1:
                shoulder += [(1.30,.42),(1.24,.48),(1.10,.48),(.96,.50),(.71,.581),(.604,.581)]
            elif rank == 2:
                shoulder += [(1.30,.46),(1.26,.52),(1.19,.52),(1.19,.61),(1.11,.61),(1.06,.55),(.96,.54),(.71,.581),(.604,.581)]
            else:
                shoulder += [(1.30,.52),(1.26,.58),(1.20,.58),(1.20,.68),(1.10,.68),(1.04,.58),(.96,.56),(.71,.581),(.604,.581)]
            author.lathe(name, shoulder, 48, 0, bands={0:1,1:7,2:7,3:0,len(shoulder)-1:1})
            m=author.MESHES[name]
            m['uvs']=[[[.5+m['vertices'][k][0]/2.652,.5+m['vertices'][k][2]/2.652] for k in face] for face in m['faces']]
        emit(f'ship-rotor-{rank}', lambda a, n=name: a.part(n, rotor=True))
    for rank in range(1, 4):
        def rail(a, r=rank):
            a.part('Laser_Base')
            length = .95 + .16 * r
            a.part('Rail_Casing', position=(0, .07, length / 2), scale=(1, 1, length / .72))
            a.part('Rail_Channel', position=(0, .205, length / 2), scale=(1, 1, length / .72))
            a.part('Laser_Lens', position=(0, .07, length - .54 * .65 / .55), scale=(1 + .12 * r, 1 + .12 * r, 1), rotation=rotation((1, 0, 0), math.pi / 2))
        emit(f'ship-laser-barrel-{rank}', rail)
        def electrode(a, r=rank):
            # Rear plate and two ceramic holders form one connected muzzle head.
            a.part('Engine_Pod', scale=(.55, .45, .13), finish=8)
            for side in (-1, 1):
                a.part('Laser_Base', position=(side * (.10 + .02 * r), 0, 0), scale=(.32, .32, .32), finish=6)
                a.part('Electrode', position=(side * (.10 + .02 * r), .07, .10 + .04 * r), scale=(1 + .15 * r, 1, 1 + .3 * r))
        emit(f'ship-electrode-{rank}', electrode)
    emit('ship-capture-port', lambda a: (a.part('Beam_Housing'), a.part('Lens_Bezel', position=(0, .02, 0), scale=(1.45, 1, 1.45)), a.part('Lens', position=(0, .02, 0), scale=(1.45, .65, 1.45))))
    emit('ship-capture-manifold', lambda a: a.part('Capture_Manifold'))
    emit('ship-winch-housing', lambda a: a.part('Winch_Housing'))
    emit('ship-winch-jaw', lambda a: (a.part('Winch_Jaw'), a.part('Winch_Tooth', position=(.10, -.10, -.22))))
    emit('ship-winch-jaw-right', lambda a: (a.part('Winch_Jaw'), a.part('Winch_Tooth', position=(-.10, -.10, -.22))))
    emit('ship-winch-pivot', lambda a: a.part('Winch_Pivot'))
    emit('ship-pylon', lambda a: a.part('Support'))
    emit('ship-maintenance-bay', lambda a: (a.part('Bay_Shell', position=(0,-.025,0), scale=(1,.2,1)), a.part('Bay_Recess'), a.part('Bay_Rail', position=(-.16,.045,0)), a.part('Bay_Rail', position=(.16,.045,0)), a.part('Bay_Rail', position=(0,.045,-.21), scale=(1,1,.8), rotation=rotation((0,1,0),math.pi/2)), a.part('Bay_Rail', position=(0,.045,.21), scale=(1,1,.8), rotation=rotation((0,1,0),math.pi/2))))
    emit('ship-repair-unit', lambda a: (a.part('Repair_Body'), a.part('Antenna_Tip', position=(0, .06, -.06), scale=(.45, .45, .45)), a.part('Repair_Arm', position=(-.10, 0, -.12)), a.part('Repair_Arm', position=(.10, 0, -.12))))
    emit('ship-engine-pod', lambda a: (a.part('Engine_Pod'), a.part('Engine_Inset', position=(0, .13, 0))))
    emit('ship-engine-nozzle', lambda a: (a.part('Engine_Nozzle', rotation=rotation((1, 0, 0), math.pi / 2)), a.part('Engine_Throat', rotation=rotation((1, 0, 0), math.pi / 2))))
    emit('ship-shield-base', lambda a: (a.part('Shield_Base'), a.part('Shield_Stem')))
    emit('ship-shield-crystal', lambda a: a.part('Shield_Crystal', position=(0, -.298, 0)))
    def drone(a, elite=False):
        a.part('Hull')
        a.part('Dome_Collar')
        a.part('Dome', scale=(1, .75, 1), position=(0, .15, 0))
        a.part('Outer_Ring', scale=(.75, 1, .75))
        a.part('Laser_Barrel', position=(0, .50, .48), scale=(.65, .65, .65), rotation=rotation((1, 0, 0), math.pi / 2))
        a.part('Laser_Lens', position=(0, .50, .48), scale=(.65, .65, .65), rotation=rotation((1, 0, 0), math.pi / 2))
        if elite:
            a.part('Winch_Housing', position=(0, .55, 0), scale=(1.1, .5, 1.1), finish=silver)
    emit('ship-drone', lambda a: drone(a))
    emit('ship-drone-elite', lambda a: drone(a, True))
    if {row['id'] for row in records} != set(recipe['models']):
        raise ValueError('Physical ship recipe and authored components disagree')
    return records


def write_component_header(header, records):
    recipe = contract()
    number = lambda v: f'{float(v):.9f}F'
    values = '\n'.join('inline constexpr float ' + name + '=' + number(recipe[key]) + ';' for name, key in
                       [('ship_ring_radius', 'ring_outer_radius_m'), ('ship_ring_growth', 'ring_growth_per_rank'),
                        ('ship_drone_scale', 'drone_scale'), ('ship_drone_ship_gap', 'drone_ship_gap_width_fraction'),
                        ('ship_drone_neighbor_gap', 'drone_neighbor_gap_width_fraction')])
    names = ','.join(json.dumps(row['id']) for row in records)
    lamp_radius = .604 + (.851 - .604) * (1.3 - .604) / (1.1 - .604)
    def deck_plane(name):
        positions, normals, _, indices = author.mesh_arrays(author.MESHES[name])
        surfaces = []
        for triangles in indices.values():
            for triangle in triangles.reshape(-1, 3):
                p = positions[triangle].astype(float)
                basis = np.array([p[1, [0, 2]] - p[0, [0, 2]], p[2, [0, 2]] - p[0, [0, 2]]]).T
                if abs(np.linalg.det(basis)) < 1e-12:
                    continue
                u, v = np.linalg.solve(basis, np.array([lamp_radius, 0]) - p[0, [0, 2]])
                if min(u, v, 1 - u - v) >= -1e-7:
                    height = p[0, 1] * (1 - u - v) + p[1, 1] * u + p[2, 1] * v
                    normal = np.cross(p[1] - p[0], p[2] - p[0])
                    surfaces.append((height, abs(normal[0] / normal[1])))
        return max(surfaces)
    baseline, _ = deck_plane('Rotor_0')
    offset = .533 - baseline
    decks = [deck_plane(f'Rotor_{rank}') for rank in range(4)]
    anchors = '\ninline constexpr float ship_lamp_radius=' + number(lamp_radius) + ';\n'
    anchors += 'inline constexpr std::array<float,4> ship_lamp_height{{' + ','.join(number(y + offset) for y, _ in decks) + '}};\n'
    anchors += 'inline constexpr std::array<float,4> ship_lamp_slope{{' + ','.join(number(slope) for _, slope in decks) + '}};\n'
    header.write_text('#pragma once\n#include <array>\n#include <string_view>\nnamespace human_resources::product::detail {\n' + values +
                      anchors + '\ninline constexpr std::array<std::string_view,' + str(len(records)) + '> ship_component_ids{{' + names + '}};\n}\n')
