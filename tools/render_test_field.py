"""Render all six walking characters and assembled vehicles in a shared field.

Blender --background --python-exit-code 1 --python tools/render_test_field.py --
  [--still FRAME] [--start FRAME --end FRAME]
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/animation/test-field'
sys.path.insert(0,str(ROOT/'tools'))
sys.path.insert(0,str(ROOT/'tools/animation'))
from render_character_showcase import aim, material, import_actor
from rig import author
from test_field import Oval, character_pose, camera_pose, validate_routes

SOURCES = {}


def empty(name, parent=None):
    obj = bpy.data.objects.new(name,None)
    bpy.context.collection.objects.link(obj)
    obj.parent = parent
    return obj


def import_model(path, name, parent=None):
    path = ROOT/path
    SOURCES[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    imported = set(bpy.data.objects)-before
    wrapper = empty(name,parent)
    for obj in imported:
        if obj.parent not in imported:
            obj.parent = wrapper
    return wrapper


def prop(asset, position, scale=1, yaw=0):
    obj = import_model(Path('assets/models')/asset/(asset+'.glb'),asset)
    obj.location = position
    obj.scale = (scale,)*3
    obj.rotation_euler.z = yaw
    return obj


def texture_material(asset):
    mat = material(asset,(1,1,1))
    nodes = mat.node_tree.nodes
    tex = nodes.new('ShaderNodeTexImage')
    path = ROOT/'assets/textures'/asset/(asset+'.png')
    tex.image = bpy.data.images.load(str(path),check_existing=True)
    tex.extension = 'REPEAT'
    mat.node_tree.links.new(tex.outputs['Color'],nodes['Principled BSDF'].inputs['Base Color'])
    SOURCES[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    return mat


def surface(name, vertices, faces, mat):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices,[],faces)
    mesh.update()
    obj = bpy.data.objects.new(name,mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(mat)
    uv = mesh.uv_layers.new(name='World metres / 2.5m tile')
    for poly in mesh.polygons:
        for loop in poly.loop_indices:
            p = mesh.vertices[mesh.loops[loop].vertex_index].co
            uv.data[loop].uv = (p.x/2.5,p.y/2.5)
    return obj


def cube(name, position, dimensions, mat):
    bpy.ops.mesh.primitive_cube_add(size=1,location=position)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    obj.data.materials.append(mat)
    return obj


def scenery(spec):
    grass,soil = texture_material('texture-grass'),texture_material('texture-soil')
    surface('Field',[(-40,-40,0),(40,-40,0),(40,40,0),(-40,40,0)],[(0,1,2,3)],grass)
    rx,ry = spec['road']['radii'];half = spec['road']['half_width']
    vertices,faces = [],[]
    for i in range(193):
        a = math.tau*i/192
        for sign in [-1,1]:
            vertices.append(((rx+sign*half)*math.cos(a),(ry+sign*half)*math.sin(a),.003))
        if i:
            j = 2*i
            faces.append((j-2,j-1,j+1,j))
    surface('Dirt service loop',vertices,faces,soil)
    paint = material('Quiet cream road paint',(.72,.66,.45))
    for i in range(32):
        a = math.tau*i/32
        p = (rx*math.cos(a),ry*math.sin(a),.008)
        marker = cube('Lane dash',p,(.05,.45,.009),paint)
        marker.rotation_euler.z = math.atan2(-rx*math.sin(a),-ry*math.cos(a))
    # Keep tall scenery behind/outside the review area, with clear sight lines.
    for asset,p,s,yaw in [
        ('barn',(-6.8,11.5,0),.68,-.2),('silo',(-1.6,12.1,0),.65,0),
        ('hay-bale',(-9.5,9.4,0),.8,.3),('hay-bale',(-8,9.7,0),.8,-.2),
        ('tree-round-large',(-12,4.8,0),.85,.8),('tree-round-large',(11.8,5.8,0),.9,2.2),
        ('tree-round-small',(9.7,10.2,0),.8,1.4),('bush-low',(-11.5,-3.2,0),.9,.5),
        ('bush-low',(11.8,-2.4,0),1,2),('field-rock-a',(-11,-6.9,0),.7,.8)]:
        prop(asset,p,s,yaw)
    wood = material('Fence warm wood',(.39,.26,.13))
    fence_x=max(10.8,rx+half+.8)
    for x in [-fence_x,fence_x]:
        for y in range(-8,9,2):
            cube('Fence post',(x,y,.45),(.12,.12,.9),wood)
        for z in [.35,.7]:
            cube('Fence rail',(x,0,z),(.075,16,.075),wood)
    # A few sparse source grass tufts anchor the perimeter without hiding feet.
    for i in range(14):
        a = math.tau*i/14
        prop('grass-tuft',(10.3*math.cos(a),8.7*math.sin(a),.004),.65,a)


def lighting_and_camera(spec):
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE_NEXT'
    scene.eevee.taa_render_samples = 24
    scene.render.resolution_x,scene.render.resolution_y = spec['dimensions']
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.compression = 15
    scene.render.fps = spec['fps']
    scene.view_settings.view_transform = 'AgX'
    scene.world = bpy.data.worlds.new('Farmland sky')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.32,.43,.52,1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .6
    sun = bpy.data.lights.new('Warm afternoon','SUN')
    sun.energy,sun.angle,sun.color = 2.0,.12,(1,.87,.71)
    obj = bpy.data.objects.new(sun.name,sun);bpy.context.collection.objects.link(obj)
    obj.rotation_euler = (.48,-.55,-.45)
    fill = bpy.data.lights.new('Soft sky fill','AREA')
    fill.energy,fill.size,fill.color = 6500,18,(.70,.82,1)
    obj = bpy.data.objects.new(fill.name,fill);bpy.context.collection.objects.link(obj)
    obj.location = (-4,-8,12);aim(obj,(0,0,0))
    data = bpy.data.cameras.new('Review camera')
    camera = bpy.data.objects.new('Review camera',data)
    bpy.context.collection.objects.link(camera)
    data.type = 'ORTHO'
    scene.camera = camera


def assembly(definition, parent):
    wrappers,spinners = {},[]
    sockets = {s['id']:s for s in definition.get('sockets',[])}
    for node in definition['nodes']:
        path = ROOT/node['model_file']
        if hashlib.sha256(path.read_bytes()).hexdigest() != node['source_sha256']:
            raise ValueError('Assembly source drift: '+node['model_file'])
        if 'attach_to_socket' in node:
            socket = sockets[node['attach_to_socket']]
            mount = empty(node['id']+'_Mount',wrappers[socket['parent_node']])
            mount.location = author(socket['translation_m'])
            mount.rotation_euler.z = math.radians(socket.get('yaw_degrees',0))
        else:
            mount = empty(node['id']+'_Mount',parent)
            mount.location = author(node.get('translation_m',[0,0,0]))
        obj = import_model(Path(node['model_file']),node['id'],mount)
        wrappers[node['id']] = mount
        if node['asset'] == 'jeep-wheel':
            spinners.append((mount,obj,1 if 'right' in node['id'] else -1))
    return wrappers,spinners


def actor(c, definitions):
    rig,actions = import_actor(c,0)
    display = rig.parent
    display.scale = (1,1,1);display.location = (0,0,0);display.rotation_euler = (0,0,0)
    path = ROOT/'build/animation/exports'/(c['id']+'.glb')
    SOURCES[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    hat,rest = None,None
    if c.get('headwear'):
        definition = next(d for d in definitions if d['nodes'][0]['asset'] == c['id'])
        node = next(n for n in definition['nodes'] if n['asset'] == c['headwear'])
        socket = definition['sockets'][0]
        hat = import_model(Path(node['model_file']),c['headwear'],display)
        rest = Matrix.Translation(author(socket['translation_m'])) @ Matrix.Rotation(
            math.radians(socket.get('yaw_degrees',0)),4,'Z')
    return {'config':c,'rig':rig,'display':display,'actions':actions,'hat':hat,'hat_rest':rest}


def pose_actor(a, seconds):
    if 'weapon' in a:
        from render_infantry_review import raw_pose
        raw_pose(a,'walk',seconds,True)
        return
    rig = a['rig'];action,slot = a['actions']['walk']
    start,end = action.frame_range
    f = start+(seconds % ((end-start)/30))*30
    for fc in action.layers[0].strips[0].channelbag(slot).fcurves:
        if fc.data_path.endswith(('rotation_quaternion','location')):
            rig.path_resolve(fc.data_path)[fc.array_index] = fc.evaluate(f)
    rig.animation_data.action = None


def set_frame(spec, characters, vehicles, oval, seconds, camera_seconds=None):
    for route,a in zip(spec['characters'],characters):
        position,yaw,gait = character_pose(route,a['config'],seconds)
        a['display'].location = (*position,0)
        a['display'].rotation_euler.z = yaw
        pose_actor(a,gait)
    for v in vehicles:
        d = oval.distance_at(v['config']['phase'])+v['config']['speed_mps']*seconds
        position,yaw,curvature = oval.sample(d)
        v['root'].location = (*position,0);v['root'].rotation_euler.z = yaw
        for mount,wheel,side in v['wheels']:
            mount.rotation_euler.z = (0 if side==1 else math.pi)+(math.atan(2.06*curvature) if 'front' in mount.name else 0)
            wheel.rotation_euler.x = seconds*v['config']['speed_mps']/.3*side
        if v['config']['id']=='tank':
            v['parts']['tank-turret'].rotation_euler.z = .24*math.sin(seconds*.55)
    camera = bpy.context.scene.camera
    eye,focus,width,chapter = camera_pose(seconds if camera_seconds is None else camera_seconds)
    camera.location = eye;aim(camera,focus);camera.data.ortho_scale = width
    bpy.context.view_layer.update()
    for a in characters:
        if 'weapon' in a:
            rig=a['rig'];socket=rig.pose.bones['socket_weapon']
            delta=socket.matrix@socket.bone.matrix_local.inverted()
            a['weapon'].matrix_basis=rig.matrix_local@delta@rig.matrix_local.inverted()
        if a['hat']:
            rig = a['rig'];head = rig.pose.bones['head']
            delta = head.matrix @ head.bone.matrix_local.inverted()
            a['hat'].matrix_basis = rig.matrix_local @ delta @ rig.matrix_local.inverted() @ a['hat_rest']
    bpy.context.view_layer.update()
    return chapter


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--still',type=int)
    parser.add_argument('--start',type=int,default=1)
    parser.add_argument('--end',type=int)
    parser.add_argument('--output',type=Path,default=OUT)
    parser.add_argument('--camera-time-offset',type=float,default=0)
    parser.add_argument('--include-infantry',action='store_true')
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    spec = json.loads((ROOT/'assets/animation/test-field.json').read_text())
    config = json.loads((ROOT/'assets/animation/characters.json').read_text())
    definitions = json.loads((ROOT/'assets/production/assemblies.json').read_text())['assemblies']
    chars = {c['id']:c for c in config['characters']}
    fits={}
    if args.include_infantry:
        fits={f['id']:f for f in json.loads((ROOT/'assets/animation/infantry-fit.json').read_text())['bodies']}
        # A wider review loop gives all nine source-sized bodies three rows
        # without placing the new patrol inside a vehicle's swept footprint.
        spec['road']['radii']=[9.7,8.6]
        for i,r in enumerate(spec['characters']):r['center'][1]=-3.6 if i<3 else 0
        for i,(name,fit) in enumerate(fits.items()):
            report=json.loads((ROOT/'build/animation/infantry/reports'/(name+'.json')).read_text())
            clip=next(c for c in report['clips'] if c['name']=='walk')
            chars[name]={'id':name,'height':fit['bounds_m'][1][1],
                         'stride_m':clip['stride_m'],'walk_seconds':clip['seconds']}
            spec['characters'].append({'id':name,'center':[-3.4+i*3.4,3.6],
                                       'radius':.75,'phase':1.7+i*1.6,'gait_phase':i*.3})
    validation = validate_routes(spec,chars)
    output = args.output.resolve()
    output.mkdir(parents=True,exist_ok=True);(output/'frames').mkdir(exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    lighting_and_camera(spec);scenery(spec)
    actors=[]
    for r in spec['characters']:
        if r['id'] in fits:
            from render_infantry_review import load_actor
            a=load_actor(fits[r['id']],ROOT/'build/animation/infantry/exports',
                         ROOT/'build/animation/infantry/reports',0,SOURCES)
            a.update(config=chars[r['id']],hat=None,hat_rest=None)
        else:a=actor(chars[r['id']],definitions)
        actors.append(a)
    vehicles = []
    for v in spec['vehicles']:
        root = empty(v['id'])
        definition = next(d for d in definitions if d['id']==v['assembly'])
        parts,wheels = assembly(definition,root)
        vehicles.append({'config':v,'root':root,'parts':parts,'wheels':wheels})
    oval = Oval(spec['road']['radii'])
    count = round(spec['duration_seconds']*spec['fps'])
    frames = [args.still] if args.still else range(args.start,(args.end or count)+1)
    from bpy_extras.object_utils import world_to_camera_view
    for frame in frames:
        t = (frame-1)/spec['fps']
        chapter = set_frame(spec,actors,vehicles,oval,t,t+args.camera_time_offset)
        labels = []
        for a in actors:
            p = a['display'].location.copy()
            p.z = a['config']['height']+(.52 if a['config'].get('headwear') else .22)
            q = world_to_camera_view(bpy.context.scene,bpy.context.scene.camera,p)
            labels.append({'id':a['config']['id'],'position':[q.x*1280,(1-q.y)*720]})
        for v in vehicles:
            p = v['root'].location.copy();p.z = 1.8 if v['config']['id']=='jeep' else 2.55
            q = world_to_camera_view(bpy.context.scene,bpy.context.scene.camera,p)
            labels.append({'id':v['config']['id'],'position':[q.x*1280,(1-q.y)*720]})
        bpy.context.scene.render.filepath = str(output/'frames'/f'{frame:05d}.png')
        bpy.ops.render.render(write_still=True)
        (output/'frames'/f'{frame:05d}.json').write_text(json.dumps({'chapter':chapter,'labels':labels}))
        if frame%30==1:print(f'FIELD_FRAME {frame}/{count}',flush=True)
    (output/'scene-report.json').write_text(json.dumps({'sources':SOURCES,'routes':validation,
        'rendered_frame_range':[frames[0],frames[-1]],'camera_time_offset':args.camera_time_offset,
        'spec_sha256':hashlib.sha256((ROOT/'assets/animation/test-field.json').read_bytes()).hexdigest(),
        'effective_spec':spec,
        'renderer_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'notes':'Offline world fixture. Physical source scale; imported walk clips; moving rigid vehicles, rolling jeep wheels and independent turret. Tank treads remain fused source geometry.'},indent=2)+'\n')
    if not args.still:
        set_frame(spec,actors,vehicles,oval,0)
        bpy.ops.file.pack_all()
        bpy.ops.wm.save_as_mainfile(filepath=str(output/'test-field.blend'))


if __name__=='__main__':
    main()
