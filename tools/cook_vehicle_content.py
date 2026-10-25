"""Reproduce the reviewed rigid mount subsets from the original source GLB.

No Blender, source rewrite, new geometry, runtime source files or ignored
authoring exports are required. NumPy is the existing offline art dependency.
"""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import numpy as np
from normalize_glb import unpack, accessor, matrix, pack
from glb_geometry_repair import append_array

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / 'assets/production/vehicle-weapons.json'

def components(positions, triangles):
    _, inverse = np.unique(np.round(positions, 5), axis=0, return_inverse=True)
    parents = np.arange(inverse.max() + 1)
    def find(index):
        while parents[index] != index:
            parents[index] = parents[parents[index]]
            index = parents[index]
        return index
    for triangle in inverse[triangles]:
        for index in triangle[1:]: parents[find(index)] = find(triangle[0])
    groups = {}
    for index, value in enumerate(inverse): groups.setdefault(find(value), []).append(index)
    return [np.asarray(indices) for indices in groups.values()]

def split_mount(output):
    contract = json.loads(CONTRACT.read_text())
    for name, wanted in contract['source_sha256'].items():
        if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != wanted:
            raise ValueError('Vehicle source no longer matches reviewed assembly: ' + name)
    source = ROOT / 'assets/models/jeep-minigun-mount/jeep-minigun-mount.glb'
    doc, chunks = unpack(source)
    blob = next(value for tag, value in chunks if tag == 0x004e4942)
    primitive = doc['meshes'][0]['primitives'][0]
    if len(doc['meshes']) != 1 or len(doc['meshes'][0]['primitives']) != 1:
        raise ValueError('Reviewed mount requires one static source mesh')
    frames=[]
    def visit(index, parent):
        node=doc['nodes'][index]; world=parent @ matrix(node)
        if 'mesh' in node: frames.append(world)
        for child in node.get('children',[]): visit(child,world)
    for index in doc['scenes'][doc.get('scene',0)]['nodes']: visit(index,np.eye(4))
    if len(frames)!=1: raise ValueError('Reviewed mount requires one mesh instance')
    transform=frames[0]
    attributes = {name: accessor(doc, blob, index) for name, index in primitive['attributes'].items()}
    positions = attributes['POSITION']
    triangles = accessor(doc, blob, primitive['indices']).reshape(-1, 3)
    ids = np.empty(len(positions), dtype=np.int32)
    normalized = (transform @ np.column_stack([positions, np.ones(len(positions))]).T).T[:, :3]
    for index, group in enumerate(components(normalized, triangles)): ids[group] = index
    rotating = np.isin(ids, contract['split']['rotating_component_ids'])
    output.mkdir(parents=True, exist_ok=True)
    results = {}
    for name, chosen, count in [('jeep-minigun-fixed-post', ~rotating, 424), ('jeep-minigun-yaw-cap', rotating, 560)]:
        selected = triangles[np.all(chosen[triangles], axis=1)]
        if len(selected) != count: raise ValueError('Reviewed source component membership changed: ' + name)
        used, inverse = np.unique(selected.reshape(-1), return_inverse=True)
        derived, binary = deepcopy(doc), deepcopy(chunks)
        target = deepcopy(primitive)
        target['attributes'] = {}
        for key, values in attributes.items():
            values = values[used].copy()
            if key == 'POSITION':
                values = normalized[used].copy()
                if name.endswith('yaw-cap'): values -= contract['yaw_pivot_mount_m']
            elif key == 'NORMAL':
                values = values @ np.linalg.inv(transform[:3, :3])
                values /= np.linalg.norm(values, axis=1)[:, None]
            target['attributes'][key] = append_array(derived, binary, values, 5126, {2:'VEC2',3:'VEC3',4:'VEC4'}[values.shape[1]], 34962)
        target['indices'] = append_array(derived, binary, inverse, 5125, 'SCALAR', 34963)
        derived['meshes'] = [{'primitives':[target]}]
        derived['nodes'] = [{'mesh':0,'name':name}]
        derived['scenes'] = [{'nodes':[0]}]; derived['scene'] = 0
        path = output / (name + '.glb'); pack(derived, binary, path)
        results[name] = path
    return results

def write_vehicle_header(header):
    assemblies = json.loads((ROOT / 'assets/production/assemblies.json').read_text())['assemblies']
    jeep = next(row for row in assemblies if row['id'] == 'jeep-four-wheels')
    tank = next(row for row in assemblies if row['id'] == 'tank-with-turret')
    number = lambda v: f'{float(v):.9f}F'
    vector = lambda p: '{' + ','.join(number(v) for v in p) + '}'
    wheels = ['{' + vector(row['translation_m']) + ',' + number(row['yaw_degrees'] * np.pi / 180) + '}' for row in jeep['sockets']]
    header.write_text('#pragma once\n#include <array>\nnamespace human_resources::product::detail {\n'
        'struct JeepWheelSocket { std::array<float,3> position; float yaw; };\n'
        'inline constexpr std::array<JeepWheelSocket,4> jeep_wheels{{' + ','.join(wheels) + '}};\n'
        'inline constexpr std::array<float,3> jeep_chassis_offset' + vector(jeep['nodes'][0]['translation_m']) + ';\n'
        'inline constexpr std::array<float,3> tank_turret_offset' + vector(tank['sockets'][0]['translation_m']) + ';\n}\n')
