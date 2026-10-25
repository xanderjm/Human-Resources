"""Measured low-poly deformation rigs. No runtime or source-mesh mutation."""
import math
import bpy
import numpy as np
from mathutils import Vector


def author(v):
    """Game Y-up/+Z-forward to Blender Z-up/-Y-forward."""
    return Vector((v[0], -v[2], v[1]))


def game(v):
    return np.array((v[0], v[2], -v[1]))


def smooth(lo, hi, value):
    t = min(1.0, max(0.0, (value - lo) / (hi - lo)))
    return t * t * (3 - 2 * t)


def humanoid_bones(c):
    hip = c['hip_height']
    bones = [
        ('root', (0, 0, 0), (0, .15, 0), None),
        ('pelvis', (0, hip, 0), (0, hip+.12, 0), 'root'),
        ('spine', (0, hip+.12, 0), (0, .88, 0), 'pelvis'),
        ('chest', (0, .88, 0), (0, 1.02, 0), 'spine'),
        ('neck', (0, 1.02, 0), (0, c['head_y']+.06, 0), 'chest'),
        ('head', (0, c['head_y']+.06, 0), (0, 1.48, 0), 'neck'),
        ('socket_head', (0, 1.51, 0), (0, 1.61, 0), 'head')]
    for side, s in [('L', 1), ('R', -1)]:
        shoulder = (s*c['shoulder_x'], .98, 0)
        elbow = (s*(c['hand_x']*.79), (c['hand_y']+.98)/2, 0)
        wrist = (s*c['hand_x'], c['hand_y']+.07, .015)
        knee = (s*.16, hip*.52, .025)
        ankle = (s*.18, .12, 0)
        bones.extend([
            ('clavicle.'+side, (s*.08, .99, 0), shoulder, 'chest'),
            ('upper_arm.'+side, shoulder, elbow, 'clavicle.'+side),
            ('forearm.'+side, elbow, wrist, 'upper_arm.'+side),
            ('hand.'+side, wrist, (s*(c['hand_x']+.015), c['hand_y']-.04, .025), 'forearm.'+side),
            ('thigh.'+side, (s*.15, hip, 0), knee, 'pelvis'),
            ('shin.'+side, knee, ankle, 'thigh.'+side),
            ('foot.'+side, ankle, (s*.18, .07, .20), 'shin.'+side)])
    return bones


def quadruped_bones(c, points):
    h = c['height']
    lo, hi = points.min(0), points.max(0)
    zrear, zfront = lo[2]*.55, hi[2]*.50
    body_y = h*.52
    pivot, tip = c['head_pivot'], c['head_tip']
    bones = [
        ('root', (0, 0, 0), (0, .12, 0), None),
        ('pelvis', (0, body_y, zrear), (0, body_y, -.05), 'root'),
        ('spine', (0, body_y, -.05), (0, body_y, zfront*.8), 'pelvis'),
        ('chest', (0, body_y, zfront*.8), pivot, 'spine'),
        ('head', pivot, tip, 'chest'),
        ('tail', (0, body_y*.85, lo[2]*.83), (0, body_y*.95, lo[2]), 'pelvis')]
    for side, s in [('L', 1), ('R', -1)]:
        ear = (s*c['ear_x'], pivot[1]+h*.13, pivot[2]+.04)
        bones.append(('ear.'+side, ear, (ear[0]*1.32, ear[1]+.025, ear[2]), 'head'))
        for end, zsign in [('front', 1), ('hind', -1)]:
            # Fit each foot from actual near-ground geometry, not nominal size.
            foot = points[(points[:, 1] < h*.12) & (points[:, 0]*s > 0)
                          & (points[:, 2]*zsign > 0)]
            if len(foot) < 3:
                raise ValueError('Cannot identify four separate feet: '+c['id'])
            x, _, z = np.median(foot, axis=0)
            top = (x, c['hip_height'], z)
            knee = (x, c['hip_height']*.52, z-.025)
            ankle = (x, h*.055, z)
            name = end+'.'+side
            bones.extend([
                ('upper_'+name, top, knee, 'chest' if end == 'front' else 'pelvis'),
                ('lower_'+name, knee, ankle, 'upper_'+name),
                ('hoof_'+name, ankle, (x, h*.035, z+.07), 'lower_'+name)])
    return bones


def blend_weights(first, second, t):
    return {first: 1-t, second: t}


