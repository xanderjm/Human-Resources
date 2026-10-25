"""Two shared, contact-driven walk cycles retargeted onto measured limb chains.

This is an offline IK solver: only baked joint TRS leaves Blender. Foot targets
move backwards at exactly root travel speed during stance; swing recovers with
continuous endpoint velocity. No per-character performance/temperament branch.
"""
import math
import bpy
from mathutils import Quaternion, Vector
from rig import author, game, smooth


def hermite(a,b,ma,mb,t):
    return (2*t**3-3*t*t+1)*a+(t**3-2*t*t+t)*ma+(-2*t**3+3*t*t)*b+(t**3-t*t)*mb


def foot_path(phase,stride,profile):
    duty = profile['stance_fraction']
    extent = stride*duty/2
    if phase < duty:
        forward = extent-stride*phase
        lift = 0.0
        pitch = profile['heel_strike_degrees']*(1-smooth(0,.12,phase))
        pitch += profile['toe_off_degrees']*smooth(duty-.14,duty,phase)
        return forward,lift,pitch,True
    t = (phase-duty)/(1-duty)
    tangent = -stride*(1-duty)
    forward = hermite(-extent,extent,tangent,tangent,t)
    lift = math.sin(math.pi*t)**1.7
    pitch = profile['toe_off_degrees']*(1-smooth(0,.55,t))
    pitch += profile['heel_strike_degrees']*smooth(.60,1,t)
    return forward,lift,pitch,False


def orient(rig,name,world_rotation):
    """Set a rig-space orientation while accounting for its posed ancestors."""
    bone = rig.pose.bones[name]
    rest = bone.bone.matrix_local.to_quaternion()
    parent_delta = Quaternion()
    if bone.parent:
        parent_delta = bone.parent.matrix.to_quaternion() @ bone.parent.bone.matrix_local.to_quaternion().inverted()
    bone.rotation_quaternion = (parent_delta @ rest).inverted() @ world_rotation


def aim_chain_bone(rig,name,direction):
    rest = rig.data.bones[name]
    delta = (rest.tail_local-rest.head_local).rotation_difference(direction)
    orient(rig,name,delta @ rest.matrix_local.to_quaternion())
    bpy.context.view_layer.update()


def solve_chain(rig,upper,lower,target,pole):
    bpy.context.view_layer.update()
    start = rig.pose.bones[upper].head.copy()
    a = rig.data.bones[upper].length
    b = rig.data.bones[lower].length
    ray = target-start
    distance = ray.length
    if distance > a+b+.0002 or distance < abs(a-b)+.0001:
        raise ValueError(f'Unreachable walk target {rig.name}/{upper}: {distance:.4f}, reach {a+b:.4f}')
    direction = ray.normalized()
    bend = pole-direction*pole.dot(direction)
    bend.normalize()
    along = (a*a-b*b+distance*distance)/(2*distance)
    knee = start+direction*along+bend*math.sqrt(max(0,a*a-along*along))
    aim_chain_bone(rig,upper,knee-start)
    aim_chain_bone(rig,lower,target-knee)
    error = (rig.pose.bones[lower].tail-target).length
    if error > .0001:
        raise ValueError(f'Walk IK residual {upper}: {error}')
    return knee


