"""Contact-driven infantry performance: one weapon frame, two solved hands."""
import math
import bpy
from mathutils import Matrix, Quaternion, Vector
from rig import author,smooth
from motion import pose_reset,rotate,translate,sample_pose,keep_above_ground
from walk import SharedWalk,solve_chain,orient

CONVERSION=Matrix(((1,0,0),(0,0,-1),(0,1,0)))


def rotation_game(rows):
    return (CONVERSION@Matrix(rows)@CONVERSION.transposed()).to_quaternion()


def recoil_envelope(u):
    kick=math.sin(math.pi*min(1,u/.20)) if u<.20 else .16*math.sin(math.tau*2*(u-.20))*math.exp(-5*(u-.20))
    return kick*(1-smooth(.65,1,u))


class InfantryPerformance:
    def __init__(self,rig,mesh,fit,config,profile):
        self.rig,self.mesh,self.fit,self.config=rig,mesh,fit,config
        c={'rig':'humanoid','id':fit['id'],'hip_height':fit['landmarks_m']['pelvis'][1],
           'head_y':fit['landmarks_m']['head_pivot'][1],'special':'signal'}
        self.character=c;self.walk=SharedWalk(rig,mesh,c,profile)
        self.candidate=fit['equipment_candidate']
        q=self.candidate['rotation_xyzw']
        self.fit_q=rotation_game(Quaternion((q[3],q[0],q[1],q[2])).to_matrix())
        self.stock=author(config['weapon_sockets']['socket_stock'])
        self.stock_target=author(self.candidate['shoulder_contact_target_m'])
        self.hand_q={s:rotation_game(self.candidate['grip_results'][s]['hand_world_rotation_row_major']) for s in ['L','R']}
        self.contacts=[]

    def base(self,clip,u):
        rig=self.rig
        pose_reset(rig)
        if clip in ['walk','run']:
            self.walk.sample(u)
        elif clip.startswith('beam_'):
            sample_pose(rig,self.character,clip,u,self.walk)
        elif clip in ['alert','startle','hit_react']:
            sample_pose(rig,self.character,clip,u,self.walk)
        elif clip=='scan':
            rotate(rig,'head',0,10*math.sin(math.tau*u))
            rotate(rig,'spine',0,3*math.sin(math.tau*u))
        else:
            rotate(rig,'spine',.45*math.sin(math.tau*u))
        if clip not in ['walk','run']:
            keep_above_ground(rig,self.mesh)

    def sample(self,clip,u):
        self.base(clip,u)
        kick=recoil_envelope(u) if clip=='fire' else 0
        if kick:
            # A single chest impulse carries shoulders/head/weapon together.
            # It is authored before the weapon/two-hand solve, never applied to
            # root/pelvis/legs. Exact-zero endpoints preserve aim transitions.
            chest=self.rig.pose.bones['chest']
            basis=chest.bone.matrix_local.to_quaternion()
            forward=self.fit_q@author((0,0,1))
            axis=forward.cross(author((0,1,0))).normalized()
            q=Quaternion(axis,math.radians(self.config['chest_recoil_degrees'])*kick)
            chest.rotation_quaternion=basis.inverted()@q@basis@chest.rotation_quaternion
            chest.location+=chest.bone.matrix_local.to_3x3().inverted()@(
                -forward*self.config['chest_recoil_back_m']*max(0,kick))
            bpy.context.view_layer.update()
        base_head=self.rig.pose.bones['head'].rotation_quaternion.copy()
        aim=1 if clip in ['aim','fire'] else smooth(0,1,u) if clip=='aim_enter' else 1-smooth(0,1,u) if clip=='aim_exit' else 0
        # Head follows the actual cross-body muzzle heading; launcher leans away
        # from the tube. These postures are distinct from a chest-centred rifle.
        rotate(self.rig,'head',-2*aim,self.candidate['yaw_degrees']*(.45+.55*aim),
               -4*aim if self.config['role']=='rocketeer' else 0)
        self.rig.pose.bones['head'].rotation_quaternion=base_head@self.rig.pose.bones['head'].rotation_quaternion
        if clip.startswith('beam_'):
            amount=1 if clip=='beam_struggle' else smooth(.08,.90,u) if clip=='beam_lift' else 1-smooth(.05,.60,u)
            rotate(self.rig,'head',-10*amount,self.candidate['yaw_degrees']*.45,0)
        protraction=self.candidate['grip_results']['L']['clavicle_protraction_m']
        protraction+=self.config['carry_protraction_m']*(1-aim)
        translate(self.rig,'clavicle.L',(0,-self.config['carry_shoulder_drop_m']*(1-aim),protraction))
        bpy.context.view_layer.update()
        chest=self.rig.pose.bones['chest']
        delta=chest.matrix@chest.bone.matrix_local.inverted()
        tilt=math.radians(self.config['carry_pitch_degrees']*(1-aim))
        if clip=='fire':
            # Fast impulse, damped return; hands and fore-end follow one frame.
            tilt-=math.radians(self.config['recoil_pitch_degrees'])*kick
        q=self.fit_q@Quaternion(author((1,0,0)),tilt)
        offset=author((0,-self.config['carry_lower_m']*(1-aim),0))
        stock_target=self.stock_target+offset-(q@author((0,0,1)))*self.config['recoil_m']*max(0,kick)
        origin=stock_target-q@self.stock
        weapon=Matrix.Translation(origin)@q.to_matrix().to_4x4()
        desired=delta@weapon
        bone=self.rig.pose.bones['socket_weapon']
        bone.matrix=desired@bone.bone.matrix_local.to_quaternion().to_matrix().to_4x4()
        bpy.context.view_layer.update()
        contact_record={}
        for side,sign in [('L',1),('R',-1)]:
            target_name='socket_support' if side=='L' else 'socket_primary'
            target=desired@author(self.config['weapon_sockets'][target_name])
            hand_rotation=delta.to_quaternion()@q@self.fit_q.inverted()@self.hand_q[side]
            palm_offset=author(self.fit['hands'][side]['wrist_to_palm_contact_m'])
            wrist=target-hand_rotation@palm_offset
            pole=delta.to_quaternion()@author((sign*.6,-.8,-.15))
            solve_chain(self.rig,'upper_arm.'+side,'forearm.'+side,wrist,pole)
            orient(self.rig,'hand.'+side,hand_rotation@self.rig.data.bones['hand.'+side].matrix_local.to_quaternion())
            rotate(self.rig,'mitten.'+side,0,0,-sign*self.config['mitten_curl_degrees'][side])
            bpy.context.view_layer.update()
            actual=self.rig.pose.bones['socket_palm.'+side].head
            error=(actual-target).length
            if error>.0001:raise ValueError(f'Palm contact residual {side}: {error}')
            contact_record[side]={'target_blender':list(target),'error_m':error}
        # A fixed 2mm authoring contact envelope covers the measured <=1.5mm
        # between-key knee/ankle arc sag at 30Hz. It preserves horizontal plants
        # and all attachment relationships; no per-frame root correction follows.
        root=self.rig.pose.bones['root']
        root.location+=root.bone.matrix_local.to_3x3().inverted()@author((0,self.config['sole_clearance_m'],0))
        bpy.context.view_layer.update()
        self.contacts.append(contact_record)


