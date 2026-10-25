"""Authored skeletal motion, sampled at 30Hz into portable glTF actions.

Angles below are model-space degrees. Conjugating by the rest-bone basis
keeps the choreography readable for both horizontal and vertical bones.
"""
import math
import json
from pathlib import Path
import bpy
from mathutils import Euler, Vector
from rig import author, smooth

TAU = 2*math.pi


def pulse(u, start=.05, peak=.28, end=1.0):
    return smooth(start, peak, u)*(1-smooth(peak, end, u))


def pose_reset(rig):
    for p in rig.pose.bones:
        p.location = (0, 0, 0)
        p.rotation_quaternion = (1, 0, 0, 0)
        p.scale = (1, 1, 1)


def rotate(rig, name, pitch=0, yaw=0, roll=0):
    p = rig.pose.bones.get(name)
    if p is None:
        return
    # Author rotation around game X/Y/Z, converted to Blender X/Z/-Y.
    q = Euler((math.radians(pitch), -math.radians(roll), math.radians(yaw)), 'XYZ').to_quaternion()
    basis = p.bone.matrix_local.to_quaternion()
    p.rotation_quaternion = basis.inverted() @ q @ basis


def translate(rig, name, v):
    p = rig.pose.bones[name]
    p.location = p.bone.matrix_local.to_3x3().inverted() @ author(v)


def human_gait(rig, c, phase, running):
    resident = c['id'] == 'resident-body'
    amp = (29 if running else 15)*(0.65 if resident else 1)
    hip = c['hip_height']
    for side, offset, sign in [('L', 0, 1), ('R', math.pi, -1)]:
        a = phase+offset
        swing = math.sin(a)
        rotate(rig, 'thigh.'+side, -amp*swing)
        rotate(rig, 'shin.'+side, max(0, swing)*(42 if running else 24))
        rotate(rig, 'foot.'+side, amp*swing*.45)
        rotate(rig, 'upper_arm.'+side, amp*swing*.72, 0, sign*(4 if running else 1))
        rotate(rig, 'forearm.'+side, -25 if running else -6)
    rotate(rig, 'pelvis', 0, math.sin(phase)*3, math.sin(phase)*2)
    rotate(rig, 'chest', -7 if running else (-3 if resident else 0), -math.sin(phase)*3)
    rotate(rig, 'head', 3 if running else 0, math.sin(phase)*1.5)
    translate(rig, 'root', (0, (1-math.cos(phase*2))*(.018 if running else .009), 0))


def animal_gait(rig, c, phase, running):
    amp = 29 if running else 18
    # Walk: four-beat lateral sequence. Run: paired diagonal trot.
    offsets = {'front.L': 0, 'hind.R': 0 if running else math.pi/2,
               'front.R': math.pi, 'hind.L': math.pi if running else 3*math.pi/2}
    for name, offset in offsets.items():
        wave = math.sin(phase+offset)
        rotate(rig, 'upper_'+name, amp*wave)
        rotate(rig, 'lower_'+name, -max(0, -wave)*(25 if running else 16))
        rotate(rig, 'hoof_'+name, -amp*wave*.45)
    bounce = (1-math.cos(phase*2))*(.016 if running else .006)
    translate(rig, 'root', (0, bounce, 0))
    rotate(rig, 'spine', math.sin(phase*2)*1.5, math.sin(phase)*1.5)
    rotate(rig, 'head', -math.sin(phase*2)*2.5)
    rotate(rig, 'tail', 0, math.sin(phase)*12)


def suspend(rig, c, phase, amount=1, height=.55):
    humanoid = c['rig'] == 'humanoid'
    translate(rig, 'root', (math.sin(phase)*.025*amount, height, 0))
    rotate(rig, 'pelvis', math.sin(phase)*5*amount, math.sin(phase*.5)*0, math.sin(phase)*5*amount)
    rotate(rig, 'head', -12*amount, math.sin(phase)*8*amount)
    if humanoid:
        for side, sign in [('L', 1), ('R', -1)]:
            wave = math.sin(phase+(0 if side == 'L' else math.pi))
            rotate(rig, 'upper_arm.'+side, wave*16*amount, 0, sign*46*amount)
            rotate(rig, 'forearm.'+side, -25*amount+wave*12*amount)
            rotate(rig, 'hand.'+side, wave*12*amount)
            rotate(rig, 'thigh.'+side, wave*20*amount, 0, sign*8*amount)
            rotate(rig, 'shin.'+side, (20+max(0, -wave)*20)*amount)
            rotate(rig, 'foot.'+side, 8*amount)
    else:
        for i, name in enumerate(['front.L', 'hind.R', 'front.R', 'hind.L']):
            wave = math.sin(phase+i*math.pi/2)
            rotate(rig, 'upper_'+name, (wave*24+6)*amount)
            rotate(rig, 'lower_'+name, (-16-max(0, wave)*16)*amount)
        rotate(rig, 'tail', 0, math.sin(phase)*25*amount)


