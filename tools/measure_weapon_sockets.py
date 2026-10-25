"""Measure separate game-weapon geometry; no equipping, rigging or animation.

Regions are inspected feature selections in canonical centred metre coordinates.
Surface sampling includes triangle interiors, avoiding sparse-vertex bias on long
wooden foregrips/flat tube surfaces. Result datums remain art attachment proposals.
"""
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import ImageDraw
from normalize_glb import unpack, pack, points
from render_prop_review import primitives, render

ROOT=Path(__file__).resolve().parents[1]
DEFINITIONS={
 'weapon-carbine':{
  'primary_grip':([-1,-.112,-.16],[1,-.02,-.06],'wood-albedo-selected pistol grip stem; bounds centre',True),
  'support_grip':([-.045,-.014,.11],[.045,.025,.19],'underside of front receiver/collar; no separate handle; bounds centre',False),
  'shoulder_rest':([-1,-1,-.351],[1,1,-.32],'rear stock buttplate contact patch; bounds centre',False),
  'muzzle':([-1,.025,.335],[1,.095,.351],'single barrel terminal rim centre',False)},
 'weapon-shotgun':{
  'primary_grip':([-1,-.04,-.16],[1,.055,-.10],'wood-albedo-selected narrow stock neck immediately behind trigger; bounds centre',True),
  'support_grip':([-1,-.01,.00],[1,.04,.31],'wooden fore-end; bounds centre',True),
  'shoulder_rest':([-1,-1,-.351],[1,1,-.295],'rear wooden stock/butt contact patch; bounds centre',True),
  'muzzle_left':([-1,.025,.33],[0,1,.351],'negative-X barrel terminal rim centre',False),
  'muzzle_right':([0,.025,.33],[1,1,.351],'positive-X barrel terminal rim centre',False)},
 'weapon-rocket-launcher':{
  'primary_grip':([-1,-.18,-.215],[1,-.035,-.105],'rear pistol grip behind trigger guard; bounds centre',False),
  'support_grip':([-1,-.205,.04],[1,-.09,.19],'forward hanging grip stem, excluding bottom flange; bounds centre',False),
  'shoulder_rest':([-.05,-.065,-.43],[.05,.005,-.27],'underside of rear tube, not a separate pad; lower tangent patch',False),
  'loaded_nose_tip':([-1,-1,.56],[1,1,.576],'terminal coral loaded-nose cap centre',False),
  'tube_mouth_plane':([-1,-1,.285],[1,1,.325],'front collar/tube-mouth plane occupied by loaded nose',False)},
}

def surface_samples(parts):
    samples=[];colours=[]
    bary=np.array([(i/8,j/8,1-(i+j)/8) for i in range(9) for j in range(9-i)])
    for pos,uv,indices,texture in parts:
        tri=pos[indices];mapped=uv[indices]
        pts=np.einsum('ki,tij->tkj',bary,tri).reshape(-1,3)
        tex=np.einsum('ki,tij->tkj',bary,mapped).reshape(-1,2)
        tx=np.clip((tex[:,0]*(texture.shape[1]-1)).astype(int),0,texture.shape[1]-1)
        ty=np.clip((tex[:,1]*(texture.shape[0]-1)).astype(int),0,texture.shape[0]-1)
        samples.append(pts);colours.append(texture[ty,tx].astype(float))
    return np.concatenate(samples),np.concatenate(colours)

def rounded(v):return np.asarray(v).round(6).tolist()

