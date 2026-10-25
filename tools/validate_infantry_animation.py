"""Independent GLB playback/source-preservation gate for equipped infantry.

Uses NumPy/Pillow from the offline art Python. No Blender or authoring pose code
is imported. Numerical acceptance does not replace equipped motion art review.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import io
import itertools
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image

from normalize_glb import accessor, matrix, unpack

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/animation/infantry'
FIT = ROOT / 'assets/animation/infantry-fit.json'
SOCKETS = ROOT / 'assets/production/weapon-sockets.json'
FPS = 30
REQUIRED_CLIPS = {
    'idle', 'walk', 'run', 'alert', 'scan', 'startle', 'hit_react',
    'beam_lift', 'beam_struggle', 'beam_release', 'aim_enter', 'aim', 'fire', 'aim_exit',
}
POS_TOL = 3e-5
GRIP_TOL = 5e-4


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_glb(path):
    raw = path.read_bytes()
    require(12 <= len(raw) <= 128 * 1024 * 1024, 'GLB file size bound')
    require(struct.unpack_from('<4sII', raw) == (b'glTF', 2, len(raw)), 'GLB header')
    offset = 12
    types = []
    while offset < len(raw):
        require(offset + 8 <= len(raw), 'Truncated GLB chunk header')
        size, kind = struct.unpack_from('<II', raw, offset)
        require(size % 4 == 0 and offset + 8 + size <= len(raw), 'GLB chunk range')
        types.append(kind)
        offset += 8 + size
    require(types == [0x4e4f534a, 0x004e4942], 'Expected embedded JSON/BIN only')
    doc, chunks = unpack(path)
    blob = chunks[1][1]
    require(len(doc.get('buffers', [])) == 1 and 'uri' not in doc['buffers'][0], 'External buffer')
    require(doc['buffers'][0]['byteLength'] <= len(blob), 'Buffer length')
    for view in doc.get('bufferViews', []):
        require(view.get('buffer', 0) == 0 and view.get('byteOffset', 0) >= 0 and
                view['byteLength'] >= 0 and
                view.get('byteOffset', 0) + view['byteLength'] <= len(blob), 'Buffer view range')
    widths = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}
    sizes = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
    for a in doc.get('accessors', []):
        require('sparse' not in a and a['type'] in widths and a['componentType'] in sizes,
                'Unsupported accessor')
        require(0 < a['count'] <= 600000, 'Accessor count bound')
        view = doc['bufferViews'][a['bufferView']]
        width = widths[a['type']] * sizes[a['componentType']]
        stride = view.get('byteStride', width)
        require(stride >= width and a.get('byteOffset', 0) >= 0 and
                a.get('byteOffset', 0) + stride * (a['count'] - 1) + width <= view['byteLength'],
                'Accessor byte range')
    return doc, blob


def hierarchy(doc):
    parents = {}
    for index, node in enumerate(doc['nodes']):
        for child in node.get('children', []):
            require(0 <= child < len(doc['nodes']) and child not in parents, 'Multiple node parents')
            parents[child] = index
    roots = doc['scenes'][doc.get('scene', 0)]['nodes']
    require(len(roots) == len(set(roots)) and all(r not in parents for r in roots), 'Invalid scene roots')
    order, active = [], set()

    def visit(index):
        require(index not in active and index not in order, 'Cyclic/repeated hierarchy')
        active.add(index)
        order.append(index)
        for child in doc['nodes'][index].get('children', []):
            visit(child)
        active.remove(index)

    for root in roots:
        visit(root)
    require(len(order) == len(doc['nodes']), 'Orphan nodes')
    return parents, order


def world_matrices(nodes, parents, order):
    world = [None] * len(nodes)
    for index in order:
        local = matrix(nodes[index])
        require(np.isfinite(local).all(), 'Nonfinite node matrix')
        world[index] = local if index not in parents else world[parents[index]] @ local
    return np.asarray(world)


def quaternion_mix(a, b, amount):
    dot = float(np.dot(a, b))
    if dot < 0:
        b, dot = -b, -dot
    if dot > .9995:
        value = a + amount * (b - a)
    else:
        angle = np.arccos(np.clip(dot, -1, 1))
        value = (np.sin((1 - amount) * angle) * a + np.sin(amount * angle) * b) / np.sin(angle)
    return value / np.linalg.norm(value)


class Playback:
    def __init__(self, doc, blob, record):
        self.doc = doc
        self.parents, self.order = hierarchy(doc)
        self.channels = []
        animation = next(a for a in doc['animations'] if a['name'] == record['name'])
        require(0 < len(animation['channels']) <= 192, 'Animation channel bound')
        targets = set()
        for channel in animation['channels']:
            target = channel['target']
            key = target['node'], target['path']
            require(key not in targets and 0 <= key[0] < len(doc['nodes']), 'Duplicate/invalid channel target')
            targets.add(key)
            require(key[1] in ('translation', 'rotation', 'scale'), 'Unsupported animation target')
            sampler = animation['samplers'][channel['sampler']]
            require(sampler.get('interpolation', 'LINEAR') == 'LINEAR', 'Expected LINEAR sampler')
            times = accessor(doc, blob, sampler['input']).ravel().astype(float)
            values = accessor(doc, blob, sampler['output']).astype(float)
            require(len(times) == len(values) == record['frames'] and 2 <= len(times) <= 256,
                    'Baked frame count mismatch')
            require(np.isfinite(times).all() and np.isfinite(values).all(), 'Nonfinite animation sampler')
            require(np.allclose(times, np.arange(len(times)) / FPS, atol=2e-6, rtol=0),
                    'Sampler is not zero-based 30Hz')
            require(abs(times[-1] - record['seconds']) < 2e-6, 'Clip duration mismatch')
            require(values.shape[1] == (4 if key[1] == 'rotation' else 3), 'Sampler value width')
            if key[1] == 'rotation':
                require(np.all(np.abs(np.linalg.norm(values, axis=1) - 1) < 2e-4), 'Nonunit quaternion')
            if key[1] == 'scale':
                require(np.all(values > 0) and np.all(values < 4), 'Unbounded/nonpositive scale')
            self.channels.append((key, times, values))

    def at(self, time):
        nodes = [dict(node) for node in self.doc['nodes']]
        for (index, path), times, values in self.channels:
            left = max(0, min(len(times) - 2, np.searchsorted(times, time, side='right') - 1))
            fraction = np.clip((time - times[left]) / (times[left + 1] - times[left]), 0, 1)
            value = quaternion_mix(values[left], values[left + 1], fraction) if path == 'rotation' else (
                values[left] + fraction * (values[left + 1] - values[left]))
            nodes[index].pop('matrix', None)
            nodes[index][path] = value
        return world_matrices(nodes, self.parents, self.order)


def image_pixels(doc, blob):
    require(len(doc.get('images', [])) == 1, 'Expected one embedded source albedo')
    image = doc['images'][0]
    require('bufferView' in image and 'uri' not in image, 'External image')
    view = doc['bufferViews'][image['bufferView']]
    begin = view.get('byteOffset', 0)
    with Image.open(io.BytesIO(blob[begin:begin + view['byteLength']])) as decoded:
        require(decoded.width * decoded.height <= 4096 * 4096, 'Image dimensions bound')
        return np.asarray(decoded.convert('RGBA'))


def skin_data(doc, blob):
    require(len(doc.get('skins', [])) == len(doc.get('meshes', [])) == 1, 'Expected one body mesh/skin')
    skin = doc['skins'][0]
    joints = skin['joints']
    require(1 <= len(joints) <= 64 and len(joints) == len(set(joints)), 'Joint count/uniqueness')
    require(all(0 <= j < len(doc['nodes']) for j in joints), 'Joint node bounds')
    inverse = accessor(doc, blob, skin['inverseBindMatrices']).reshape(-1, 4, 4).transpose(0, 2, 1)
    require(len(inverse) == len(joints) and np.isfinite(inverse).all(), 'Inverse bind matrices')
    mesh_nodes = [n for n in doc['nodes'] if 'mesh' in n]
    require(len(mesh_nodes) == 1 and mesh_nodes[0].get('skin') == 0, 'Expected one skinned body instance')
    primitives = doc['meshes'][0]['primitives']
    require(len(primitives) == 1, 'Body primitive count')
    primitive = primitives[0]
    require(primitive.get('mode', 4) == 4 and 'targets' not in primitive, 'Expected unmorphed triangles')
    attrs = primitive['attributes']
    require(set(attrs) == {'POSITION', 'NORMAL', 'TEXCOORD_0', 'JOINTS_0', 'WEIGHTS_0'}, 'Skinned attributes')
    pos, normal, uv, influences, weights = [accessor(doc, blob, attrs[k]) for k in (
        'POSITION', 'NORMAL', 'TEXCOORD_0', 'JOINTS_0', 'WEIGHTS_0')]
    require(0 < len(pos) <= 32768 and all(len(v) == len(pos) for v in (normal, uv, influences, weights)),
            'Vertex counts')
    require(all(np.isfinite(v).all() for v in (pos, normal, uv, weights)), 'Nonfinite vertex attributes')
    require(np.max(np.abs(np.linalg.norm(normal, axis=1) - 1)) < 2e-3, 'Nonunit normal')
    require(np.all(weights >= 0) and np.all(weights <= 1) and
            np.allclose(weights.sum(axis=1), 1, atol=2e-5, rtol=0), 'Skin weights not normalized')
    require(influences.min() >= 0 and influences.max() < len(joints), 'Influence index bound')
    indices = accessor(doc, blob, primitive['indices']).ravel().astype(int)
    require(0 < len(indices) <= 600000 and len(indices) % 3 == 0 and
            indices.min() >= 0 and indices.max() < len(pos), 'Triangle indices')
    return np.array(joints), inverse, np.column_stack((pos, np.ones(len(pos)))), influences.astype(int), weights, uv, indices


def deform(world, data):
    joints, inverse, pos, influences, weights, _, _ = data
    skin = world[joints] @ inverse
    output = np.zeros_like(pos)
    for slot in range(4):
        output += np.einsum('nij,nj->ni', skin[influences[:, slot]], pos) * weights[:, slot, None]
    return output[:, :3]


def source_geometry(doc, blob):
    parents, order = hierarchy(doc)
    world = world_matrices(doc['nodes'], parents, order)
    positions, uvs, triangles = [], [], []
    for index in order:
        node = doc['nodes'][index]
        if 'mesh' not in node:
            continue
        for primitive in doc['meshes'][node['mesh']]['primitives']:
            pos = accessor(doc, blob, primitive['attributes']['POSITION'])
            uv = accessor(doc, blob, primitive['attributes']['TEXCOORD_0'])
            ind = accessor(doc, blob, primitive['indices']).ravel().astype(int)
            triangles.append(ind + sum(map(len, positions)))
            positions.append((world[index] @ np.column_stack((pos, np.ones(len(pos)))).T).T[:, :3])
            uvs.append(uv)
    return np.concatenate(positions), np.concatenate(uvs), np.concatenate(triangles)


def geometry_preserved(bind, uv, indices, source):
    original, source_uv, source_indices = source
    require(len(indices) == len(source_indices), 'Source triangle count changed')
    cells = defaultdict(list)
    for i, point in enumerate(original):
        cells[tuple(np.floor(point / POS_TOL).astype(int))].append(i)
    source_ids = [tuple(np.round(np.r_[p, v], 7)) for p, v in zip(original, source_uv)]
    mapped = []
    max_error = 0
    for point, coord in zip(bind, uv):
        cell = np.floor(point / POS_TOL).astype(int)
        candidates = [i for delta in itertools.product((-1, 0, 1), repeat=3)
                      for i in cells.get(tuple(cell + delta), ())]
        good = [i for i in candidates if np.linalg.norm(original[i] - point) < POS_TOL and
                np.max(np.abs(source_uv[i] - coord)) < 2e-6]
        require(good, 'Source position/UV corner absent in export')
        best = min(good, key=lambda i: np.linalg.norm(original[i] - point))
        mapped.append(source_ids[best])
        max_error = max(max_error, float(np.linalg.norm(original[best] - point)))
    require(set(mapped) == set(source_ids), 'Source position/UV vertices missing from export')

    def triangles(ids, ind):
        result = Counter()
        for a, b, c in ind.reshape(-1, 3):
            corners = (ids[a], ids[b], ids[c])
            result[min(corners, corners[1:] + corners[:1], corners[2:] + corners[:2])] += 1
        return result

    require(triangles(mapped, indices) == triangles(source_ids, source_indices),
            'Source position/UV triangle topology or winding changed')
    return max_error


def check_character(fit, weapons):
    identity = fit['id']
    path = OUT / 'exports' / (identity + '.glb')
    report = json.loads((OUT / 'reports' / (identity + '.json')).read_text())
    require(report['id'] == identity, 'Report identity')
    weapon = weapons[fit['equipment_candidate']['weapon_id']]
    for prefix, expected, expected_hash in (
        ('body', fit['model_file'], fit['model_sha256']),
        ('weapon', weapon['model_file'], weapon['model_sha256']),
    ):
        require(report[prefix + '_source'] == expected, 'Unexpected ' + prefix + ' source')
        require(digest(ROOT / expected) == report[prefix + '_source_sha256'] == expected_hash,
                'Stale/modified ' + prefix + ' source')
    require(report.get('authoring_inputs'), 'Missing authoring inputs')
    for name, expected_hash in report['authoring_inputs'].items():
        require(digest(ROOT / name) == expected_hash, 'Stale authoring input: ' + name)
    doc, blob = read_glb(path)
    require(1 <= len(doc['nodes']) <= 64, 'Node count bound')
    names = [n.get('name') for n in doc['nodes']]
    require(len(names) == len(set(names)), 'Node names not unique')
    node = {name: i for i, name in enumerate(names)}
    required = {'socket_weapon', 'socket_palm.L', 'socket_palm.R', 'socket_primary', 'socket_support'}
    muzzle_names = {'muzzle': 'socket_muzzle', 'muzzle_left': 'socket_muzzle_left',
                    'muzzle_right': 'socket_muzzle_right', 'loaded_nose_tip': 'socket_nose',
                    'tube_mouth_plane': 'socket_tube'}
    required.update(muzzle_names[s] for s in weapon['sockets'] if s in muzzle_names)
    require(required <= node.keys(), 'Missing grip/muzzle marker: ' + str(required - node.keys()))
    clips = report['clips']
    require(len(clips) == len(doc['animations']) <= 16, 'Clip count mismatch/bound')
    require({c['name'] for c in clips} == {a['name'] for a in doc['animations']} == REQUIRED_CLIPS,
            'Required clip set mismatch')
    require(len(clips) == len({c['name'] for c in clips}), 'Duplicate clips')
    require(all(isinstance(c['loop'], bool) and 0 < c['seconds'] <= 10 and
                np.isfinite(c['stride_m']) and c['stride_m'] >= 0 for c in clips),
            'Invalid clip metadata')
    data = skin_data(doc, blob)
    parents, order = hierarchy(doc)
    rest = world_matrices(doc['nodes'], parents, order)
    bind = deform(rest, data)
    source_doc, source_blob = read_glb(ROOT / fit['model_file'])
    source_error = geometry_preserved(bind, data[5], data[6], source_geometry(source_doc, source_blob))
    require(np.array_equal(image_pixels(doc, blob), image_pixels(source_doc, source_blob)),
            'Source albedo pixels changed')
    for side in ('L', 'R'):
        require(np.linalg.norm(rest[node['socket_palm.' + side], :3, 3] -
                               fit['landmarks_m']['palm_contact.' + side]) < POS_TOL,
                'Palm marker does not match measured source hand ' + side)
    # Raw weapon-local marker data are checked separately from palm agreement.
    # Primary contact is deliberately offset to the outside of the solid grip:
    # these source hands are open mittens rather than closed articulated fingers.
    candidate = fit['equipment_candidate']
    fit_transform = matrix({'translation': candidate['position_m'], 'rotation': candidate['rotation_xyzw']})
    primary_local = (np.linalg.inv(fit_transform) @ np.r_[candidate['grip_results']['R']['contact_target_m'], 1])[:3]
    config = report['config']
    offsets = config.get('grip_offsets', {})
    support_local = np.asarray(weapon['sockets']['support_grip']['position_m']) + np.asarray(offsets.get('L', [0, 0, 0]))
    local_markers = {'socket_primary': primary_local, 'socket_support': support_local}
    local_markers.update({muzzle_names[s]: value['position_m'] for s, value in weapon['sockets'].items()
                          if s in muzzle_names})
    for name, local in local_markers.items():
        require(np.linalg.norm(rest[node[name], :3, 3] - local) < POS_TOL,
                'Weapon marker differs from source measurement: ' + name)
        require(parents.get(node[name]) == node['socket_weapon'], 'Weapon marker parent: ' + name)
    for side in ('L', 'R'):
        require(parents.get(node['socket_palm.' + side]) == node['hand.' + side],
                'Palm marker must follow actual hand bone ' + side)
    require(np.linalg.norm(rest[node['socket_weapon'], :3, 3]) < POS_TOL, 'Weapon origin must remain source origin')
    inverse_weapon_rest = np.linalg.inv(rest[node['socket_weapon']])
    seam_pairs = []
    first_at_position = {}
    for index, point in enumerate(data[2][:, :3]):
        key = tuple(np.round(point, 6))
        if key in first_at_position:
            seam_pairs.append((first_at_position[key], index))
        else:
            first_at_position[key] = index
    seam_pairs = np.asarray(seam_pairs, dtype=int).reshape(-1, 2)
    results, snapshots, issues = [], {}, []
    for clip in clips:
        playback = Playback(doc, blob, clip)
        frames = clip['frames']
        floor, mid_floor, grip, mid_grip, extent, seam_gap, marker_error = 1e9, 1e9, 0, 0, 0, 0, 0
        motion = 0
        first_pose = last_pose = first_world = last_world = None
        for frame in range(2 * (frames - 1) + 1):
            world = playback.at(frame / (2 * FPS))
            posed = deform(world, data)
            require(np.isfinite(posed).all(), 'Nonfinite skinned position')
            if frame % 2:
                mid_floor = min(mid_floor, float(posed[:, 1].min()))
            else:
                floor = min(floor, float(posed[:, 1].min()))
            extent = max(extent, float(np.ptp(posed, axis=0).max()))
            if len(seam_pairs):
                seam_gap = max(seam_gap, float(np.linalg.norm(
                    posed[seam_pairs[:, 0]] - posed[seam_pairs[:, 1]], axis=1).max()))
            error = max(np.linalg.norm(world[node['socket_palm.' + side], :3, 3] -
                                      world[node[socket], :3, 3])
                        for side, socket in (('R', 'socket_primary'), ('L', 'socket_support')))
            if frame % 2:
                mid_grip = max(mid_grip, float(error))
            else:
                grip = max(grip, float(error))
            for name, local in local_markers.items():
                expected = world[node['socket_weapon']] @ inverse_weapon_rest @ np.r_[local, 1]
                marker_error = max(marker_error, float(np.linalg.norm(world[node[name], :3, 3] - expected[:3])))
            if first_pose is None:
                first_pose, first_world = posed, world
            motion = max(motion, float(np.linalg.norm(posed - first_pose, axis=1).max()))
            last_pose, last_world = posed, world
        def check(value, message):
            if not value:
                issues.append(clip['name'] + ': ' + message)

        check(floor >= -3e-5, f'baked mesh below floor {floor:.7f}m')
        check(mid_floor >= -3e-5, f'subframe mesh below floor {mid_floor:.7f}m')
        check(extent < 3 * fit['bounds_m'][1][1], 'Unbounded skin deformation')
        check(seam_gap < 3e-5, f'UV seam opens {seam_gap:.7f}m')
        check(grip < GRIP_TOL, f'baked palm contact gap {grip:.7f}m')
        # Subframe two-hand IK is baked, so a bounded residual is inevitable.
        check(mid_grip < .002, f'subframe palm contact gap {mid_grip:.7f}m')
        check(marker_error < POS_TOL, 'Weapon marker drifts from rigid source datum')
        if clip['name'] in ('walk', 'run', 'fire', 'aim_enter', 'aim_exit', 'beam_lift', 'beam_release'):
            check(motion > .003, 'Expected movement clip is effectively static')
        loop_error = float(np.linalg.norm(first_pose - last_pose, axis=1).max())
        socket_loop = max(float(np.linalg.norm(first_world[node[n]] - last_world[node[n]])) for n in required)
        if clip['loop']:
            check(loop_error < POS_TOL and socket_loop < POS_TOL, 'Loop seam')
        snapshots[clip['name']] = (first_pose, last_pose, first_world, last_world)
        results.append({'name': clip['name'], 'key_frames': frames, 'subframe_samples': frames - 1,
                        'floor_min_m': floor, 'subframe_floor_min_m': mid_floor,
                        'baked_grip_gap_m': grip, 'subframe_grip_gap_m': mid_grip,
                        'rigid_marker_error_m': marker_error, 'uv_seam_gap_m': seam_gap,
                        'max_motion_from_first_frame_m': motion,
                        'loop_mesh_error_m': loop_error if clip['loop'] else None,
                        'loop_socket_matrix_error': socket_loop if clip['loop'] else None})
    transitions = {}
    for left, right in (('beam_lift', 'beam_struggle'), ('beam_struggle', 'beam_release'),
                        ('aim_enter', 'aim'), ('aim', 'fire'), ('fire', 'aim'), ('aim', 'aim_exit')):
        a, b = snapshots[left], snapshots[right]
        mesh_error = float(np.linalg.norm(a[1] - b[0], axis=1).max())
        socket_error = max(float(np.linalg.norm(a[3][node[n]] - b[2][node[n]])) for n in required)
        if mesh_error >= POS_TOL or socket_error >= POS_TOL:
            issues.append(f'Transition discontinuity {left}->{right}: mesh {mesh_error}, socket {socket_error}')
        transitions[left + '->' + right] = {'mesh_error_m': mesh_error, 'socket_matrix_error': socket_error}
    return {'id': identity, 'status': 'failed' if issues else 'passed', 'issues': issues,
            'sha256': digest(path), 'bytes': path.stat().st_size,
            'nodes': len(names), 'joints': len(data[0]), 'vertices': len(bind),
            'triangles': len(data[6]) // 3, 'source_bind_max_error_m': source_error,
            'albedo_pixels_preserved': True, 'clips': results, 'transitions': transitions}


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--only', action='append', help='Validate one body ID (repeatable)')
    parser.add_argument('--output-root',type=Path,default=OUT)
    args = parser.parse_args()
    OUT=args.output_root.resolve()
    from transaction_character_animation import locked_output
    with locked_output(OUT):
        return validate(args.only)


def validate(only=None):
    fit = json.loads(FIT.read_text())
    weapons = {w['id']: w for w in json.loads(SOCKETS.read_text())['weapons']}
    bodies = [b for b in fit['bodies'] if not only or b['id'] in only]
    require(bodies and (not only or set(only) <= {b['id'] for b in bodies}), 'Unknown --only ID')
    results = []
    for body in bodies:
        try:
            result = check_character(body, weapons)
        except (ValueError, KeyError, IndexError, OSError, StopIteration) as error:
            result = {'id': body['id'], 'status': 'failed', 'error': str(error)}
        results.append(result)
        print(json.dumps(result if result['status'] == 'failed' else
                         {k: v for k, v in result.items() if k not in ('clips', 'transitions')}), flush=True)
    output = {'status': 'passed' if all(r['status'] == 'passed' for r in results) else 'failed',
              'scope': 'Independent source preservation, 30Hz keys and 60Hz interpolated GLB skin/socket playback',
              'limits': 'Technical gate only; no collision, closed-finger anatomy, motion-art or native-runtime acceptance.',
              'validator_sha256': digest(Path(__file__)), 'fit_sha256': digest(FIT),
              'weapon_sockets_sha256': digest(SOCKETS), 'characters': results}
    target = OUT / 'reports' / 'infantry-validation.json'
    target.parent.mkdir(parents=True, exist_ok=True)
    from transaction_character_animation import atomic_bytes
    atomic_bytes(target,(json.dumps(output,indent=2)+'\n').encode())
    raise SystemExit(0 if output['status'] == 'passed' else 1)


if __name__ == '__main__':
    main()