class SharedWalk:
    def __init__(self,rig,mesh,character,profile):
        self.rig,self.mesh,self.character,self.profile = rig,mesh,character,profile
        self.human = character['rig']=='humanoid'
        self.feet = {}
        for name in profile['footfalls']:
            upper = 'thigh.'+name if self.human else 'upper_'+name
            lower = 'shin.'+name if self.human else 'lower_'+name
            foot = 'foot.'+name if self.human else 'hoof_'+name
            group = mesh.vertex_groups[foot].index
            vertices = [v.co.copy() for v in mesh.data.vertices
                        if any(g.group==group and g.weight>.99 for g in v.groups)]
            if not vertices:
                raise ValueError('No rigid sole vertices: '+foot)
            floor = min(v.z for v in vertices)
            sole = [v for v in vertices if v.z < floor+.006]
            center = Vector(tuple((min(v[i] for v in sole)+max(v[i] for v in sole))/2 for i in range(3)))
            self.feet[name] = {'upper':upper,'lower':lower,'foot':foot,'sole':sole,'surface':vertices,
                               'center':center,'ankle':rig.data.bones[foot].head_local.copy(),
                               'length':rig.data.bones[upper].length+rig.data.bones[lower].length}
        self.leg_length = min(f['length'] for f in self.feet.values())
        self.stride = self.leg_length*profile['stride_per_leg_length']
        self.contacts = []

    def target(self,name,phase):
        f = self.feet[name]
        forward,lift,pitch,stance = foot_path(phase,self.stride,self.profile)
        q = Quaternion((1,0,0),math.radians(pitch))
        sole = f['sole'];ankle = f['ankle'];center = f['center']
        # Heel/toe pivot remains fixed in world space while the boot rolls.
        pivot = min(sole,key=lambda v:v.y) if pitch>0 else max(sole,key=lambda v:v.y)
        target = center.copy()
        target.y -= forward
        target.z = -min((q@(v-ankle)).z for v in f['surface'])
        target.z += lift*self.leg_length*self.profile['foot_clearance_per_leg_length']
        target.z += self.profile.get('sole_clearance_m',0)
        target.y += pivot.y-center.y-(q@(pivot-ankle)).y
        target.x = ankle.x
        return target,q,stance

    def body(self,u):
        from motion import rotate,translate
        phase = math.tau*u
        leg = self.leg_length
        lower = self.profile['body_lower_per_leg_length']*leg
        bob = self.profile['body_bob_per_leg_length']*leg
        if self.human:
            # Down after contact, then up over the supporting leg. The previous
            # sign lifted the body during loading and produced a buoyant shuffle.
            translate(self.rig,'pelvis',(.055*leg*math.sin(phase),-lower-bob*math.sin(2*phase),0))
            rotate(self.rig,'pelvis',0,-4*math.cos(phase),2*math.sin(phase))
            rotate(self.rig,'spine',1.5,2*math.cos(phase),-.8*math.sin(phase))
            rotate(self.rig,'chest',2.0,4*math.cos(phase),-1.2*math.sin(phase))
            rotate(self.rig,'head',-2.5,-1.5*math.cos(phase),.5*math.sin(phase))
        else:
            translate(self.rig,'pelvis',(.035*leg*math.sin(phase),-lower+bob*math.cos(2*phase),0))
            rotate(self.rig,'pelvis',1.3*math.sin(2*phase+.7),.9*math.sin(phase),1.6*math.sin(phase))
            rotate(self.rig,'spine',-1.0*math.sin(2*phase+.3),-.8*math.sin(phase),-1.2*math.sin(phase))
            rotate(self.rig,'chest',1.0*math.sin(2*phase),0,-1.3*math.sin(phase))
            rotate(self.rig,'head',-1.7*math.sin(2*phase+.4),-.9*math.sin(phase),.9*math.sin(phase))
            rotate(self.rig,'tail',0,9*math.sin(phase-.7),3*math.sin(phase))
            rotate(self.rig,'ear.L',0,0,3*math.sin(phase+.5))
            rotate(self.rig,'ear.R',0,0,-3*math.sin(phase+.5))
        bpy.context.view_layer.update()

    def arms(self,u):
        phase = math.tau*u
        for side,sign in [('L',1),('R',-1)]:
            upper,lower = 'upper_arm.'+side,'forearm.'+side
            # A pendulum at the shoulder with a soft elbow and delayed forearm,
            # rather than a fixed-length wrist track that freezes elbow shape.
            swing = -sign*math.radians(23)*math.cos(phase)
            flex = math.radians(23+9*math.sin(phase*sign-.45))
            spread = math.radians(12)
            def direction(angle):
                return author((sign*math.sin(spread),
                               -math.cos(spread)*math.cos(angle),
                               math.cos(spread)*math.sin(angle)))
            aim_chain_bone(self.rig,upper,direction(swing))
            aim_chain_bone(self.rig,lower,direction(swing+flex))
            forearm = self.rig.pose.bones[lower]
            delta = forearm.matrix.to_quaternion() @ forearm.bone.matrix_local.to_quaternion().inverted()
            wrist = Quaternion((1,0,0),math.radians(4)*sign*math.sin(phase-.3))
            orient(self.rig,'hand.'+side,wrist @ delta @ self.rig.data.bones['hand.'+side].matrix_local.to_quaternion())
        bpy.context.view_layer.update()

    def sample(self,u):
        self.body(u)
        if self.human:
            self.arms(u)
        frame_contacts = {}
        for name,fall in self.profile['footfalls'].items():
            phase = (u-fall)%1
            target,q,stance = self.target(name,phase)
            f = self.feet[name]
            # These short, fused animal legs expose the hock/carpal silhouette;
            # the hidden proximal stifle is not a separate source-mesh segment.
            pole = author((0,0,1 if self.human else -1))
            solve_chain(self.rig,f['upper'],f['lower'],target,pole)
            orient(self.rig,f['foot'],q @ self.rig.data.bones[f['foot']].matrix_local.to_quaternion())
            bpy.context.view_layer.update()
            frame_contacts[name] = {'stance':stance,'phase':phase,'ankle':list(game(target))}
        self.contacts.append(frame_contacts)