def human_weights(p, c):
    x, y, z = p
    side = 'L' if x > 0 else 'R'
    ax = abs(x)
    # Anatomical masks prevent a hand binding to the nearby thigh or robe.
    arm_border = .43-.18*smooth(.65, 1.0, y)
    arm_bottom = .54 if c['id'] == 'resident-body' else .36
    arm = smooth(arm_border-.035, arm_border+.035, ax) if y > arm_bottom else 0
    if y > c['head_y']-.02:
        return blend_weights('neck', 'head', smooth(c['head_y']-.02, c['head_y']+.12, y))
    if arm > 0 and y < 1.06:
        wrist_y = c['hand_y']+.07
        elbow_y = (c['hand_y']+.98)/2
        if y < wrist_y+.035:
            weights = blend_weights('hand.'+side, 'forearm.'+side,
                                    smooth(wrist_y-.03, wrist_y+.035, y))
        else:
            weights = blend_weights('forearm.'+side, 'upper_arm.'+side,
                                    smooth(elbow_y-.045, elbow_y+.045, y))
        return {**{k: v*arm for k, v in weights.items()}, 'chest': 1-arm}
    if y < c['hip_height']+.06:
        hip = c['hip_height']
        if y < .18:
            return blend_weights('foot.'+side, 'shin.'+side, smooth(.11, .18, y))
        if y < hip*.65:
            return blend_weights('shin.'+side, 'thigh.'+side, smooth(hip*.44, hip*.65, y))
        return blend_weights('thigh.'+side, 'pelvis', smooth(hip-.12, hip+.06, y))
    if y < .87:
        return blend_weights('pelvis', 'spine', smooth(c['hip_height']+.04, .87, y))
    return blend_weights('chest', 'neck', smooth(1.0, c['head_y']+.03, y))


def animal_weights(p, c, bones):
    x, y, z = p
    h = c['height']
    side = 'L' if x > 0 else 'R'
    head_z = c['head_pivot'][2]
    # Ears are part of the head silhouette; only the outer tip has ear weight.
    head_t = smooth(head_z-.045, head_z+.15, z)
    ear_z = head_z+.04
    if (y > c['head_pivot'][1]+h*.04 and abs(z-ear_z) < h*.16
            and abs(x) > c['ear_x']):
        t = smooth(c['ear_x'], c['ear_x']*1.32, abs(x))
        return blend_weights('head', 'ear.'+side, t)
    # Legs attach below the belly. Blend their root into the body over a narrow band.
    leg_t = 1-smooth(c['hip_height']-.015, c['hip_height']+.07, y)
    end = 'front' if z > 0 else 'hind'
    leg_name = end+'.'+side
    leg_origin = next(b[1] for b in bones if b[0] == 'upper_'+leg_name)
    # A round attachment footprint excludes the low belly between the legs.
    # The old rectangular mask assigned that surface to a shin, dragging wool
    # below the floor when the carpal joint bent during a planted step.
    radius = math.hypot(z-leg_origin[2],x-leg_origin[0])/(h*.13)
    footprint = 1-smooth(.75,1.2,radius)
    foot_lock = 1-smooth(h*.055,h*.115,y)
    leg_t *= max(footprint,foot_lock)
    if leg_t > 0:
        end = 'front' if z > 0 else 'hind'
        name = end+'.'+side
        hip = c['hip_height']
        if y < h*.10:
            weights = blend_weights('hoof_'+name, 'lower_'+name, smooth(h*.045, h*.10, y))
        else:
            weights = blend_weights('lower_'+name, 'upper_'+name, smooth(hip*.43, hip*.65, y))
        parent = 'chest' if end == 'front' else 'pelvis'
        return {**{k: v*leg_t for k, v in weights.items()}, parent: 1-leg_t}
    # Include the whole protruding muzzle, even where it hangs below the belly.
    # A height mask here pinches the lower snout when the animal lowers its head.
    if head_t > .98:
        return {'head': 1}
    tail_z = next(b[1][2] for b in bones if b[0] == 'tail')
    if z < tail_z and abs(x) < h*.12 and y < h*.65:
        return blend_weights('pelvis', 'tail', smooth(tail_z, tail_z-.07, z))
    body = 'pelvis' if z < -.10 else 'spine'
    return blend_weights(body, 'head', head_t)


def import_character(root, c):
    source = root/'assets/models'/c['id']/(c['id']+'.glb')
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(source))
    imported = set(bpy.data.objects)-before
    meshes = [o for o in imported if o.type == 'MESH']
    for o in meshes:
        transform = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = transform
        bpy.context.view_layer.objects.active = o
        o.select_set(True)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        o.select_set(False)
    for o in imported:
        if o.type != 'MESH':
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    mesh = bpy.context.object
    mesh.name = c['id']+'_Skin'
    points = np.array([game(v.co) for v in mesh.data.vertices])
    return mesh, points


