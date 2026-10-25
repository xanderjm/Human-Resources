"""Render the exported GLBs, not the in-memory authoring scene.

Blender --background --python tools/render_character_showcase.py --
  --group animals [--still 1] [--start 1 --end 600]
Typography and encoding are handled by finish_character_showcase.py.
"""
import argparse
import json
import math
from pathlib import Path
import sys
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/animation/showcase'
sys.path.insert(0, str(ROOT/'tools/animation'))
from showcase_timeline import timeline, locate


def material(name, color, metallic=0):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Roughness'].default_value = .72
    bsdf.inputs['Metallic'].default_value = metallic
    return mat


def aim(obj, target):
    obj.rotation_euler = (Vector(target)-obj.location).to_track_quat('-Z', 'Y').to_euler()


def lighting():
    for name, location, power, size, color in [
        ('Warm key', (-3, -4, 7), 1400, 5, (1, .89, .75)),
        ('Cool fill', (4, -2, 4), 1000, 4, (.64, .80, 1)),
        ('Mint rim', (0, 4, 5), 1800, 3, (.57, 1, .84))]:
        data = bpy.data.lights.new(name, 'AREA')
        data.energy, data.shape, data.size, data.color = power, 'DISK', size, color
        o = bpy.data.objects.new(name, data)
        bpy.context.collection.objects.link(o)
        o.location = location
        aim(o, (0, 0, .6))


def stage():
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE_NEXT'
    scene.eevee.taa_render_samples = 32
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.fps = 30
    scene.world = bpy.data.worlds.new('Studio')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.11, .16, .23, 1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .4
    scene.view_settings.view_transform = 'AgX'
    floor = material('Deep slate', (.035, .055, .082))
    bpy.ops.mesh.primitive_plane_add(size=200)
    bpy.context.object.data.materials.append(floor)
    plinth = material('Blue slate', (.075, .12, .16), .15)
    rim = material('Mint registration', (.20, .62, .47), .15)
    for x in [-2.25, 0, 2.25]:
        bpy.ops.mesh.primitive_cylinder_add(vertices=96, radius=.91, depth=.09, location=(x, 0, -.055))
        bpy.context.object.data.materials.append(plinth)
        bevel = bpy.context.object.modifiers.new('Soft edge', 'BEVEL')
        bevel.width, bevel.segments = .035, 3
        bpy.ops.mesh.primitive_torus_add(major_segments=96, minor_segments=8,
                                       major_radius=.89, minor_radius=.009, location=(x, 0, -.005))
        bpy.context.object.data.materials.append(rim)
    lighting()
    data = bpy.data.cameras.new('Camera')
    camera = bpy.data.objects.new('Camera', data)
    bpy.context.collection.objects.link(camera)
    camera.location = (0, -9, 4.8)
    aim(camera, (0, 0, 1.15))
    data.type, data.ortho_scale = 'ORTHO', 7.8
    scene.camera = camera


def import_actor(c, x):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(ROOT/'build/animation/exports'/(c['id']+'.glb')))
    objects = set(bpy.data.objects)-before
    rig = next(o for o in objects if o.type == 'ARMATURE')
    scale = 1.5/c['height']
    display = bpy.data.objects.new(c['id']+'_Display', None)
    bpy.context.collection.objects.link(display)
    rig.parent = display
    display.scale = (scale,)*3
    display.location.x = x
    display.rotation_euler.z = math.radians(32 if c['rig'] == 'quadruped' else -12)
    actions = {}
    # Importer puts each clip in its own muted NLA track. Reuse those exact actions.
    for track in rig.animation_data.nla_tracks:
        for strip in track.strips:
            name = strip.action.name
            for expected in ['idle','walk','run','alert',c['special'],'startle','hit_react',
                             'beam_lift','beam_struggle','beam_release']:
                if track.name == expected or name == expected or name.startswith(expected+'.'):
                    actions[expected] = (strip.action, strip.action_slot)
        track.mute = True
    if len(actions) != 10:
        raise ValueError(f'Imported action mismatch {c["id"]}: {list(actions)}')
    return rig, actions


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--group', choices=['animals','humans'], required=True)
    parser.add_argument('--still', type=int)
    parser.add_argument('--start', type=int, default=1)
    parser.add_argument('--end', type=int)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    spec = json.loads((ROOT/'assets/animation/characters.json').read_text())
    chars = spec['characters'][:3] if args.group == 'animals' else spec['characters'][3:]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    stage()
    actors = [import_actor(c, x) for c, x in zip(chars, [-2.25, 0, 2.25])]
    shots = timeline(spec)
    count = round(shots[-1]['end']*30)
    dest = OUT/args.group
    dest.mkdir(parents=True, exist_ok=True)
    (OUT/'timeline.json').write_text(json.dumps(shots, indent=2)+'\n')
    frames = [args.still] if args.still else range(args.start, (args.end or count)+1)
    for frame in frames:
        shot, local = locate(shots, (frame-1)/30)
        for c, (rig, actions) in zip(chars, actors):
            clip = c['special'] if shot['clip'] == 'special' else shot['clip']
            action, slot = actions[clip]
            rig.animation_data.action = action
            rig.animation_data.action_slot = slot
        # Actions have different lengths; evaluate their F-curves directly per actor.
        bpy.context.scene.frame_set(0)
        for c, (rig, actions) in zip(chars, actors):
            clip = c['special'] if shot['clip'] == 'special' else shot['clip']
            action, slot = actions[clip]
            start, end = action.frame_range
            duration = (end-start)/30
            t = local % duration if shot['loop'] else min(local, duration)
            aframe = start+t*30
            bag = action.layers[0].strips[0].channelbag(slot)
            for fc in bag.fcurves:
                path = fc.data_path
                if path.endswith('rotation_quaternion'):
                    rig.path_resolve(path)[fc.array_index] = fc.evaluate(aframe)
                elif path.endswith('location'):
                    rig.path_resolve(path)[fc.array_index] = fc.evaluate(aframe)
        # Disable action evaluation after assigning evaluated transforms.
        for rig, actions in actors:
            rig.animation_data.action = None
        bpy.context.view_layer.update()
        bpy.context.scene.render.filepath = str(dest/f'{frame:05d}.png')
        bpy.ops.render.render(write_still=True)
        if frame % 30 == 1:
            print(f'SHOWCASE_FRAME {args.group} {frame}/{count}', flush=True)


if __name__ == '__main__':
    main()
