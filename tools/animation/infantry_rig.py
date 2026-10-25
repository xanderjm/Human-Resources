"""Measured T-pose infantry rigs; original geometry/UV/materials stay intact."""
import math
import bpy
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree
from rig import author, game, reconcile_seam_weights, smooth


def bones_for(fit, sockets):
    m=fit['landmarks_m'];hip=m['pelvis'];head=m['head_pivot'];height=fit['bounds_m'][1][1]
    chest_y=(m['shoulder.L'][1]+m['shoulder.R'][1])/2
    spine_y=hip[1]+(chest_y-hip[1])*.55
    bones=[('root',(0,0,0),(0,.12,0),None),
           ('pelvis',hip,(0,hip[1]+.10,0),'root'),
           ('spine',(0,hip[1]+.10,0),(0,spine_y,0),'pelvis'),
           ('chest',(0,spine_y,0),(0,chest_y+.06,0),'spine'),
           ('neck',(0,chest_y+.06,0),head,'chest'),
           ('head',head,m['head_tip'],'neck'),
           ('socket_head',(0,height*.94,.03),(0,height*1.01,.03),'head')]
    for side,sign in [('L',1),('R',-1)]:
        shoulder,elbow,wrist=[m[n+'.'+side] for n in ['shoulder','elbow','wrist']]
        hinge=[sign*fit['hands'][side]['distal_mitten_curl_threshold_abs_x_m'],wrist[1],wrist[2]]
        bones += [('clavicle.'+side,(sign*.08,chest_y,0),shoulder,'chest'),
                  ('upper_arm.'+side,shoulder,elbow,'clavicle.'+side),
                  ('forearm.'+side,elbow,wrist,'upper_arm.'+side),
                  ('hand.'+side,wrist,hinge,'forearm.'+side),
                  ('mitten.'+side,hinge,m['hand_tip.'+side],'hand.'+side),
                  ('thigh.'+side,m['hip.'+side],m['knee.'+side],'pelvis'),
                  ('shin.'+side,m['knee.'+side],m['ankle.'+side],'thigh.'+side)]
        ankle=m['ankle.'+side]
        bones.append(('foot.'+side,ankle,(ankle[0],ankle[1]-.04,ankle[2]+.17),'shin.'+side))
        palm=m['palm_contact.'+side]
        bones.append(('socket_palm.'+side,palm,(palm[0],palm[1]+.05,palm[2]),'hand.'+side))
    bones.append(('socket_weapon',(0,0,0),(0,.15,0),'chest'))
    for name,position in sockets.items():
        bones.append((name,position,(position[0],position[1]+.05,position[2]),'socket_weapon'))
    return bones


def nearest_bone(point,bones):
    best=(float('inf'),'pelvis')
    for name,a,b,parent in bones:
        if name=='root' or name.startswith('socket_') or name.startswith('mitten.'):
            continue
        a,b=np.array(a),np.array(b);d=b-a
        amount=np.clip(np.dot(point-a,d)/np.dot(d,d),0,1)
        distance=float(np.linalg.norm(point-a-amount*d))
        if distance<best[0]:best=(distance,name)
    return best[1]


