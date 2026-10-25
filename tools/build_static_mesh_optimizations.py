"""Replay the two approved static mesh edits; invoked by static_mesh_optimizations.py.

Blender writes geometry only. Pinned art Python assembles it into unchanged
original GLB scene/image/material data and checks the reviewed output hashes.
"""
from pathlib import Path
import argparse
import collections
import copy
import hashlib
import json
import math
import struct
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--assemble', action='store_true')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else None)
ROOT = args.root.resolve()
sys.path.insert(0, str(ROOT / 'tools'))
from static_mesh_optimizations import checked_recipe, digest
recipe = checked_recipe(ROOT)
root = args.output.resolve()
if not root.is_relative_to((ROOT / 'build').resolve()):
    raise ValueError('Replay output must stay inside ignored build/')


def blender_geometry():
    import bpy
    def read_glb(path):
        raw=path.read_bytes();offset=12;chunks={}
        while offset<len(raw):
            size,kind=struct.unpack_from('<II',raw,offset);offset+=8
            chunks[kind]=raw[offset:offset+size];offset+=size
        return json.loads(chunks[0x4e4f534a]),chunks[0x004e4942]
    def accessor(doc,blob,index):
        a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
        width={'SCALAR':1,'VEC2':2,'VEC3':3}[a['type']]
        kind={5123:'H',5125:'I',5126:'f'}[a['componentType']]
        stride=v.get('byteStride',struct.calcsize('<'+kind)*width)
        offset=v.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from('<'+kind*width,blob,offset+n*stride) for n in range(a['count'])]

    assert bpy.app.version_string.startswith('4.5.15')
    report=[]
    for asset in ['crop-seedling','grass-tuft']:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        doc,blob=read_glb(ROOT/'assets/models'/asset/(asset+'.glb'))
        assert not doc.get('skins') and not doc.get('animations')
        assert len(doc['meshes'])==1 and len(doc['meshes'][0]['primitives'])==1
        primitive=doc['meshes'][0]['primitives'][0]
        positions=accessor(doc,blob,primitive['attributes']['POSITION'])
        normals=accessor(doc,blob,primitive['attributes']['NORMAL'])
        uv=accessor(doc,blob,primitive['attributes']['TEXCOORD_0'])
        indices=[x[0] for x in accessor(doc,blob,primitive['indices'])]
        faces=[indices[n:n+3] for n in range(0,len(indices),3)]
        # Weld only exact source positions; all attributes remain per-corner.
        position_map={};welded_positions=[];remap=[]
        for position in positions:
            if position not in position_map:position_map[position]=len(welded_positions);welded_positions.append(position)
            remap.append(position_map[position])
        welded_faces=[[remap[i] for i in face] for face in faces]
        assert all(len(set(face))==3 for face in welded_faces)
        mesh=bpy.data.meshes.new(asset);mesh.from_pydata(welded_positions,[],welded_faces);mesh.update()
        assert len(mesh.loops)==len(indices)
        assert all(tuple(mesh.vertices[loop.vertex_index].co)==positions[indices[loop.index]] for loop in mesh.loops)
        assert all(polygon.material_index==0 for polygon in mesh.polygons)
        layer=mesh.uv_layers.new(name='OriginalUV')
        for loop in mesh.loops:layer.data[loop.index].uv=uv[indices[loop.index]]
        assert all(tuple(layer.data[loop.index].uv)==uv[indices[loop.index]] for loop in mesh.loops)
        for polygon in mesh.polygons:polygon.use_smooth=True
        mesh.normals_split_custom_set([normals[indices[loop.index]] for loop in mesh.loops])
        # Named corner data certifies that exact source custom normals were not
        # averaged merely because positions now share a geometric vertex.
        normal_layer=mesh.attributes.new('OriginalCornerNormal','FLOAT_VECTOR','CORNER')
        for loop in mesh.loops:normal_layer.data[loop.index].vector=normals[indices[loop.index]]
        assert all(tuple(normal_layer.data[loop.index].vector)==normals[indices[loop.index]] for loop in mesh.loops)
        obj=bpy.data.objects.new(asset,mesh);bpy.context.collection.objects.link(obj)
        bpy.context.view_layer.objects.active=obj;obj.select_set(True)
        source_error=max(abs(mesh.corner_normals[loop.index].vector[k]-normals[indices[loop.index]][k]) for loop in mesh.loops for k in range(3))
        assert source_error<=.001,(asset,source_error)
        modifier=obj.modifiers.new('ReviewedWeldedSingle25PercentCandidate','DECIMATE')
        modifier.decimate_type='COLLAPSE';modifier.ratio=.25;modifier.use_collapse_triangulate=True
        evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        candidate=evaluated.to_mesh(preserve_all_data_layers=True,depsgraph=bpy.context.evaluated_depsgraph_get())
        candidate.calc_loop_triangles();layer=candidate.uv_layers.active
        assert layer is not None
        data=[];new_indices=[];lookup={}
        for triangle in candidate.loop_triangles:
            for index in triangle.loops:
                loop=candidate.loops[index]
                values=tuple(candidate.vertices[loop.vertex_index].co)+tuple(candidate.corner_normals[index].vector)+tuple(layer.data[index].uv)
                assert all(math.isfinite(x) for x in values)
                key=struct.pack('<8f',*values)
                if key not in lookup:lookup[key]=len(data);data.append(values)
                new_indices.append(lookup[key])
        dest=root/asset;dest.mkdir(parents=True,exist_ok=True)
        (dest/'geometry.json').write_text(json.dumps({'vertices':data,'indices':new_indices},separators=(',',':'))+'\n')
        report.append({'asset':asset,'source_triangles':len(indices)//3,'candidate_triangles':len(new_indices)//3,'source_vertices':len(positions),'welded_input_vertices':len(welded_positions),'weld_only_corner_positions_uv_material_exact':True,'source_corner_normals_stored_exact':True,'effective_weld_only_normal_max_abs_error':source_error,'candidate_vertices':len(data),'ratio_requested':.25,'ratio_actual':len(new_indices)/len(indices),'UV_layer':layer.name,'modifier':'COLLAPSE, triangulated, one candidate; exact geometry weld first, original UV/custom normals retained per corner','images_materials':'not imported/exported; original GLB bytes retained separately'})
        evaluated.to_mesh_clear()
    (root/'replay-blender.json').write_text(json.dumps({'blender':bpy.app.version_string,'assets':report},indent=2)+'\n')
    print(json.dumps(report,indent=2))

def components(adjacency):
    unseen = set(adjacency)
    result = []
    while unseen:
        todo = [unseen.pop()]
        part = []
        while todo:
            node = todo.pop()
            part.append(node)
            for neighbor in adjacency[node]:
                if neighbor in unseen:
                    unseen.remove(neighbor)
                    todo.append(neighbor)
        result.append(part)
    return result


def topology(positions, indices):
    lookup, remap = {}, []
    for position in positions:
        key = tuple(position)
        if key not in lookup:
            lookup[key] = len(lookup)
        remap.append(lookup[key])
    edges, adjacency = collections.Counter(), collections.defaultdict(set)
    degenerate = 0
    for start in range(0, len(indices), 3):
        ids = [remap[int(index)] for index in indices[start:start + 3]]
        degenerate += len(set(ids)) < 3
        for a, b in zip(ids, ids[1:] + ids[:1]):
            edges[tuple(sorted((a, b)))] += 1
            adjacency[a].add(b)
            adjacency[b].add(a)
    boundary = collections.defaultdict(set)
    for (a, b), count in edges.items():
        if count == 1:
            boundary[a].add(b)
            boundary[b].add(a)
    return {'components': len(components(adjacency)),
            'boundary_loops': len(components(boundary)),
            'boundary_degree_two': all(len(neighbors) == 2 for neighbors in boundary.values()),
            'nonmanifold_incidence': sorted(count for count in edges.values() if count > 2),
            'degenerate_triangles': degenerate}


def assemble():
    import numpy as np
    from normalize_glb import unpack, pack, accessor
    if np.__version__ != '2.3.5':
        raise ValueError('Replay assembly requires existing pinned NumPy2.3.5')

    def append(doc, blob, values, kind, width):
        values = np.asarray(values, dtype='<f4' if kind == 5126 else '<u4').reshape(-1, width)
        blob += b'\0' * (-len(blob) % 4)
        view = len(doc['bufferViews'])
        doc['bufferViews'].append({'buffer': 0, 'byteOffset': len(blob), 'byteLength': values.nbytes,
                                  'target': 34963 if width == 1 else 34962})
        blob += values.tobytes()
        index = len(doc['accessors'])
        item = {'bufferView': view, 'componentType': kind, 'count': len(values),
                'type': {1: 'SCALAR', 2: 'VEC2', 3: 'VEC3'}[width]}
        if width == 3:
            item.update(min=values.min(axis=0).tolist(), max=values.max(axis=0).tolist())
        doc['accessors'].append(item)
        return blob, index

    report = []
    for row in recipe['models']:
        asset = row['id']
        doc, chunks = unpack(ROOT / row['source'])
        old = copy.deepcopy(doc)
        blob = next(data for kind, data in chunks if kind == 0x004e4942)
        old_blob = blob
        primitive = old['meshes'][0]['primitives'][0]
        positions = accessor(old, blob, primitive['attributes']['POSITION'])
        original_indices = accessor(old, blob, primitive['indices']).reshape(-1)
        geometry = json.loads((root / asset / 'geometry.json').read_text())
        vertices = np.asarray(geometry['vertices'], dtype=np.float32)
        indices = np.asarray(geometry['indices'], dtype=np.uint32)
        if (vertices.ndim != 2 or vertices.shape[1] != 8 or not np.isfinite(vertices).all() or
                not len(indices) or len(indices) % 3 or int(indices.max()) >= len(vertices)):
            raise ValueError('Invalid static mesh replay geometry: ' + asset)
        original_min, original_max = positions.min(axis=0), positions.max(axis=0)
        raw_min, raw_max = vertices[:, :3].min(axis=0), vertices[:, :3].max(axis=0)
        axis_scale = (original_max - original_min) / (raw_max - raw_min)
        # This exact reviewed small affine correction keeps authored culling
        # bounds; it does not establish shape equivalence or art acceptance.
        vertices[:, :3] = (vertices[:, :3] - raw_min) * axis_scale + original_min
        vertices[:, 3:6] /= axis_scale
        vertices[:, 3:6] /= np.linalg.norm(vertices[:, 3:6], axis=1, keepdims=True)
        normal_lengths = np.linalg.norm(vertices[:, 3:6], axis=1)
        if (not np.isfinite(vertices).all() or not np.all(np.abs(normal_lengths - 1) < .001) or
                vertices[:, 6:8].min() < -.001 or vertices[:, 6:8].max() > 1.001):
            raise ValueError('Invalid replay normals or UVs: ' + asset)
        before = topology(positions, original_indices)
        after = topology(vertices[:, :3], indices)
        if before != row['topology'] or after != before or len(indices) // 3 != row['triangles_after']:
            raise ValueError('Static mesh topology/count differs from approved export: ' + asset)
        attributes = {}
        for name, values, width in [('POSITION', vertices[:, :3], 3), ('NORMAL', vertices[:, 3:6], 3),
                                    ('TEXCOORD_0', vertices[:, 6:8], 2)]:
            blob, attributes[name] = append(doc, blob, values, 5126, width)
        blob, index = append(doc, blob, indices, 5125, 1)
        doc['meshes'][0]['primitives'][0]['attributes'] = attributes
        doc['meshes'][0]['primitives'][0]['indices'] = index
        doc['buffers'][0]['byteLength'] = len(blob)
        destination = root / asset / (asset + '-runtime.glb')
        pack(doc, [(kind, blob if kind == 0x004e4942 else value) for kind, value in chunks], destination)
        for key in ('nodes', 'scenes', 'scene', 'materials', 'images', 'textures', 'samplers'):
            if doc.get(key) != old.get(key):
                raise ValueError('Replay changed original scene/material/image contract')
        if blob[:len(old_blob)] != old_blob or digest(destination) != row['runtime_source_sha256']:
            raise ValueError('Replay differs from reviewed source; never publish automatically: ' + asset)
        report.append({'id': asset, 'source_sha256': digest(ROOT / row['source']),
                       'output_sha256': digest(destination), 'triangles': len(indices) // 3,
                       'topology': after, 'bounds_axis_scale': axis_scale.tolist()})
    (root / 'replay-verified.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Exact reviewed crop/grass GLB replay hashes match')


if args.assemble:
    assemble()
else:
    blender_geometry()
