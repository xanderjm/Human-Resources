"""Run in Blender: blender --background --python tools/import_ufo_modular_blender.py.

Produces an editable quad .blend from the same source as the runtime GLBs.
Use -- --upgraded for the fitted upgrade assembly. No glTF triangulation roundtrip.
"""
import json
import math
import sys
from pathlib import Path

import bpy

ROOT = Path(__file__).resolve().parents[1]
ASSET = ROOT/'assets/models/ufo-modular'
data = json.loads((ASSET/'ufo-source.json').read_text())
upgraded = '--upgraded' in sys.argv
name = 'UFO_Modular_Upgraded' if upgraded else 'UFO_Modular'
scene = bpy.data.scenes.new(name)
bpy.context.window.scene = scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1

materials=[]
for source in data['materials']:
    mat=bpy.data.materials.new(source['name']);mat.use_nodes=True
    shader=mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Metallic'].default_value=source['metallic']
    shader.inputs['Roughness'].default_value=source['roughness']
    shader.inputs['Base Color'].default_value=source['color']
    if 'clearcoat' in source:
        shader.inputs['Coat Weight'].default_value=source['clearcoat']
        shader.inputs['Coat Roughness'].default_value=.3
    if 'texture' in source:
        tex=mat.node_tree.nodes.new('ShaderNodeTexImage')
        tex.image=bpy.data.images.load(str(ASSET/'materials'/source['texture']),check_existing=True)
        tex.image.pack()
        tint=mat.node_tree.nodes.new('ShaderNodeMixRGB')
        tint.blend_type='MULTIPLY';tint.inputs[0].default_value=1
        tint.inputs[2].default_value=source['color']
        mat.node_tree.links.new(tex.outputs['Color'],tint.inputs[1])
        mat.node_tree.links.new(tint.outputs['Color'],shader.inputs['Base Color'])
    if 'emissive' in source:
        shader.inputs['Emission Strength'].default_value=1
        emission=mat.node_tree.nodes.new('ShaderNodeMixRGB')
        emission.blend_type='MULTIPLY';emission.inputs[0].default_value=1
        emission.inputs[2].default_value=source['emissive']+[1]
        mat.node_tree.links.new(tex.outputs['Color'],emission.inputs[1])
        mat.node_tree.links.new(emission.outputs['Color'],shader.inputs['Emission Color'])
    materials.append(mat)

# Convert game +Y-up to Blender +Z-up while keeping all child transforms local.
conversion = bpy.data.objects.new('Game_Coordinates_Y_Up',None)
scene.collection.objects.link(conversion)
conversion.rotation_euler = (math.pi/2,0,0)
meshes = {}
for key,source in data['meshes'].items():
    mesh = bpy.data.meshes.new(key+'_Quads')
    mesh.from_pydata(source['vertices'],[],source['faces'])
    mesh.update()
    for mat in materials:mesh.materials.append(mat)
    uv = mesh.uv_layers.new(name='Paint_UV')
    for face,coords,finish in zip(mesh.polygons,source['uvs'],source['finishes']):
        face.material_index=finish
        face.use_smooth=source['smooth']
        for loop,(u,v) in zip(face.loop_indices,coords):uv.data[loop].uv=(u,1-v)
    meshes[key]=mesh
nodes=data['upgraded_nodes' if upgraded else 'base_nodes']
objects=[]
for n in nodes:
    obj=bpy.data.objects.new(n['name'],meshes.get(n.get('mesh_name')))
    scene.collection.objects.link(obj)
    obj.location=n.get('translation',(0,0,0))
    if 'rotation' in n:
        x,y,z,w=n['rotation'];obj.rotation_mode='QUATERNION'
        obj.rotation_quaternion=(w,x,y,z)
    for k,v in n.get('extras',{}).items():obj[k]=v
    if n['name'].startswith('Socket_'):
        obj.empty_display_type='ARROWS';obj.empty_display_size=.08
    objects.append(obj)
for i,n in enumerate(nodes):
    for j in n.get('children',[]):objects[j].parent=objects[i]
objects[0].parent=conversion

rotor=next(o for o in objects if o.name=='Outer_Ring_Pivot')
rotor.rotation_mode='XYZ'
for frame,angle in [(1,0),(193,math.tau)]:
    rotor.rotation_euler.y=angle;rotor.keyframe_insert(data_path='rotation_euler',frame=frame,index=1)
if rotor.animation_data:
    action=rotor.animation_data.action
    # Blender versions differ in action channel storage; the model remains editable.
    if hasattr(action,'fcurves'):
        for curve in action.fcurves:
            for key in curve.keyframe_points:key.interpolation='LINEAR'
scene.frame_end=192
scene.render.fps=24
scene.frame_set(1)
for o in bpy.context.selected_objects:o.select_set(False)
rotor.select_set(True);bpy.context.view_layer.objects.active=rotor
bpy.ops.wm.save_as_mainfile(filepath=str(ASSET/(name.lower()+'.blend')))
print('Saved editable quad assembly:',name)
