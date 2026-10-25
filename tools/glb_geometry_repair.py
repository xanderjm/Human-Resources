"""Small authoring helpers for controlled additions to uncompressed Tripo GLBs."""
import hashlib
import json
from pathlib import Path

import numpy as np

from normalize_glb import ROOT, accessor, pack, unpack
from audit_glb import inspect


def load_asset(asset):
    record = ROOT / 'assets/production/records' / asset
    audit_path = record / 'model-audit.json'
    audit = json.loads(audit_path.read_text())
    raw = ROOT / audit.get('generation_raw_model_file', audit.get('raw_model_file', audit['model_file']))
    rejected = record / 'rejected-raw-model-audit.json'
    if not rejected.exists():
        rejected.write_text(json.dumps(audit, indent=2) + '\n')
    doc, chunks = unpack(raw)
    return record, audit, raw, doc, chunks


def array_from(doc, chunks, index):
    return accessor(doc, next(blob for typ, blob in chunks if typ == 0x004e4942), index)


def append_array(doc, chunks, values, component_type, kind, target):
    values = np.ascontiguousarray(values, dtype='<f4' if component_type == 5126 else '<u4')
    blob = next(blob for typ, blob in chunks if typ == 0x004e4942)
    blob += b'\0' * (-len(blob) % 4)
    view = len(doc['bufferViews'])
    doc['bufferViews'].append({'buffer': 0, 'byteOffset': len(blob), 'byteLength': values.nbytes, 'target': target})
    blob += values.tobytes()
    chunks[:] = [(typ, blob if typ == 0x004e4942 else value) for typ, value in chunks]
    doc['buffers'][0]['byteLength'] = len(blob)
    index = len(doc['accessors'])
    data = {'bufferView': view, 'componentType': component_type, 'count': len(values), 'type': kind}
    if kind == 'VEC3':
        data.update(min=values.min(axis=0).tolist(), max=values.max(axis=0).tolist())
    doc['accessors'].append(data)
    return index


def sample_uv_triangle(doc, chunks, target_position):
    primitive = doc['meshes'][0]['primitives'][0]
    positions = array_from(doc, chunks, primitive['attributes']['POSITION'])
    uv = array_from(doc, chunks, primitive['attributes']['TEXCOORD_0'])
    triangles = array_from(doc, chunks, primitive['indices']).reshape(-1, 3)
    centers = positions[triangles].mean(axis=1)
    index = np.argmin(np.linalg.norm(centers - np.asarray(target_position), axis=1))
    return uv[triangles[index]]


def add_hexahedron(doc, chunks, corners, uv):
    corners = np.asarray(corners, dtype=float)
    faces = ((0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0))
    positions, normals, texture = [], [], []
    centre = corners.mean(axis=0)
    for face in faces:
        q = corners[list(face)]
        normal = np.cross(q[1] - q[0], q[2] - q[0])
        if np.dot(normal, q.mean(axis=0) - centre) < 0:
            q = q[::-1]
            normal = -normal
        normal /= np.linalg.norm(normal)
        for triangle in ((0, 1, 2), (0, 2, 3)):
            positions.extend(q[list(triangle)])
            normals.extend([normal] * 3)
            texture.extend(uv)
    attributes = {'POSITION': append_array(doc, chunks, positions, 5126, 'VEC3', 34962),
                  'NORMAL': append_array(doc, chunks, normals, 5126, 'VEC3', 34962),
                  'TEXCOORD_0': append_array(doc, chunks, texture, 5126, 'VEC2', 34962)}
    indices = append_array(doc, chunks, np.arange(36).reshape(-1, 1), 5125, 'SCALAR', 34963)
    material = doc['meshes'][0]['primitives'][0].get('material', 0)
    doc['meshes'][0]['primitives'].append({'attributes': attributes, 'indices': indices, 'material': material, 'mode': 4})


def merge_same_material_primitives(doc, chunks):
    primitives = doc['meshes'][0]['primitives']
    if len({p.get('material', 0) for p in primitives}) != 1:
        raise ValueError('Controlled repair expects one original material')
    positions, normals, uv, indices = [], [], [], []
    offset = 0
    for p in primitives:
        pos = array_from(doc, chunks, p['attributes']['POSITION'])
        positions.extend(pos)
        normals.extend(array_from(doc, chunks, p['attributes']['NORMAL']))
        uv.extend(array_from(doc, chunks, p['attributes']['TEXCOORD_0']))
        indices.extend(array_from(doc, chunks, p['indices']).reshape(-1) + offset)
        offset += len(pos)
    attrs = {'POSITION': append_array(doc, chunks, positions, 5126, 'VEC3', 34962),
             'NORMAL': append_array(doc, chunks, normals, 5126, 'VEC3', 34962),
             'TEXCOORD_0': append_array(doc, chunks, uv, 5126, 'VEC2', 34962)}
    index = append_array(doc, chunks, np.asarray(indices).reshape(-1, 1), 5125, 'SCALAR', 34963)
    doc['meshes'][0]['primitives'] = [{'attributes': attrs, 'indices': index,
        'material': primitives[0].get('material', 0), 'mode': 4}]


def save_repair(asset, record, audit, raw, doc, chunks, description, details):
    output = ROOT / 'assets/models' / asset / f'{asset}-repaired-source.glb'
    pack(doc, chunks, output)
    audit.update(inspect(output))
    audit['triangle_budget_review'].update(actual_triangles=audit['triangles'],
        target_met=audit['triangles'] <= audit['requested_face_limit'],
        status='passed' if audit['triangles'] <= 3000 else 'failed')
    audit['generation_raw_model_file'] = str(raw.relative_to(ROOT))
    audit['raw_model_file'] = str(output.relative_to(ROOT))
    audit['model_file'] = str(output.relative_to(ROOT))
    audit['visual_review'] = {'status': 'pending', 'notes': description}
    audit['import_review'] = {'status': 'pending'}
    repair = {'asset': asset, 'description': description, 'source': str(raw.relative_to(ROOT)),
              'source_sha256': hashlib.sha256(raw.read_bytes()).hexdigest(), 'repaired_source': str(output.relative_to(ROOT)),
              'repaired_source_sha256': hashlib.sha256(output.read_bytes()).hexdigest(), 'details': details,
              'material_policy': 'Original embedded base-colour and material retained; no PBR maps, no retexture.'}
    (record / 'geometry-repair.json').write_text(json.dumps(repair, indent=2) + '\n')
    audit['geometry_repair'] = repair
    (record / 'model-audit.json').write_text(json.dumps(audit, indent=2) + '\n')
    return output