def create_actions(rig,mesh,fit,config,spec,profile):
    performance=InfantryPerformance(rig,mesh,fit,config,profile)
    records=[];rig.animation_data_create()
    for name,definition in spec['clips'].items():
        seconds=definition['seconds'];count=round(seconds*spec['fps'])
        action=bpy.data.actions.new(name);action.use_fake_user=True
        rig.animation_data.action=action;contact_start=len(performance.contacts)
        for i in range(count+1):
            performance.sample(name,i/count)
            for bone in rig.pose.bones:
                bone.keyframe_insert('rotation_quaternion',frame=i+1,group=bone.name)
                bone.keyframe_insert('location',frame=i+1,group=bone.name)
        slot=rig.animation_data.action_slot
        for layer in action.layers:
            for strip in layer.strips:
                for curve in strip.channelbag(slot).fcurves:
                    for key in curve.keyframe_points:key.interpolation='LINEAR'
        action['loop']=definition['loop']
        stride=performance.walk.stride if name in ['walk','run'] else 0
        records.append({'name':name,'seconds':count/spec['fps'],'frames':count+1,'loop':definition['loop'],
                        'stride_m':stride,'contact_samples':performance.contacts[contact_start:]})
    rig.animation_data.action=None;pose_reset(rig)
    return records
