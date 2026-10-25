"""Review imported infantry GLBs with their original separate weapon meshes.

Blender --background --factory-startup --python-exit-code 1
  --python tools/render_infantry_review.py -- [--start 1 --end 540]
  --still --clip aim --time .6 --actor infantry-rifleman-body --view right

The weapon receives only the exported socket's pose delta. Fit-candidate
transforms are deliberately not applied a second time. This is offline evidence,
not a native rendering or performance qualification.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/animation/infantry'
FPS = 30
DIMENSIONS = (1280, 720)
BODY_IDS = ('infantry-rifleman-body', 'infantry-shotgunner-body', 'infantry-rocketeer-body')
# Seconds are playback seconds, never accelerated for the film.
SEQUENCE = (
    ('idle', 3.0, True), ('walk', 3.3, True), ('aim_enter', .5, False),
    ('aim', 2.0, True), ('fire', 1.8, True), ('aim_exit', .5, False),
    ('beam_lift', 1.5, False), ('beam_struggle', 2.4, True),
    ('beam_release', 1.4, False), ('idle', 1.6, True),
)
COUNT = round(sum(s[1] for s in SEQUENCE)*FPS)
sys.path.insert(0, str(ROOT/'tools'))
from render_character_showcase import aim, lighting, material


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sequence():
    result = []
    start = 0.0
    for clip, duration, loop in SEQUENCE:
        previous = result[-1]['clip'] if result else None
        fade = .18 if previous and 'walk' in (clip, previous) else 0.0
        result.append({'clip': clip, 'start': start, 'end': start+duration,
                       'loop': loop, 'crossfade_seconds': fade})
        start += duration
    return result


def locate(seconds):
    shots = sequence()
    for index, shot in enumerate(shots):
        if seconds < shot['end']-1e-8:
            return index, shot, max(0, seconds-shot['start'])
    return len(shots)-1, shots[-1], shots[-1]['end']-shots[-1]['start']


def empty(name):
    obj = bpy.data.objects.new(name, None)
    bpy.context.collection.objects.link(obj)
    return obj


def import_objects(path, wrapper):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    imported = set(bpy.data.objects)-before
    for obj in imported:
        if obj.parent not in imported:
            world = obj.matrix_world.copy()
            obj.parent = wrapper
            obj.matrix_world = world
    return imported


def clip_names(path):
    raw = path.read_bytes()
    if len(raw) < 20 or raw[:4] != b'glTF':
        raise ValueError('Not a GLB: '+str(path))
    length, kind = struct.unpack_from('<II', raw, 12)
    if kind != 0x4e4f534a or length+20 > len(raw):
        raise ValueError('Missing GLB JSON: '+str(path))
    doc = json.loads(raw[20:20+length])
    names = [a['name'] for a in doc.get('animations', [])]
    if len(set(names)) != len(names):
        raise ValueError('Duplicate clip names: '+str(path))
    if not {s[0] for s in SEQUENCE} <= set(names):
        raise ValueError('Missing review clips: '+str(path))
    return names


def action_curves(action, slot):
    if action.is_action_layered:
        return action.layers[0].strips[0].channelbag(slot).fcurves
    return action.fcurves


def load_actor(config, exports, reports, x, sources):
    asset = config['id']
    path = exports/(asset+'.glb')
    names = clip_names(path)
    sources[str(path.relative_to(ROOT))] = digest(path)
    for name, expected in [(config['model_file'], config['model_sha256']),
                           (config['equipment_candidate']['weapon_file'],
                            config['equipment_candidate']['weapon_sha256'])]:
        source = ROOT/name
        if digest(source) != expected:
            raise ValueError('Fit-source drift: '+name)
        sources[name] = expected
    display = empty(asset+'_ReviewDisplay')
    imported = import_objects(path, display)
    rigs = [o for o in imported if o.type == 'ARMATURE']
    if len(rigs) != 1:
        raise ValueError('Expected one armature: '+asset)
    rig = rigs[0]
    # Make the socket conjugation's parent space explicit, preserving import TRS.
    world = rig.matrix_world.copy()
    rig.parent = display
    rig.matrix_world = world
    if 'socket_weapon' not in rig.pose.bones or not rig.animation_data:
        raise ValueError('Missing weapon socket or actions: '+asset)
    actions = {}
    for track in rig.animation_data.nla_tracks:
        for strip in track.strips:
            matched = [name for name in names if track.name == name
                       or strip.action.name == name or strip.action.name.startswith(name+'.')]
            if len(matched) != 1 or matched[0] in actions:
                raise ValueError('Ambiguous imported action: '+asset+'/'+track.name)
            actions[matched[0]] = (strip.action, strip.action_slot)
        track.mute = True
    if set(actions) != set(names):
        raise ValueError('Imported action set mismatch: '+asset)
    rig.animation_data.action = None
    for bone in rig.pose.bones:
        bone.rotation_mode = 'QUATERNION'
    report_path = reports/(asset+'.json')
    report = json.loads(report_path.read_text())
    sources[str(report_path.relative_to(ROOT))] = digest(report_path)
    walk = next(c for c in report['clips'] if c['name'] == 'walk')
    stride = walk['stride_m']
    walk_action = actions['walk'][0]
    walk_duration = (walk_action.frame_range[1]-walk_action.frame_range[0])/FPS
    if not math.isfinite(stride) or stride <= 0 or abs(walk_duration-1.1) > 1/FPS:
        raise ValueError('Invalid authored walk stride/duration: '+asset)
    weapon = empty(asset+'_SourceWeapon')
    weapon.parent = display
    import_objects(ROOT/config['equipment_candidate']['weapon_file'], weapon)
    display.location.x = x
    return {'id': asset, 'height': config['bounds_m'][1][1], 'display': display,
            'rig': rig, 'weapon': weapon, 'actions': actions, 'stride_m': stride,
            'walk_speed_mps': stride/walk_duration, 'clip_names': names}


def raw_pose(actor, clip, seconds, loop):
    rig = actor['rig']
    for bone in rig.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    action, slot = actor['actions'][clip]
    start, end = action.frame_range
    duration = (end-start)/FPS
    if duration <= 0:
        raise ValueError('Empty clip: '+actor['id']+'/'+clip)
    local = seconds % duration if loop else min(max(0, seconds), duration)
    for curve in action_curves(action, slot):
        if curve.data_path.endswith(('rotation_quaternion', 'location', 'scale')):
            rig.path_resolve(curve.data_path)[curve.array_index] = curve.evaluate(start+local*FPS)
    for bone in rig.pose.bones:
        bone.rotation_quaternion.normalize()
    return local


def pose(actor, index, shot, local, unblended=False):
    fade = shot['crossfade_seconds']
    previous = None
    if not unblended and index and fade > 0 and local < fade:
        last = sequence()[index-1]
        raw_pose(actor, last['clip'], last['end']-last['start'], last['loop'])
        previous = {b.name: (b.location.copy(), b.rotation_quaternion.copy(), b.scale.copy())
                    for b in actor['rig'].pose.bones}
    sampled = raw_pose(actor, shot['clip'], local, shot['loop'])
    if previous is not None:
        blend = local/fade
        for bone in actor['rig'].pose.bones:
            location, rotation, scale = previous[bone.name]
            bone.location = location.lerp(bone.location, blend)
            bone.rotation_quaternion = rotation.slerp(bone.rotation_quaternion, blend).normalized()
            bone.scale = scale.lerp(bone.scale, blend)
    return sampled


def stage(samples):
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE_NEXT'
    scene.eevee.taa_render_samples = samples
    scene.render.resolution_x, scene.render.resolution_y = DIMENSIONS
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.compression = 15
    scene.render.fps = FPS
    scene.view_settings.view_transform = 'AgX'
    scene.world = bpy.data.worlds.new('Infantry review studio')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.16, .21, .27, 1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .5
    bpy.ops.mesh.primitive_plane_add(size=200)
    bpy.context.object.name = 'Flat contact plane / no plinth'
    bpy.context.object.data.materials.append(material('Contact floor', (.12, .16, .20)))
    line = material('Half-metre registration', (.145, .19, .23))
    for axis in (0, 1):
        for i in range(-12, 13):
            position = [0, 0, .0005]
            position[axis] = i*.5
            bpy.ops.mesh.primitive_cube_add(size=1, location=position)
            obj = bpy.context.object
            obj.name = 'Quiet half-metre floor rule'
            obj.dimensions = (.005, 12, .001) if axis == 0 else (12, .005, .001)
            obj.data.materials.append(line)
    lighting()
    data = bpy.data.cameras.new('Fit review camera')
    camera = bpy.data.objects.new(data.name, data)
    bpy.context.collection.objects.link(camera)
    data.type = 'ORTHO'
    scene.camera = camera


def set_camera(actors, seconds, view, framing):
    scene = bpy.context.scene
    centre_y = sum(a['display'].location.y for a in actors)/len(actors)
    if len(actors) == 1:
        actor = actors[0]
        height = actor['height']
        focus = Vector((0, centre_y, height*(.72 if framing == 'upper' else .57)))
        width = 2.5 if framing == 'upper' else 4.15
    else:
        focus = Vector((0, centre_y, 1.14))
        width = 8.35
    vectors = {'front': (0, -10, 2.4), 'right': (10, 0, 2.0),
               'three-quarter': (6, -10, 2.6), 'opposite': (-6, -10, 2.6), 'back': (-6, 10, 2.6)}
    if view == 'timeline':
        # A small continuous arc exposes depth while retaining visible soles.
        angle = math.radians(8+14*math.sin(math.pi*seconds/18))
        eye = Vector((10*math.sin(angle), -10*math.cos(angle), 2.5))
    else:
        eye = Vector(vectors[view])
    scene.camera.location = focus+eye
    aim(scene.camera, focus)
    scene.camera.data.ortho_scale = width


def set_frame(actors, seconds, view, framing, clip=None, clip_seconds=0):
    index, shot, local = locate(seconds)
    if clip:
        index, shot, local = 0, {'clip': clip, 'loop': False, 'crossfade_seconds': 0}, clip_seconds
    records = []
    for actor in actors:
        # Retain the completed walk translation through aiming/capture/landing.
        travel = 0 if clip else max(0, min(3.3, seconds-3.0))
        actor['display'].location.y = -travel*actor['walk_speed_mps']
        sampled = pose(actor, index, shot, local, unblended=bool(clip))
        records.append({'id': actor['id'], 'clip': shot['clip'], 'clip_seconds': sampled,
                        'travel_m': travel*actor['walk_speed_mps']})
    bpy.context.view_layer.update()
    for actor in actors:
        rig = actor['rig']
        socket = rig.pose.bones['socket_weapon']
        delta = socket.matrix @ socket.bone.matrix_local.inverted()
        actor['weapon'].matrix_basis = rig.matrix_local @ delta @ rig.matrix_local.inverted()
    bpy.context.view_layer.update()
    set_camera(actors, seconds, view, framing)
    bpy.context.view_layer.update()
    for actor, record in zip(actors, records):
        ground = actor['display'].location.copy()
        projected = world_to_camera_view(bpy.context.scene, bpy.context.scene.camera, ground)
        record['label_x'] = projected.x*DIMENSIONS[0]
    return {'clip': shot['clip'], 'local_seconds': local, 'actors': records,
            'crossfade_seconds': shot['crossfade_seconds'], 'unblended_still': bool(clip)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--start', type=int, default=1)
    parser.add_argument('--end', type=int, default=COUNT)
    parser.add_argument('--still', nargs='?', const=1, type=int)
    parser.add_argument('--clip')
    parser.add_argument('--time', type=float, default=0)
    parser.add_argument('--actor', choices=('all', *BODY_IDS), default='all')
    parser.add_argument('--view', choices=('timeline', 'front', 'right', 'three-quarter', 'opposite', 'back'), default='timeline')
    parser.add_argument('--framing', choices=('full', 'upper'), default='upper')
    parser.add_argument('--samples', type=int, choices=(16, 24, 32), default=24)
    parser.add_argument('--exports', type=Path, default=OUT/'exports')
    parser.add_argument('--reports', type=Path, default=OUT/'reports')
    parser.add_argument('--output', type=Path, default=OUT)
    argv = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    args = parser.parse_args(argv)
    if args.clip and args.still is None:
        parser.error('--clip requires --still')
    if args.actor != 'all' and args.still is None:
        parser.error('Single-actor views require --still; the film contains all three actors')
    if not math.isfinite(args.time) or args.time < 0:
        parser.error('--time must be finite and nonnegative')
    if not 1 <= args.start <= args.end <= COUNT or (args.still is not None and not 1 <= args.still <= COUNT):
        parser.error('Frame range must be within 1..'+str(COUNT))
    output = args.output.resolve()
    sources = {}
    fit_path = ROOT/'assets/animation/infantry-fit.json'
    sources[str(fit_path.relative_to(ROOT))] = digest(fit_path)
    recipe = ROOT/'assets/animation/infantry.json'
    if recipe.exists():
        sources[str(recipe.relative_to(ROOT))] = digest(recipe)
    fit = json.loads(fit_path.read_text())
    by_id = {b['id']: b for b in fit['bodies']}
    ids = BODY_IDS if args.actor == 'all' else (args.actor,)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    stage(args.samples)
    positions = (-2.5, 0, 2.5) if len(ids) == 3 else (0,)
    actors = [load_actor(by_id[asset], args.exports.resolve(), args.reports.resolve(), x, sources)
              for asset, x in zip(ids, positions)]
    if args.clip and any(args.clip not in a['actions'] for a in actors):
        parser.error('Requested clip is absent from an imported GLB')
    provenance = {'sources': sources, 'renderer_sha256': digest(Path(__file__)),
                  'fps': FPS, 'dimensions': DIMENSIONS, 'timeline': sequence(),
                  'samples': args.samples, 'view': args.view, 'framing': args.framing,
                  'actors': [{'id': a['id'], 'height_m': a['height'],
                              'stride_m': a['stride_m'], 'speed_mps': a['walk_speed_mps'],
                              'clips': a['clip_names']} for a in actors],
                  'method': 'Reimported exported GLBs; original normalized weapons follow socket pose/rest delta; physical scale; no plinth; snapshot local TRS crossfade at walk boundaries only. No runtime performance claim.'}
    fingerprint = hashlib.sha256(json.dumps(provenance, sort_keys=True).encode()).hexdigest()
    scenes = output/'review-scenes'
    scenes.mkdir(parents=True, exist_ok=True)
    (scenes/(fingerprint+'.json')).write_text(json.dumps(provenance, indent=2)+'\n')
    frames = [args.still] if args.still is not None else range(args.start, args.end+1)
    for frame in frames:
        seconds = (frame-1)/FPS
        meta = set_frame(actors, seconds, args.view, args.framing, args.clip, args.time)
        if args.still is not None:
            dest = output/'fit-stills'
            time_label = f'{args.time if args.clip else frame:g}'.replace('.', 'p')
            name = f'{args.actor}-{args.clip or "timeline"}-{time_label}-{args.view}-{args.framing}'
        else:
            dest, name = output/'review-frames', f'{frame:05d}'
        dest.mkdir(parents=True, exist_ok=True)
        path = dest/name
        meta.update(frame=frame, seconds=seconds, source_fingerprint=fingerprint)
        bpy.context.scene.render.filepath = str(path.with_suffix('.png'))
        bpy.ops.render.render(write_still=True)
        path.with_suffix('.json').write_text(json.dumps(meta, indent=2)+'\n')
        if frame % FPS == 1 or args.still is not None:
            print(f'INFANTRY_REVIEW_FRAME {frame}/{COUNT} {meta["clip"]}', flush=True)
    for name, expected in sources.items():
        if digest(ROOT/name) != expected:
            raise ValueError('Source changed during capture: '+name)
    print('INFANTRY_REVIEW_PROVENANCE '+fingerprint, flush=True)


if __name__ == '__main__':
    main()