def build(mesh,points,fit,sockets):
    bones=bones_for(fit,sockets)
    data=bpy.data.armatures.new(fit['id']+'_Skeleton')
    rig=bpy.data.objects.new(fit['id']+'_Rig',data);bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active=rig;rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    for name,a,b,parent in bones:
        bone=data.edit_bones.new(name);bone.head=author(a);bone.tail=author(b)
        if parent:bone.parent=data.edit_bones[parent]
        bone.use_deform=name!='root' and not name.startswith('socket_')
    bpy.ops.object.mode_set(mode='OBJECT')
    # Solve heat on a welded proxy; transfer weights to original split vertices.
    proxy=mesh.copy();proxy.data=mesh.data.copy();bpy.context.collection.objects.link(proxy)
    proxy.vertex_groups.clear()
    bpy.ops.object.select_all(action='DESELECT');proxy.select_set(True)
    bpy.context.view_layer.objects.active=proxy;bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.remove_doubles(threshold=.000001)
    bpy.ops.object.mode_set(mode='OBJECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    weights={};tree=KDTree(len(proxy.data.vertices))
    for v in proxy.data.vertices:
        tree.insert(v.co,v.index)
        row=sorted([(proxy.vertex_groups[g.group].name,g.weight) for g in v.groups
                    if g.weight>1e-6],key=lambda a:-a[1])[:4]
        if not row:row=[(nearest_bone(game(v.co),bones),1)]
        total=sum(w for n,w in row);weights[v.index]=[(n,w/total) for n,w in row]
    tree.balance();mesh.vertex_groups.clear()
    for bone in data.bones:mesh.vertex_groups.new(name=bone.name)
    for v,p in zip(mesh.data.vertices,points):
        nearest,index,distance=tree.find(v.co)
        if distance>.000002:raise ValueError('Proxy changed source geometry')
        row=weights[index];side='L' if p[0]>0 else 'R';sign=1 if side=='L' else -1
        m=fit['landmarks_m'];wrist=m['wrist.'+side]
        # A height-only head mask cuts across the heavy body's horizontal
        # shoulders. Keep those vertices on the arm instead of pinning them to
        # the head and stretching the sleeve into a triangular fin.
        shoulder_y=(m['shoulder.L'][1]+m['shoulder.R'][1])/2
        head_width=min(.23,abs(m['shoulder.L'][0])*.85)
        if (p[1]>m['head_pivot'][1]-.015 and
                (abs(p[0])<head_width or p[1]>shoulder_y+.16)):
            row=[('head',1)]
        elif abs(p[0])<head_width and m['pelvis'][1]+.06<p[1]<m['head_pivot'][1]:
            # Central straps and disconnected pack details cannot follow an
            # adjacent upper arm. Retain only torso influences from heat.
            torso=[(n,w) for n,w in row if n in ('pelvis','spine','chest','neck')]
            total=sum(w for n,w in torso)
            row=[(n,w/total) for n,w in torso] if total>1e-6 else [('chest',1)]
        if (fit['id']=='infantry-rocketeer-body' and abs(p[0])<.22
                and .82<p[1]<m['head_pivot'][1] and p[2]<-.16):
            # Actual source pack bounds: X +/- .185, Y .854..1.312, Z behind
            # -.1668. Preserve the box as a rigid chest attachment.
            row=[('chest',1)]
        if abs(p[0])>abs(wrist[0])-.005:
            # Rigid palm gives a stable surface contact; a broad, limited mitten
            # fold preserves the original topology without invented fingers.
            hinge=fit['hands'][side]['distal_mitten_curl_threshold_abs_x_m']
            thumb=p[2]>m['palm_contact.'+side][2]+.045
            curl=0 if thumb else smooth(hinge-.012,hinge+.025,abs(p[0]))
            row=[('hand.'+side,1-curl),('mitten.'+side,curl)]
        rigid_top=min(m['knee.'+side][1]-.04,m['ankle.'+side][1]+.10)
        if p[1]<rigid_top:row=[('foot.'+side,1)]
        for name,weight in row:
            if weight>1e-6:mesh.vertex_groups[name].add([v.index],weight,'REPLACE')
    pdata=proxy.data;bpy.data.objects.remove(proxy,do_unlink=True);bpy.data.meshes.remove(pdata)
    mesh.parent=rig
    mod=mesh.modifiers.new('Portable linear skin','ARMATURE');mod.object=rig
    mod.use_deform_preserve_volume=False
    reconcile_seam_weights(mesh)
    for bone in rig.pose.bones:bone.rotation_mode='QUATERNION'
    rig.show_in_front=True;data.display_type='STICK'
    return rig
