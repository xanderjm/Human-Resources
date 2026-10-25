"""Independently decode and skin every exported animation frame with NumPy.

This does not use Blender, its deformation result, or its action data.
Validates delivered GLBs rather than trusting successful authoring/export.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import sys
import numpy as np
from normalize_glb import accessor, matrix, points, unpack

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/animation'


def require(value, message):
    if not value:
        raise ValueError(message)


def sampled_world(doc, blob, animation, time):
    nodes = [dict(n) for n in doc['nodes']]
    for channel in animation['channels']:
        sampler = animation['samplers'][channel['sampler']]
        require(sampler.get('interpolation', 'LINEAR') == 'LINEAR', 'Unexpected interpolation')
        times = accessor(doc, blob, sampler['input']).ravel()
        values = accessor(doc, blob, sampler['output'])
        require(np.isfinite(values).all() and np.isfinite(times).all(), 'Nonfinite animation')
        require(np.all(np.diff(times) > 0), 'Unordered sampler')
        index = min(len(times)-1, max(0, np.searchsorted(times, time, side='right')-1))
        # Verification samples all authored 30Hz keys; roundoff may lie just before one.
        if index+1 < len(times) and abs(times[index+1]-time) < 1e-5:
            index += 1
        value = values[index].astype(float)
        if channel['target']['path'] == 'rotation':
            require(abs(np.linalg.norm(value)-1) < 2e-4, 'Nonunit quaternion')
        target = nodes[channel['target']['node']]
        target.pop('matrix', None)
        target[channel['target']['path']] = value
    world = [None]*len(nodes)
    def visit(index, parent):
        require(world[index] is None, 'Cyclic or multiply parented skeleton')
        world[index] = parent @ matrix(nodes[index])
        for child in nodes[index].get('children', []):
            visit(child, world[index])
    for root in doc['scenes'][doc.get('scene', 0)]['nodes']:
        visit(root, np.eye(4))
    return world


def load_skin(doc, blob):
    primitives = []
    require(len(doc['skins']) == 1, 'Expected one complete skin')
    skin = doc['skins'][0]
    inverse = accessor(doc, blob, skin['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
    require(np.isfinite(inverse).all(), 'Invalid inverse bind matrices')
    for node in doc['nodes']:
        if 'mesh' not in node:
            continue
        require(node.get('skin') == 0, 'Unskinned mesh')
        for primitive in doc['meshes'][node['mesh']]['primitives']:
            attr = primitive['attributes']
            require({'POSITION','NORMAL','TEXCOORD_0','JOINTS_0','WEIGHTS_0'} <= attr.keys(),
                    'Incomplete skinned attributes')
            pos = accessor(doc, blob, attr['POSITION'])
            weights = accessor(doc, blob, attr['WEIGHTS_0'])
            joints = accessor(doc, blob, attr['JOINTS_0']).astype(int)
            indices = accessor(doc, blob, primitive['indices']).ravel().astype(int)
            require(np.isfinite(pos).all() and np.isfinite(weights).all(), 'Nonfinite skin')
            require(np.all(weights >= 0) and np.allclose(weights.sum(1), 1, atol=2e-5),
                    'Invalid normalized weights')
            require(joints.min() >= 0 and joints.max() < len(skin['joints']), 'Invalid joint index')
            require(len(indices)%3 == 0 and indices.max() < len(pos), 'Invalid triangles')
            primitives.append((np.column_stack([pos, np.ones(len(pos))]), weights, joints, indices))
    return skin['joints'], inverse, primitives


def deform(world, joints, inverse, primitives):
    matrices = np.stack([world[j] for j in joints]) @ inverse
    output = []
    for pos, weights, influences, indices in primitives:
        result = np.zeros_like(pos)
        for i in range(4):
            result += np.einsum('nij,nj->ni', matrices[influences[:,i]], pos)*weights[:,i,None]
        output.append(result[:,:3])
    return np.concatenate(output)


def check_character(c, spec):
    path = OUT/'exports'/(c['id']+'.glb')
    doc, chunks = unpack(path)
    blob = next(b for t,b in chunks if t == 0x004e4942)
    report = json.loads((OUT/'reports'/(c['id']+'.json')).read_text())
    source = ROOT/report['source']
    require('authoring_inputs' in report, 'Missing authoring provenance: rebuild '+c['id'])
    for name, digest in report['authoring_inputs'].items():
        require(hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == digest,
                'Stale export; authoring input changed: '+name)
    require(hashlib.sha256(source.read_bytes()).hexdigest() == report['source_sha256'],
            'Source changed since build')
    require(len(doc['animations']) == len(spec['clips']), 'Missing/extra animations')
    joints, inverse, primitives = load_skin(doc, blob)
    seam_first = {}
    seam_pairs = []
    positions = np.concatenate([p[0][:,:3] for p in primitives])
    for index, point in enumerate(positions):
        key = tuple(np.round(point, 6))
        if key in seam_first:
            seam_pairs.append((seam_first[key], index))
        else:
            seam_first[key] = index
    seam_pairs = np.asarray(seam_pairs, dtype=int).reshape(-1, 2)
    require(sum(len(p[3])//3 for p in primitives) == report['triangles'], 'Topology changed')
    require(all('bufferView' in im for im in doc['images']), 'External texture dependency')
    # Reconstructed bind pose must match the original asset in metres and orientation.
    bind = deform(sampled_world(doc, blob, {'channels':[]}, 0), joints, inverse, primitives)
    original, _ = points(*unpack(source))
    require(np.allclose(bind.min(0), original.min(0), atol=3e-5) and
            np.allclose(bind.max(0), original.max(0), atol=3e-5), 'Rest bounds differ from source')
    # Float32 skin matrices may move a coordinate across a decimal rounding cell.
    # Check actual distance for those unmatched cells rather than exact tuple equality.
    bind_cells = set(map(tuple, np.round(bind, 4)))
    source_cells = set(map(tuple, np.round(original, 4)))
    for values, other, missing in [(bind, original, bind_cells-source_cells),
                                    (original, bind, source_cells-bind_cells)]:
        for value in values:
            if tuple(np.round(value, 4)) in missing:
                require(np.linalg.norm(other-value, axis=1).min() < 3e-5,
                        c['id']+': rest vertex differs from source')
    animation_results = []
    snapshots = {}
    for record in report['clips']:
        animation = next(a for a in doc['animations'] if a['name'] == record['name'])
        frames = record['frames']
        first, last = None, None
        floor, maximum_motion, max_extent = 1e9, 0, 0
        for i in range(frames):
            world = sampled_world(doc, blob, animation, i/spec['fps'])
            posed = deform(world, joints, inverse, primitives)
            require(np.isfinite(posed).all(), 'Nonfinite deformed geometry')
            if len(seam_pairs):
                gap = np.linalg.norm(posed[seam_pairs[:,0]]-posed[seam_pairs[:,1]], axis=1).max()
                require(gap < 2e-5, c['id']+': UV seam tears during '+record['name'])
            floor = min(floor, float(posed[:,1].min()))
            maximum_motion = max(maximum_motion, float(np.linalg.norm(posed-bind, axis=1).max()))
            max_extent = max(max_extent, float(np.ptp(posed, axis=0).max()))
            if first is None:
                first = posed
            last = posed
        seam = float(np.linalg.norm(first-last, axis=1).max())
        require(floor >= -3e-5, f'{c["id"]}/{record["name"]}: below ground {floor}')
        require(maximum_motion > .003, 'Animation contains no visible deformation')
        require(max_extent < c['height']*3, 'Unbounded deformation')
        if record['loop']:
            require(seam < 3e-5, 'Loop seam '+record['name'])
        snapshots[record['name']] = (first, last)
        animation_results.append({'name':record['name'], 'sampled_frames':frames,
                                  'floor_min_m':round(floor,7), 'maximum_motion_m':round(maximum_motion,5),
                                  'loop_seam_m':round(seam,7) if record['loop'] else None})
    transitions = {}
    for a,b in [('beam_lift','beam_struggle'),('beam_struggle','beam_release')]:
        error = float(np.linalg.norm(snapshots[a][1]-snapshots[b][0], axis=1).max())
        require(error < 3e-5, f'Beam transition mismatch: {a}->{b}: {error}')
        transitions[a+'->'+b] = error
    return {'id':c['id'], 'status':'passed', 'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'bytes':path.stat().st_size, 'joints':len(joints),
            'weighted_vertices':sum(len(p[0]) for p in primitives),
            'triangles':report['triangles'], 'clips':animation_results,
            'beam_transition_max_error_m':transitions}


def validate(output=None, only=None):
    global OUT
    if output is not None:OUT=Path(output)
    spec = json.loads((ROOT/'assets/animation/characters.json').read_text())
    results = [check_character(c, spec) for c in spec['characters'] if only is None or c['id']==only]
    result = {'status':'passed', 'validation':'Independent glTF linear skinning at every authored frame',
              'characters':results, 'clip_count':sum(len(c['clips']) for c in results),
              'sampled_frames':sum(a['sampled_frames'] for c in results for a in c['clips'])}
    from transaction_character_animation import atomic_bytes
    atomic_bytes(OUT/'reports/validation.json',(json.dumps(result,indent=2)+'\n').encode())
    print(json.dumps({k:v for k,v in result.items() if k != 'characters'}, indent=2))


def main():
    import argparse
    from transaction_character_animation import locked_output
    parser=argparse.ArgumentParser()
    parser.add_argument('--output-root',type=Path,default=OUT)
    parser.add_argument('--only')
    args=parser.parse_args()
    with locked_output(args.output_root):validate(args.output_root,args.only)


if __name__=='__main__':main()
