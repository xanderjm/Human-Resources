"""Run with Blender --background --python this_file -- [--only ID].

Recipe: assets/animation/characters.json. Generated exports, editable packed
Blender files and verification live under ignored build/animation/.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import shutil
import subprocess
import bpy

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
sys.path.insert(0, str(ROOT/'tools/animation'))
from transaction_character_animation import locked_output, staging, publish
from rig import import_character, build_rig
from motion import create_actions


def build(c, spec, output):
    inputs=[Path(__file__), ROOT/'tools/animation/rig.py', ROOT/'tools/animation/motion.py',
            ROOT/'tools/animation/walk.py', ROOT/'assets/animation/walk-profiles.json',
            ROOT/'assets/animation/characters.json', ROOT/'tools/transaction_character_animation.py']
    source=ROOT/'assets/models'/c['id']/(c['id']+'.glb')
    before={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs+[source]}
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = spec['fps']
    mesh, points = import_character(ROOT, c)
    rig = build_rig(mesh, points, c)
    clips = create_actions(rig, mesh, c, spec)
    rig['units'] = 'metres'
    rig['forward'] = '-Y in Blender; +Z in glTF'
    rig['animation_recipe'] = 'assets/animation/characters.json'
    # Each named action is exported independently; root translation is intentional.
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.gltf(
        filepath=str(output/'exports'/(c['id']+'.glb')),
        export_format='GLB', use_selection=True, export_animations=True,
        export_animation_mode='ACTIONS', export_force_sampling=True,
        export_anim_slide_to_zero=True,
        export_frame_range=False, export_frame_step=1,
        export_skins=True, export_def_bones=False, export_yup=True,
        export_materials='EXPORT', export_extras=True,
        export_optimize_animation_size=False)
    # Save a friendly editable file with idle selected, all actions retained.
    rig.animation_data.action = bpy.data.actions['idle']
    scene.frame_start, scene.frame_end = 1, 91
    scene.frame_set(1)
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(output/'blend'/(c['id']+'.blend')))
    source = ROOT/'assets/models'/c['id']/(c['id']+'.glb')
    result = {'id': c['id'], 'source': str(source.relative_to(ROOT)),
              'source_sha256': before[str(source.relative_to(ROOT))],
              'vertices': len(mesh.data.vertices), 'bones': len(rig.data.bones),
              'triangles': sum(len(p.vertices)-2 for p in mesh.data.polygons),
              'clips': clips, 'config': c,
              'authoring_inputs': {str(p.relative_to(ROOT)):before[str(p.relative_to(ROOT))] for p in inputs}}
    (output/'reports'/(c['id']+'.json')).write_text(json.dumps(result, indent=2)+'\n')
    print('CHARACTER_COMPLETE '+c['id'], flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--only')
    parser.add_argument('--output-root',type=Path)
    parser.add_argument('--validator-python',type=Path,default=Path(shutil.which('python3') or 'python3'))
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    spec = json.loads((ROOT/'assets/animation/characters.json').read_text())
    output = (args.output_root or ROOT/spec['output_root']).resolve()
    selected = [c for c in spec['characters'] if not args.only or c['id']==args.only]
    if not selected:raise ValueError('Unknown character '+str(args.only))
    with locked_output(output), staging(output) as stage:
        # A queued invocation must read the recipe again after acquiring ownership.
        spec = json.loads((ROOT/'assets/animation/characters.json').read_text())
        selected = [c for c in spec['characters'] if not args.only or c['id']==args.only]
        if not selected:raise ValueError('Unknown character '+str(args.only))
        for directory in ['exports','blend','reports']:(stage/directory).mkdir()
        for c in selected:build(c,spec,stage)
        # Blender's bundled Python lacks Pillow. Use the existing art Python;
        # validators lock only this unique staging root, never the parent's root.
        for tool in ['validate_character_animation.py','validate_shared_walks.py']:
            command=[str(args.validator_python),str(ROOT/'tools'/tool),'--output-root',str(stage)]
            if args.only:command+=['--only',args.only]
            subprocess.run(command,cwd=ROOT,check=True)
        # Recheck every input after generation; edited sources cannot publish
        # artifacts from a mixed authoring generation.
        for c in selected:
            report=json.loads((stage/'reports'/(c['id']+'.json')).read_text())
            for name,expected in {**report['authoring_inputs'],report['source']:report['source_sha256']}.items():
                if hashlib.sha256((ROOT/name).read_bytes()).hexdigest()!=expected:
                    raise ValueError('Authoring input changed during generation: '+name)
        files=[p.relative_to(stage) for p in stage.rglob('*') if p.is_file() and not p.name.startswith('.animation-')]
        files.sort(key=lambda p:p.as_posix()=='reports/validation.json')
        publish(output,stage,files)



if __name__ == '__main__':
    main()