def human_gesture(rig, c, u):
    # A full cycle begins/ends at rest; the middle holds the readable silhouette.
    a = smooth(.05, .30, u)*(1-smooth(.72, .98, u))
    wave = math.sin(TAU*3*u)*a
    if c['special'] == 'shoo':
        rotate(rig, 'upper_arm.L', -38*a, 0, 52*a)
        rotate(rig, 'forearm.L', -24*a+12*wave)
        rotate(rig, 'hand.L', 0, 0, wave*20)
        rotate(rig, 'head', -10*a, 10*a)
    elif c['special'] == 'yawn':
        rotate(rig, 'upper_arm.R', -40*a, -12*a, -28*a)
        rotate(rig, 'forearm.R', -66*a)
        rotate(rig, 'head', -12*a, -5*a)
        rotate(rig, 'chest', -4*a)
        rotate(rig, 'upper_arm.L', -18*a, 0, 22*a)
    else:
        rotate(rig, 'upper_arm.L', -35*a, 0, 65*a)
        rotate(rig, 'forearm.L', -15*a+wave*10)
        rotate(rig, 'head', -12*a, 18*a)
        rotate(rig, 'chest', 0, 6*a)


def animal_gesture(rig, c, u):
    a = smooth(.02, .23, u)*(1-smooth(.73, .98, u))
    # Small head nods preserve the short generated neck and avoid belly penetration.
    angle = {'graze': 22, 'sniff': 16, 'nibble': 19}[c['special']]
    rotate(rig, 'head', (angle+math.sin(TAU*4*u)*3)*a,
           math.sin(TAU*2*u)*(6 if c['special'] == 'sniff' else 2)*a)
    rotate(rig, 'chest', 3*a)
    rotate(rig, 'ear.L', 0, 0, math.sin(TAU*3*u)*8*a)
    rotate(rig, 'ear.R', 0, 0, -math.sin(TAU*3*u)*8*a)
    rotate(rig, 'tail', 0, math.sin(TAU*2*u)*16*a)