def build_rig(mesh, points, c):
    bones = humanoid_bones(c) if c['rig'] == 'humanoid' else quadruped_bones(c, points)
    data = bpy.data.armatures.new(c['id']+'_Skeleton')
    rig = bpy.data.objects.new(c['id']+'_Rig', data)
    bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    for name, head, tail, parent in bones:
        bone = data.edit_bones.new(name)
        bone.head, bone.tail = author(head), author(tail)
        if parent:
            bone.parent = data.edit_bones[parent]
        # Root and attachment socket are controls, never weighted skin influences.
        bone.use_deform = name != 'root' and not name.startswith('socket_')
    bpy.ops.object.mode_set(mode='OBJECT')
    rig.show_in_front = True
    data.display_type = 'STICK'
    for bone in data.bones:
        mesh.vertex_groups.new(name=bone.name)
    for i, p in enumerate(points):
        weights = human_weights(p, c) if c['rig'] == 'humanoid' else animal_weights(p, c, bones)
        weights = {k: v for k, v in weights.items() if v > 1e-6}
        total = sum(weights.values())
        if not total or len(weights) > 4:
            raise ValueError('Invalid skin influence count')
        for name, value in weights.items():
            mesh.vertex_groups[name].add([i], value/total, 'REPLACE')
    mesh.parent = rig
    mod = mesh.modifiers.new('Skeletal deformation', 'ARMATURE')
    mod.object = rig
    mod.use_deform_preserve_volume = False  # Match ordinary glTF linear skinning.
    for p in rig.pose.bones:
        p.rotation_mode = 'QUATERNION'
    if c['rig'] == 'humanoid':
        # Bone heat follows the connected arm surface through the armpit, where
        # coordinate-only masks cannot separate a sleeve from nearby clothing.
        # Solve on a welded proxy, then transfer only weights. Keep the original
        # per-corner UVs/normals and topology exactly as imported.
        proxy = mesh.copy()
        proxy.data = mesh.data.copy()
        bpy.context.collection.objects.link(proxy)
        proxy.vertex_groups.clear()
        bpy.ops.object.select_all(action='DESELECT')
        proxy.select_set(True)
        bpy.context.view_layer.objects.active = proxy
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.remove_doubles(threshold=.000001)
        bpy.ops.object.mode_set(mode='OBJECT')
        rig.select_set(True)
        bpy.context.view_layer.objects.active = rig
        bpy.ops.object.parent_set(type='ARMATURE_AUTO')
        solved = {}
        for vertex in proxy.data.vertices:
            influences = sorted([(g.group, g.weight) for g in vertex.groups
                                 if g.weight > 1e-6], key=lambda g: -g[1])[:4]
            if not influences:
                # Separate buttons/trim may not connect to the heat surface.
                # Their anatomical position is a deliberate secondary binding.
                fallback = human_weights(game(vertex.co), c)
                influences = [(proxy.vertex_groups[n].index, w) for n,w in fallback.items()]
            total = sum(w for _, w in influences)
            solved[tuple(round(v, 6) for v in vertex.co)] = [
                (proxy.vertex_groups[index].name, weight/total) for index,weight in influences]
        from mathutils.kdtree import KDTree
        tree = KDTree(len(proxy.data.vertices))
        for vertex in proxy.data.vertices:
            tree.insert(vertex.co, vertex.index)
        tree.balance()
        mesh.vertex_groups.clear()
        for bone in data.bones:
            mesh.vertex_groups.new(name=bone.name)
        for vertex in mesh.data.vertices:
            nearest, index, distance = tree.find(vertex.co)
            if distance > .000002:
                raise ValueError('Weight proxy changed source positions')
            weights = solved[tuple(round(v, 6) for v in nearest)]
            for name, weight in weights:
                mesh.vertex_groups[name].add([vertex.index], weight, 'REPLACE')
        proxy_data = proxy.data
        bpy.data.objects.remove(proxy, do_unlink=True)
        bpy.data.meshes.remove(proxy_data)
        # Keep scalp, face and helmet rigid; do not deform painted facial features.
        for vertex, point in zip(mesh.data.vertices, points):
            if point[1] > c['head_y']-.02:
                for group in mesh.vertex_groups:
                    group.remove([vertex.index])
                mesh.vertex_groups['head'].add([vertex.index], 1, 'REPLACE')
    if c['rig'] == 'humanoid':
        # Boot soles are rigid contact surfaces, not a blend into the calf.
        for vertex,point in zip(mesh.data.vertices,points):
            if point[1] < .105:
                for group in mesh.vertex_groups:
                    group.remove([vertex.index])
                name = 'foot.L' if point[0]>0 else 'foot.R'
                mesh.vertex_groups[name].add([vertex.index],1,'REPLACE')
    reconcile_seam_weights(mesh)
    return rig


def reconcile_seam_weights(mesh):
    """UV/normal splits must share weights or the visible surface tears open."""
    seams = {}
    for vertex in mesh.data.vertices:
        key = tuple(round(v, 6) for v in vertex.co)
        seams.setdefault(key, []).append(vertex)
    for vertices in seams.values():
        if len(vertices) < 2:
            continue
        combined = {}
        for vertex in vertices:
            for group in vertex.groups:
                combined[group.group] = combined.get(group.group, 0)+group.weight
        top = sorted(combined.items(), key=lambda item: -item[1])[:4]
        total = sum(w for _, w in top)
        indices = [v.index for v in vertices]
        for group in mesh.vertex_groups:
            group.remove(indices)
        for index, weight in top:
            mesh.vertex_groups[index].add(indices, weight/total, 'REPLACE')
