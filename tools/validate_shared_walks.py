"""Measure ground contact and articulation in delivered GLBs, without Blender.

These checks complement close moving visual review; they are not an art score.
Straight-line root travel is stride / clip duration. Curved-world steering and
runtime blending still need a future runtime foot-placement owner.
"""
from pathlib import Path
import hashlib
import json
import numpy as np
from validate_character_animation import (OUT, ROOT, unpack, load_skin,
                                          deform, require)
from animation.runtime_reference import world_at


def check(c, profile):
    doc,chunks = unpack(OUT/'exports'/(c['id']+'.glb'))
    blob = next(b for t,b in chunks if t==0x004e4942)
    joints,inverse,primitives = load_skin(doc,blob)
    names = {n['name']:i for i,n in enumerate(doc['nodes']) if 'name' in n}
    skin_names = {doc['nodes'][n]['name']:i for i,n in enumerate(joints)}
    animation = next(a for a in doc['animations'] if a['name']=='walk')
    report = json.loads((OUT/'reports'/(c['id']+'.json')).read_text())
    clip = next(a for a in report['clips'] if a['name']=='walk')
    require(abs(clip['stride_m']-c['stride_m'])<1e-6,'Stride metadata mismatch')
    weights = np.concatenate([p[1] for p in primitives])
    influences = np.concatenate([p[2] for p in primitives])
    human = c['rig']=='humanoid'
    feet = {}
    for foot,fall in profile['footfalls'].items():
        name = ('foot.' if human else 'hoof_')+foot
        mask = np.any((influences==skin_names[name]) & (weights>.99),axis=1)
        indices = np.flatnonzero(mask)
        require(len(indices)>3,'Missing rigid foot surface')
        feet[foot] = {'indices':indices,'fall':fall,'samples':[]}
    arms = {s:[] for s in ['L','R']} if human else {}
    knees = {f:[] for f in feet}
    floor = 1e6
    # Exercise interpolation as well as authored keys: quaternion interpolation
    # can dip a sole below the floor even when both neighbouring keys are valid.
    sample_count = 2*(clip['frames']-1)
    for frame in range(sample_count+1):
        u = frame/sample_count;t = u*clip['seconds']
        world = world_at(doc,blob,animation,t)
        posed = deform(world,joints,inverse,primitives)
        floor = min(floor,float(posed[:,1].min()))
        for f,record in feet.items():
            phase = (u-record['fall'])%1
            vertices = posed[record['indices']].copy()
            vertices[:,2] += c['stride_m']*u
            record['samples'].append((phase,vertices))
            upper = ('thigh.' if human else 'upper_')+f
            lower = ('shin.' if human else 'lower_')+f
            foot = ('foot.' if human else 'hoof_')+f
            h,k,a = [world[names[n]][:3,3] for n in [upper,lower,foot]]
            cosine = np.dot(h-k,a-k)/(np.linalg.norm(h-k)*np.linalg.norm(a-k))
            knees[f].append(180-float(np.degrees(np.arccos(np.clip(cosine,-1,1)))))
        for s in arms:
            shoulder = world[names['upper_arm.'+s]][:3,3]
            hand = world[names['hand.'+s]][:3,3]
            arms[s].append(hand-shoulder)
    require(floor>=-3e-5,c['id']+': geometry penetrates floor')
    metrics = {}
    for f,record in feet.items():
        max_drift = 0.;clearance = 0.;contact_samples = 0
        previous = None
        for phase,vertices in record['samples']:
            clearance = max(clearance,float(vertices[:,1].min()))
            flat = .14<phase<profile['stance_fraction']-.15
            if flat:
                contact_samples += 1
                if previous is not None and phase>previous[0]:
                    max_drift = max(max_drift,float(np.linalg.norm(vertices-previous[1],axis=1).max()))
                previous = (phase,vertices)
            else:previous = None
        bend_range = max(knees[f])-min(knees[f])
        require(contact_samples>=3,'No sustained flat stance: '+c['id']+'/'+f)
        require(max_drift<.001,'Planted foot slides: '+c['id']+'/'+f)
        require(clearance>.012,'Swing foot does not clear ground: '+c['id']+'/'+f)
        require(bend_range>15,'Insufficient knee articulation: '+c['id']+'/'+f)
        metrics[f] = {'flat_stance_samples':contact_samples,'maximum_frame_drift_m':max_drift,
                      'swing_clearance_m':clearance,'knee_bend_range_degrees':bend_range}
    arm_metrics = {}
    for s,values in arms.items():
        values = np.array(values)
        reach = float(-values[:,1].max())
        swing = float(np.ptp(values[:,2]))
        require(reach>.20,'Arm remains raised near bind pose: '+c['id'])
        require(swing>.12,'Arm lacks opposing swing: '+c['id'])
        arm_metrics[s] = {'minimum_hand_below_shoulder_m':reach,'fore_aft_swing_m':swing}
    digest = hashlib.sha256((OUT/'exports'/(c['id']+'.glb')).read_bytes()).hexdigest()
    return {'id':c['id'],'sha256':digest,'profile':c['rig'],'feet':metrics,
            'arms':arm_metrics,'minimum_skin_y_m':floor,'sample_rate_hz':60,
            'samples':sample_count+1}


def validate(output=None, only=None):
    global OUT
    if output is not None:OUT=Path(output)
    config = json.loads((ROOT/'assets/animation/characters.json').read_text())
    profiles = json.loads((ROOT/'assets/animation/walk-profiles.json').read_text())
    results = [check(c,profiles[c['rig']]) for c in config['characters'] if only is None or c['id']==only]
    output = OUT/'walk-v3/walk-validation.json'
    output.parent.mkdir(parents=True,exist_ok=True)
    from transaction_character_animation import atomic_bytes
    atomic_bytes(output,(json.dumps({'status':'passed','characters':results},indent=2)+'\n').encode())
    print('Shared walk contact/articulation checks passed for',len(results),'exported characters.')


def main():
    import argparse
    from transaction_character_animation import locked_output
    parser=argparse.ArgumentParser()
    parser.add_argument('--output-root',type=Path,default=OUT)
    parser.add_argument('--only')
    args=parser.parse_args()
    with locked_output(args.output_root):validate(args.output_root,args.only)


if __name__=='__main__':main()
