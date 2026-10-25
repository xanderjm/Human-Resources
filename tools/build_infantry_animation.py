"""Isolated infantry authoring; run in Blender --python this_file -- [--only ID].

Body-only GLBs retain measured weapon/control sockets; packed .blend files
contain the independently sourced equipped weapon. No shipping/native edits.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import shutil
import subprocess
import bpy

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
sys.path.insert(0,str(ROOT/'tools/animation'))
from transaction_character_animation import locked_output,staging,publish
from rig import import_character
from infantry_rig import build as build_rig
from infantry_motion import create_actions


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def build(config,spec,fit,output):
    for name,key in [('body_source','model_sha256'),('weapon_source','weapon_sha256')]:
        expected=fit[key] if key in fit else fit['equipment_candidate'][key]
        if digest(ROOT/config[name])!=expected:raise ValueError('Source changed: '+config[name])
    inputs=[Path(__file__),ROOT/'tools/animation/infantry_rig.py',ROOT/'tools/animation/infantry_motion.py',
            ROOT/'assets/animation/infantry.json',ROOT/spec['fit_source'],ROOT/'assets/production/weapon-sockets.json',
            ROOT/'tools/animation/rig.py',ROOT/'tools/animation/motion.py',ROOT/'tools/animation/walk.py',
            ROOT/'tools/validate_infantry_animation.py',ROOT/'tools/transaction_character_animation.py']
    before={str(p.relative_to(ROOT)):digest(p) for p in inputs}
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene=bpy.context.scene;scene.render.fps=spec['fps']
    mesh,points=import_character(ROOT,{'id':config['id']})
    rig=build_rig(mesh,points,fit,config['weapon_sockets'])
    clips=create_actions(rig,mesh,fit,config,spec,spec['walk_profile'])
    rig['units']='metres';rig['forward']='+Z in glTF';rig['recipe']='assets/animation/infantry.json'
    rig['weapon_source']=config['weapon_source'];rig['weapon_socket']='socket_weapon'
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);mesh.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.gltf(filepath=str(output/'exports'/(config['id']+'.glb')),
        export_format='GLB',use_selection=True,export_animations=True,export_animation_mode='ACTIONS',
        export_force_sampling=True,export_anim_slide_to_zero=True,export_frame_range=False,
        export_frame_step=1,export_skins=True,export_def_bones=False,export_yup=True,
        export_materials='EXPORT',export_extras=True,export_optimize_animation_size=False)
    # A rigidly weighted raw weapon in the editable scene uses the exact same
    # posed * inverse(rest) frame that a native attachment consumer will use.
    weapon,_=import_character(ROOT,{'id':config['weapon_id']})
    group=weapon.vertex_groups.new(name='socket_weapon')
    group.add(list(range(len(weapon.data.vertices))),1,'REPLACE')
    weapon.parent=rig;mod=weapon.modifiers.new('Rigid weapon socket','ARMATURE');mod.object=rig
    mod.use_deform_preserve_volume=False
    rig.animation_data.action=bpy.data.actions['idle'];scene.frame_start=1;scene.frame_end=91
    scene.frame_set(1);bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(output/'blend'/(config['id']+'.blend')))
    report={'id':config['id'],'body_source':config['body_source'],'body_source_sha256':digest(ROOT/config['body_source']),
            'weapon_source':config['weapon_source'],'weapon_source_sha256':digest(ROOT/config['weapon_source']),
            'config':config,'clips':clips,'vertices':len(mesh.data.vertices),'bones':len(rig.data.bones),
            'triangles':sum(len(p.vertices)-2 for p in mesh.data.polygons),
            'authoring_inputs':before,
            'limits':['Open mitten source; broad distal fold, no individual trigger fingers',
                      'Current native soldier unchanged; candidate body GLB plus separate weapon source',
                      'Cross-body yaw is intentional; consumer must aim using actual muzzle socket axes']}
    (output/'reports'/(config['id']+'.json')).write_text(json.dumps(report,indent=2)+'\n')
    print('INFANTRY_COMPLETE',config['id'],flush=True)


def manifest(spec,stage,target,selected):
    entries=[];inputs={}
    for c in selected:
        report_path=stage/'reports'/(c['id']+'.json');report=json.loads(report_path.read_text())
        body=stage/'exports'/(c['id']+'.glb');inputs.update(report['authoring_inputs'])
        entries.append({'id':c['id'],'body_export':str((target/'exports'/body.name).relative_to(ROOT)),
            'body_export_sha256':digest(body),'report':str((target/'reports'/report_path.name).relative_to(ROOT)),
            'report_sha256':digest(report_path),'body_source':report['body_source'],
            'body_source_sha256':report['body_source_sha256'],'weapon_source':report['weapon_source'],
            'weapon_source_sha256':report['weapon_source_sha256'],
            'clips':[{k:v for k,v in clip.items() if k!='contact_samples'} for clip in report['clips']],
            'raw_weapon_socket_datums_m':c['weapon_sockets']})
    return {'schema':1,'status':'validated-frozen-authoring-set' if len(selected)==len(spec['variants']) else 'partial-authoring-set',
        'producer':'tools/build_infantry_animation.py','output_root':str(target.relative_to(ROOT)),
        'coordinates':'Metres, +Y up, +Z source weapon forward; D=posed socket_weapon global * inverse(rest global). Actor transform once outside D.',
        'producer_inputs_sha256':inputs,'variants':entries,
        'publication_contract':'All producer/validator/consumer snapshots acquire transaction_character_animation.locked_output(output_root). Producer stages a complete set, validates with art Python, rechecks inputs, then publishes under the same lock with journal rollback. Consumer verifies this manifest, copies exports/reports under the lock, then cooks from its own snapshot.',
        'legacy_encoder_adapter':'Alias source=body_source and source_sha256=body_source_sha256 only in consumer-owned report copies; use the fourteen-clip infantry recipe. Legacy six inputs/cooker/native contract are unchanged.',
        'motion_contract':'Planted full-body aim/fire. Walk stride follows authoritative travel; capture preempts full-body. Neutral aim reference drives an upper chest elevation; preserve authored chest/gun fire recoil after this correction. No seated clip.'}


def main():
    p=argparse.ArgumentParser();p.add_argument('--only');p.add_argument('--output-root',type=Path)
    p.add_argument('--validator-python',type=Path,default=Path(shutil.which('python3') or 'python3'))
    a=p.parse_args(sys.argv[sys.argv.index('--')+1:] if '--'in sys.argv else[])
    spec=json.loads((ROOT/'assets/animation/infantry.json').read_text())
    output=(a.output_root or ROOT/spec['output_root']).resolve()
    # Output paths are repo-local authoring derivatives, never shipping roots.
    output.relative_to(ROOT)
    with locked_output(output),staging(output) as stage:
        spec=json.loads((ROOT/'assets/animation/infantry.json').read_text())
        if digest(ROOT/spec['fit_source'])!=spec['fit_sha256']:raise ValueError('Fit recipe changed')
        fits={f['id']:f for f in json.loads((ROOT/spec['fit_source']).read_text())['bodies']}
        selected=[c for c in spec['variants'] if not a.only or c['id']==a.only]
        if not selected:raise ValueError('Unknown infantry ID')
        for name in ['exports','blend','reports']:(stage/name).mkdir()
        for config in selected:build(config,spec,fits[config['id']],stage)
        command=[str(a.validator_python),str(ROOT/'tools/validate_infantry_animation.py'),'--output-root',str(stage)]
        if a.only:command+=['--only',a.only]
        subprocess.run(command,cwd=ROOT,check=True)
        for c in selected:
            report=json.loads((stage/'reports'/(c['id']+'.json')).read_text())
            sources={**report['authoring_inputs'],report['body_source']:report['body_source_sha256'],report['weapon_source']:report['weapon_source_sha256']}
            for path,expected in sources.items():
                if digest(ROOT/path)!=expected:raise ValueError('Infantry input changed during generation: '+path)
        (stage/'producer-manifest.json').write_text(json.dumps(manifest(spec,stage,output,selected),indent=2)+'\n')
        files=[p.relative_to(stage) for p in stage.rglob('*') if p.is_file() and not p.name.startswith('.animation-')]
        files.sort(key=lambda p:p.as_posix()=='producer-manifest.json')
        publish(output,stage,files)
    print('INFANTRY_PUBLISHED',output,flush=True)


if __name__=='__main__':main()