def measure(asset,regions):
    record=ROOT/'assets/production/records'/asset/'model-audit.json'
    if not record.exists():return None
    audit=json.loads(record.read_text())
    if audit.get('import_review',{}).get('status')!='passed':raise ValueError(f'{asset}: normalize before measurement')
    model=ROOT/audit['model_file'];doc,chunks=unpack(model);actual,_=points(doc,chunks);centre=(actual.min(0)+actual.max(0))/2
    parts=list(primitives(doc,chunks));samples,rgb=surface_samples(parts);samples-=centre
    anchors={}
    for name,(lo,hi,description,wood) in regions.items():
        mask=((samples>=lo)&(samples<=hi)).all(1)
        if wood:mask&=(rgb[:,0]>rgb[:,1]*1.2)&(rgb[:,1]>rgb[:,2]*1.2)&(rgb[:,0]>65)
        selected=samples[mask]
        if len(selected)<6:raise ValueError(f'{asset}/{name}: insufficient actual feature samples')
        low,high=selected.min(0),selected.max(0);point=(low+high)/2
        if name=='shoulder_rest' and asset=='weapon-rocket-launcher':point[1]=low[1]
        if name.startswith('muzzle') or name=='loaded_nose_tip':point[2]=high[2]
        anchors[name]={'canonical_position':point,'feature_bounds':[low,high],'samples':len(selected),'method':description}
    if asset=='weapon-rocket-launcher':
        red=(rgb[:,0]>rgb[:,1]*1.4)&(rgb[:,0]>rgb[:,2]*1.25)&(rgb[:,0]>90)&(samples[:,2]>.24)
        nose=samples[red]
        if len(nose)<50:raise ValueError('Approved coral loaded nose not found')
        low,high=nose.min(0),nose.max(0);point=(low+high)/2;point[2]=low[2]
        anchors['tube_mouth_plane']={'canonical_position':point,'feature_bounds':[low,high],'samples':len(nose),'method':'Measured rear boundary of distal coral nose surface; tube mouth is occupied by integral loaded nose.'}
    primary=anchors['primary_grip']['canonical_position'];current_grip=primary+centre
    # Move only the existing outer normalization root. The complete BIN payload
    # (geometry, UVs and image) remains unchanged; raw download is never written.
    roots=doc['scenes'][doc.get('scene',0)]['nodes']
    if len(roots)!=1:raise ValueError('Expected one normalized outer root')
    node=doc['nodes'][roots[0]]
    if 'matrix' not in node:raise ValueError('Expected normalization-root matrix')
    extras=doc.setdefault('asset',{}).setdefault('extras',{})
    changed=np.linalg.norm(current_grip)>1e-10 or extras.get('pivot')!='primary-grip'
    if changed:
        values=np.array(node['matrix']);values[12:15]-=current_grip;node['matrix']=values.tolist()
        node['name']=asset+'-Y-up-primary-grip-pivot';extras['pivot']='primary-grip'
        pack(doc,chunks,model)
    final,stats=points(*unpack(model))
    sockets={}
    for name,a in anchors.items():
        point=a['canonical_position']-primary
        sockets[name]={'position_m':rounded(point),'up':[0,1,0],'forward':[0,0,1],
            'measurement':{'method':a['method'],'feature_surface_samples':a['samples'],'selected_feature_bounds_m':[rounded(b-primary) for b in a['feature_bounds']]}}
    audit['import_review'].update({'pivot':'primary-grip','bounds_m':[rounded(final.min(0)),rounded(final.max(0))],'dimensions_m':rounded(np.ptp(final,axis=0)),
      'primary_grip_origin':True,'measured_canonical_grip_m':rounded(primary),'pivot_notes':'Actual inspected feature bounds measured from triangle surface samples; only outer-root translation changed. No body poses/equipment/rigging.'})
    digest=hashlib.sha256(model.read_bytes()).hexdigest()
    reviewed_hash=audit.get('visual_review',{}).get('model_sha256')
    if reviewed_hash and reviewed_hash!=digest:
        audit['visual_review']['status']='pending'
        audit['visual_review']['needs_review_reason']='Weapon content/transform changed since the recorded visual-review hash.'
    audit['socket_metadata']='assets/production/weapon-sockets.json';record.write_text(json.dumps(audit,indent=2)+'\n')
    entry={'id':asset,'model_file':audit['model_file'],'model_sha256':hashlib.sha256(model.read_bytes()).hexdigest(),
      'raw_model_file':audit['raw_model_file'],'units':'metres','up':'+Y','forward':'+Z','origin':'primary_grip','sockets':sockets,
      'status':'measured-independent-weapon','equipment_or_hand_fit_performed':False,
      'measurement_precision_note':'Geometric art datums derived from inspected mesh feature bounds, not manufactured grip dimensions or an approved character-hand fit.'}
    if asset=='weapon-carbine':
        entry['barrel_count']=1;entry['fire_alignment']={'axis':[0,0,1],'origins':['muzzle'],'implemented':False}
    if asset=='weapon-shotgun':
        entry['barrel_count']=2;entry['fire_alignment']={'axis':[0,0,1],'origins':['muzzle_left','muzzle_right'],'implemented':False}
    if asset=='weapon-rocket-launcher':
        entry['loaded_state']=True;entry['integral_coral_nose']=True
        entry['fire_alignment']={'axis':[0,0,1],'future_projectile_origin_m':rounded(np.asarray(sockets['loaded_nose_tip']['position_m'])+[0,0,.01]),'implemented':False,'notes':'Static loaded nose retained. Future projectile marker is 1 cm beyond its measured cap; no extraction, discharge or animation.'}
    return entry


def annotate(entries):
    for entry in entries:
        parts=list(primitives(*unpack(ROOT/entry['model_file'])))
        vertices=np.concatenate([p[0] for p in parts]);centre=(vertices.min(0)+vertices.max(0))/2
        direction=np.array([1,0,0]);right=np.cross([0,1,0],direction);up=np.cross(direction,right)
        basis=np.array([right,up,direction]).T;projected=(vertices-centre)@basis
        span=np.ptp(projected[:,:2],axis=0).max()*1.14;offset=(projected[:,:2].min(0)+projected[:,:2].max(0))/2
        image=render(parts,direction);draw=ImageDraw.Draw(image)
        for index,(name,socket) in enumerate(entry['sockets'].items()):
            location=(np.asarray(socket['position_m'])-centre)@basis;pixel=(location[:2]-offset)*512/span
            px,py=float(pixel[0]+256),float(256-pixel[1]);colour=['#ff4030','#14995f','#e29000','#1467cc','#9334c2'][index%5]
            draw.ellipse((px-4,py-4,px+4,py+4),outline=colour,width=2)
            draw.text((8,20+index*18),name,fill=colour);draw.line((135,25+index*18,px,py),fill=colour,width=1)
        evidence=f"assets/production/records/{entry['id']}/review-sockets-side.png"
        image.save(ROOT/evidence);entry['socket_visual_evidence']=evidence

def main():
    entries=[]
    for asset,regions in DEFINITIONS.items():
        entry=measure(asset,regions)
        if entry:entries.append(entry)
    annotate(entries)
    output={'schema_version':1,'scope':'Independent weapon normalization/pivot metadata only. All character bodies remain unarmed true T-pose. Equipping, seating, rigging and animation are deferred.','coordinate_convention':'Right-handed metres, +Y up, nominal muzzle/nose +Z. Socket frames are in normalized weapon-local space.','measurement_method':'Feature selections on actual decoded GLB triangle surfaces, including barycentric interior samples and wood albedo classification where applicable.','weapons':entries}
    (ROOT/'assets/production/weapon-sockets.json').write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps({e['id']:{k:v['position_m'] for k,v in e['sockets'].items()} for e in entries},indent=2))
if __name__=='__main__':main()