def sample_pose(rig, c, clip, u, walker=None):
    pose_reset(rig)
    phase = TAU*u
    human = c['rig'] == 'humanoid'
    if clip == 'walk':
        if walker is None:
            raise ValueError('Walk sampling requires fitted contact solver')
        walker.sample(u)
    elif clip == 'run':
        (human_gait if human else animal_gait)(rig, c, phase, clip == 'run')
    elif clip == 'idle':
        rotate(rig, 'spine', math.sin(phase)*.8, 0, math.sin(phase)*.7)
        rotate(rig, 'head', math.sin(phase)*1.5, math.sin(phase)*3)
        if not human:
            rotate(rig, 'tail', 0, math.sin(phase)*10)
            rotate(rig, 'ear.L', 0, 0, math.sin(phase)*4)
            rotate(rig, 'ear.R', 0, 0, -math.sin(phase)*4)
    elif clip == 'special':
        (human_gesture if human else animal_gesture)(rig, c, u)
    elif clip == 'alert':
        a = pulse(u, .03, .36, .99)
        rotate(rig, 'head', -24*a if human else -18*a, math.sin(phase)*5*a)
        rotate(rig, 'chest', -4*a)
        if human:
            rotate(rig, 'upper_arm.L', -8*a, 0, 7*a)
            rotate(rig, 'upper_arm.R', -8*a, 0, -7*a)
    elif clip in ('startle', 'hit_react'):
        a = pulse(u, .02, .23, .98)
        hop = pulse(u, .10, .38, .72)*(.06 if clip == 'startle' else .015)
        translate(rig, 'root', (0, hop, -.035*a))
        rotate(rig, 'pelvis', -7*a)
        rotate(rig, 'head', -12*a)
        if human:
            for side, sign in [('L', 1), ('R', -1)]:
                rotate(rig, 'upper_arm.'+side, -14*a, 0, sign*25*a)
                rotate(rig, 'forearm.'+side, -15*a)
                rotate(rig, 'thigh.'+side, -8*a)
                rotate(rig, 'shin.'+side, 16*a)
        else:
            for name in ['front.L', 'front.R', 'hind.L', 'hind.R']:
                rotate(rig, 'upper_'+name, -10*a)
                rotate(rig, 'lower_'+name, 8*a)
    elif clip == 'beam_struggle':
        suspend(rig, c, phase)
    elif clip == 'beam_lift':
        a = smooth(.08, .90, u)
        # End phase 2pi matches the struggle cycle's phase zero.
        suspend(rig, c, phase, a, .55*a)
    elif clip == 'beam_release':
        a = 1-smooth(.05, .60, u)
        suspend(rig, c, 0, a, .55*a)
        squash = pulse(u, .52, .66, .98)
        translate(rig, 'root', (0, .55*a-.025*squash, 0))
        if human and squash > 0:
            rotate(rig, 'thigh.L', -10*squash)
            rotate(rig, 'thigh.R', -10*squash)
            rotate(rig, 'shin.L', 20*squash)
            rotate(rig, 'shin.R', 20*squash)
    else:
        raise ValueError('Unknown choreography '+clip)


def keep_above_ground(rig, mesh):
    """Correct only below-floor geometry; retain deliberately airborne motion."""
    bpy.context.view_layer.update()
    evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    skin = evaluated.to_mesh()
    lowest = min(v.co.z for v in skin.vertices)
    evaluated.to_mesh_clear()
    if lowest < 0:
        root = rig.pose.bones['root']
        root.location += root.bone.matrix_local.to_3x3().inverted() @ Vector((0, 0, -lowest))


def create_actions(rig, mesh, c, spec):
    from walk import SharedWalk
    profiles = json.loads((Path(__file__).resolve().parents[2]/'assets/animation/walk-profiles.json').read_text())
    walker = SharedWalk(rig,mesh,c,profiles[c['rig']])
    if abs(walker.stride-c['stride_m']) > 1e-6 or c['walk_seconds'] != walker.profile['cycle_seconds']:
        raise ValueError('Walk travel metadata differs from shared profile: '+c['id'])
    fps = spec['fps']
    records = []
    rig.animation_data_create()
    for clip, definition in spec['clips'].items():
        duration = c['walk_seconds'] if clip == 'walk' else definition['seconds']
        count = round(duration*fps)
        name = c['special'] if clip == 'special' else clip
        action = bpy.data.actions.new(name)
        action.use_fake_user = True
        rig.animation_data.action = action
        for i in range(count+1):
            sample_pose(rig, c, clip, i/count,walker)
            if clip != 'walk':
                keep_above_ground(rig, mesh)
            for bone in rig.pose.bones:
                bone.keyframe_insert('rotation_quaternion', frame=i+1, group=bone.name)
                bone.keyframe_insert('location', frame=i+1, group=bone.name)
        # All channels sampled: LINEAR prevents overshoot between exported frames.
        slot = rig.animation_data.action_slot
        for layer in action.layers:
            for strip in layer.strips:
                for fcurve in strip.channelbag(slot).fcurves:
                    for key in fcurve.keyframe_points:
                        key.interpolation = 'LINEAR'
        action['loop'] = definition['loop']
        action['purpose'] = definition['purpose']
        records.append({'name': name, 'source_clip': clip, 'frames': count+1,
                        'seconds': count/fps, 'loop': definition['loop'],
                        'stride_m': (walker.stride if clip=='walk' else c['stride_m']*(1.8 if clip == 'run' else 1)
                                     if clip in ('walk', 'run') else 0)})
        if clip == 'walk':
            records[-1]['shared_profile'] = c['rig']
            records[-1]['contact_samples'] = walker.contacts
    rig.animation_data.action = None
    pose_reset(rig)
    return records
