"""Close, moving-camera review of the actual exported shared walk clips.

Blender --background --python-exit-code 1 --python tools/render_walk_review.py
  -- --group humans --start 1 --end 180 [--only farmer-body] [--sheet]
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from render_character_showcase import aim, material, lighting, import_actor
from render_test_field import pose_actor
from render_infantry_review import load_actor, raw_pose


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--group',choices=['humans','animals','infantry'],required=True)
    p.add_argument('--only')
    p.add_argument('--sheet',action='store_true')
    p.add_argument('--check-only',action='store_true')
    p.add_argument('--output',type=Path,default=ROOT/'build/animation/walk-v3')
    p.add_argument('--start',type=int,default=1)
    p.add_argument('--end',type=int,default=180)
    args = p.parse_args(sys.argv[sys.argv.index('--')+1:])
    config = json.loads((ROOT/'assets/animation/characters.json').read_text())
    chars = [c for c in config['characters'] if (c['rig']=='humanoid')==(args.group=='humans')
             and (not args.only or c['id']==args.only)]
    if args.group=='infantry':
        fits=json.loads((ROOT/'assets/animation/infantry-fit.json').read_text())['bodies']
        chars=[{'id':f['id'],'height':f['bounds_m'][1][1],'walk_seconds':1.1,'fit':f}
               for f in fits if not args.only or f['id']==args.only]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE_NEXT'
    scene.eevee.taa_render_samples = 24
    scene.render.resolution_x,scene.render.resolution_y = 1280,720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.fps = 30
    scene.world = bpy.data.worlds.new('Review sky')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.15,.20,.26,1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .5
    scene.view_settings.view_transform = 'AgX'
    bpy.ops.mesh.primitive_plane_add(size=200)
    bpy.context.object.data.materials.append(material('Slate floor',(.07,.10,.13)))
    # Visible metre marks make foot sliding evident as the camera follows travel.
    marks = material('Metre marks',(.22,.29,.31))
    for x in range(-10,20):
        bpy.ops.mesh.primitive_cube_add(size=1,location=(x,0,-.001))
        bpy.context.object.dimensions = (.012,30,.002)
        bpy.context.object.data.materials.append(marks)
    lighting()
    data = bpy.data.cameras.new('Review camera')
    camera = bpy.data.objects.new('Review camera',data)
    bpy.context.collection.objects.link(camera);scene.camera = camera;data.type = 'ORTHO'
    actors = []
    for i,c in enumerate(chars):
        if args.group=='infantry':
            a=load_actor(c['fit'],ROOT/'build/animation/infantry/exports',
                         ROOT/'build/animation/infantry/reports',0,{})
            c['stride_m']=a['stride_m'];a['config']=c
            rig,actions,display=a['rig'],a['actions'],a['display']
        else:
            rig,actions = import_actor(c,0)
            display = rig.parent
            a={'config':c,'rig':rig,'display':display,'actions':actions}
        display.scale = (1,)*3
        display.rotation_euler.z = math.pi/2
        actors.append(a)
    dest = args.output/('poses-'+args.only if args.only else args.group)
    dest.mkdir(parents=True,exist_ok=True)
    frames = range(1,9) if args.sheet else range(args.start,args.end+1)
    for frame in frames:
        t = (frame-1)/8*chars[0]['walk_seconds'] if args.sheet else (frame-1)/30
        travel = []
        for i,a in enumerate(actors):
            c = a['config']
            if args.group=='infantry':raw_pose(a,'walk',t,True)
            else:pose_actor(a,t)
            x = c['stride_m']/c['walk_seconds']*t
            travel.append(x)
            a['display'].location = (x+(i-(len(actors)-1)/2)*2.5,0,0)
        center = sum(travel)/len(travel)
        bpy.context.view_layer.update()
        for a in actors:
            if 'weapon' in a:
                rig=a['rig'];socket=rig.pose.bones['socket_weapon']
                delta=socket.matrix@socket.bone.matrix_local.inverted()
                a['weapon'].matrix_basis=rig.matrix_local@delta@rig.matrix_local.inverted()
        focus = (center,0,.48 if args.group=='animals' else .82)
        diagonal = 0 if args.sheet else max(0,min(1,(t-2.5)/1.5))
        camera.location = (center+diagonal*4,-10,3.0)
        aim(camera,focus)
        data.ortho_scale = (3.4 if args.group=='humans' else 3.0) if args.only else {
            'humans':7.0,'animals':7.4,'infantry':9.2}[args.group]
        bpy.context.view_layer.update()
        # Check actual evaluated vertices: corners of a rotated long weapon's
        # bounding box include empty space and produce false crop failures.
        depsgraph=bpy.context.evaluated_depsgraph_get()
        projection=camera.calc_matrix_camera(depsgraph,x=1280,y=720)
        view=projection@camera.matrix_world.inverted()
        for a in actors:
            for obj in a['display'].children_recursive:
                if obj.type!='MESH':continue
                evaluated=obj.evaluated_get(depsgraph)
                skin=evaluated.to_mesh()
                positions=np.empty(len(skin.vertices)*3,dtype=np.float32)
                skin.vertices.foreach_get('co',positions)
                evaluated.to_mesh_clear()
                positions=np.column_stack((positions.reshape(-1,3),np.ones(len(positions)//3)))
                clip=positions@np.array(view@evaluated.matrix_world).T
                q=clip[:,:2]/clip[:,3,None]*.5+.5
                lo,hi=q.min(0),q.max(0)
                if not (.015<lo[0] and hi[0]<.985 and .10<lo[1] and hi[1]<.875):
                    raise ValueError(f"Walk review crops {a['config']['id']} at frame {frame}: {lo}, {hi}")
        if not args.check_only:
            scene.render.filepath = str(dest/f'{frame:05d}.png')
            bpy.ops.render.render(write_still=True)
    exports=ROOT/('build/animation/infantry/exports' if args.group=='infantry' else 'build/animation/exports')
    (dest/'framing-validation.json').write_text(json.dumps({
        'status':'passed','frames':[frames[0],frames[-1]],'check_only':args.check_only,
        'normalized_content_rectangle':[.015,.10,.985,.875],
        'export_sha256':{c['id']:hashlib.sha256((exports/(c['id']+'.glb')).read_bytes()).hexdigest() for c in chars},
        'renderer_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},indent=2)+'\n')
    print('WALK_REVIEW_COMPLETE',dest,flush=True)


if __name__=='__main__':main()
