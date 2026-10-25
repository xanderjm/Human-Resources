"""Build the UFO as editable, closed quad surfaces and a glTF part hierarchy.

Run with Python + NumPy and the saved paint maps. No service or Blender is required.
The JSON/OBJ retain quads; only the runtime GLBs are triangulated.
"""
from __future__ import annotations

import copy
import io
import json
import math
import struct
from collections import Counter
from functools import lru_cache
from pathlib import Path

import numpy as np
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/models/ufo-modular'
TAU = math.tau
MESHES = {}
NODES = []
RING_RADIUS = 1.3
MATERIALS = [
    {'name':'Cobalt_Armour_Matte','texture':'mapped-ring.png','color':[.82,.92,1,1],'metallic':.04,'roughness':.84},
    {'name':'Navy_Frame','texture':'dome-enamel.png','color':[.22,.30,.43,1],'metallic':.08,'roughness':.74},
    {'name':'Mint_Lens','texture':'mint-lens.png','color':[.30,.85,.70,1],'metallic':0,'roughness':.72,
     'emissive':[0,.28,.20]},
    {'name':'Satin_Blue_Edge','color':[.09,.17,.24,1],'metallic':.12,'roughness':.65},
    {'name':'Painted_Cobalt_Dome','texture':'mapped-dome.png','color':[.85,.94,1,1],
     'metallic':0,'roughness':.68},
    {'name':'Slate_Cover_Plates','texture':'expressive-blue.png','color':[.54,.63,.74,1],'metallic':.04,'roughness':.88},
    {'name':'Light_Cover_Plates','texture':'expressive-blue.png','color':[.80,.90,1,1],'metallic':.04,'roughness':.80},
    {'name':'Cobalt_Module_Paint','texture':'expressive-blue.png','color':[.72,.84,.94,1],'metallic':.04,'roughness':.84},
    {'name':'Exposed_Blue_Grey_Metal','color':[.23,.31,.37,1],'metallic':.15,'roughness':.72},
    {'name':'Cobalt_Cover_Plates','texture':'expressive-blue.png','color':[.68,.80,.93,1],'metallic':.04,'roughness':.88},
    {'name':'Dome_Light_Paint','texture':'mapped-dome.png','color':[.94,.98,1,1],'metallic':0,'roughness':.72},
    {'name':'Dome_Shadow_Paint','texture':'mapped-dome.png','color':[.75,.85,.95,1],'metallic':0,'roughness':.72},
    {'name':'Teal_Lens_Bezel','color':[.025,.20,.20,1],'metallic':.06,'roughness':.74},
    {'name':'Warm_Ochre_Underpaint','color':[.46,.22,.085,1],'metallic':0,'roughness':.92},
]


def texture():
    """Keep authored raster textures intact; rebuilding never generates noise."""
    for material in MATERIALS:
        if 'texture' in material:
            path=OUT/'materials'/material['texture']
            if not path.is_file():raise FileNotFoundError(f'Missing authored paint texture: {path}')


@lru_cache(maxsize=None)
def encoded_texture(name):
    """Encode for delivery; the original ImageGen PNG remains untouched."""
    output=io.BytesIO()
    with Image.open(OUT/'materials'/name) as image:
        image.convert('RGB').save(output,format='JPEG',quality=95,subsampling=0,optimize=True)
    return output.getvalue()


def lathe(name, profile, segments=48, finish=0, bands=None, closed=True, smooth=False):
    """Closed cross-section about local +Y. Pole rows become triangle fans."""
    verts, rows, faces, uvs, finishes = [], [], [], [], []
    for r, y in profile:
        row = []
        for i in range(1 if r == 0 else segments):
            a = TAU * i / segments
            row.append(len(verts))
            verts.append([r * math.cos(a), y, r * math.sin(a)])
        rows.append(row)
    steps = len(rows) if closed else len(rows)-1
    for j in range(steps):
        j2 = (j+1) % len(rows)
        v0, v1 = j / len(rows), (j+1) / len(rows)
        for i in range(segments):
            k = (i+1) % segments
            if len(rows[j]) == 1:
                face = [rows[j][0], rows[j2][i], rows[j2][k]]
                uv = [[(i+.5)/segments, v0], [i/segments,v1], [(i+1)/segments,v1]]
            elif len(rows[j2]) == 1:
                face = [rows[j][i], rows[j2][0], rows[j][k]]
                uv = [[i/segments,v0], [(i+.5)/segments,v1], [(i+1)/segments,v0]]
            else:
                face = [rows[j][i], rows[j2][i], rows[j2][k], rows[j][k]]
                uv = [[i/segments,v0], [i/segments,v1], [(i+1)/segments,v1], [(i+1)/segments,v0]]
            faces.append(face)
            uvs.append(uv)
            finishes.append(bands.get(j, finish) if bands else finish)
    MESHES[name] = dict(vertices=verts, faces=faces, uvs=uvs, finishes=finishes, smooth=smooth)
    return name


def node(name, mesh=None, parent=None, position=(0,0,0), rotation=None, extras=None):
    n = dict(name=name, translation=list(position))
    if mesh: n['mesh_name'] = mesh
    if rotation: n['rotation'] = list(rotation)
    if extras: n['extras'] = extras
    index = len(NODES)
    NODES.append(n)
    if parent is not None: NODES[parent].setdefault('children', []).append(index)
    return index


def quat(axis, angle):
    a = np.array(axis, dtype=float)
    a /= np.linalg.norm(a)
    return [*(a * math.sin(angle/2)), math.cos(angle/2)]


def qmul(a,b):
    av, bv = np.array(a[:3]), np.array(b[:3])
    return [*(a[3]*bv+b[3]*av+np.cross(av,bv)), a[3]*b[3]-float(av@bv)]


def rotate(q, v):
    xyz = np.array(q[:3])
    return np.array(v) + 2*np.cross(xyz, np.cross(xyz,v)+q[3]*np.array(v))


def widen_radius(r):
    return .604+(r-.604)*(RING_RADIUS-.604)/(1.1-.604)


def make_geometry():
    MESHES.clear()
    # A true annular opening is retained for the abduction emitter.
    lathe('Hull', [(.175,.17),(.36,0),(.50,.10),(.568,.19),(.584,.27),
                   (.584,.57),(.54,.61),(.175,.61)], bands={0:1,1:1,6:1,7:1})
    lathe('Dome_Collar', [(.49,.58),(.55,.58),(.57,.60),(.57,.628),
                          (.55,.648),(.49,.648)], finish=7)
    lathe('Dome', [(0,.615),(.505,.615),(.505,.65),(.474,.74),(.409,.835),
                   (.321,.918),(.213,.974),(.101,1.006),(0,1.016)],
          segments=20, closed=False,finish=4)
    dome=MESHES['Dome']
    for j in range(2,8):
        for i in range(20):
            if j in [4,5] and i in [3,4]:dome['finishes'][j*20+i]=10
            elif j in [2,3,4,5] and i in [9,10,11,12,13]:dome['finishes'][j*20+i]=11
    # Its entire lower shell rotates with the rim; the static centre stays inside r=.584.
    lathe('Outer_Ring', [(.604,.207),(.76,0),(.85,.142),(1.04,.18),(1.10,.22),
                         (1.10,.355),(1.075,.395),(.98,.462),(.80,.54),
                         (.68,.581),(.604,.581)],
          bands={0:1,1:7,2:7,3:0,4:0,5:0,10:1})
    lathe('Bearing', [(.56,.49),(.587,.49),(.587,.58),(.575,.60),(.56,.60)], finish=1)
    # Local y=0 is each replaceable lens's mounting face.
    lathe('Lens_Bezel', [(0,-.009),(.137,-.009),(.148,.002),(.148,.016),
                         (.133,.028),(0,.028)], segments=16, finish=12, closed=False)
    lathe('Lens', [(0,.019),(.125,.019),(.125,.032),(.104,.064),
                   (.065,.081),(0,.086)], segments=16, finish=2, closed=False)
    lathe('Antenna_Stem', [(0,0),(.035,0),(.028,.255),(0,.255)],
          segments=8,finish=7,closed=False)
    lathe('Antenna_Tip', [(0,-.082),(.048,-.065),(.078,-.025),(.081,.022),
                          (.057,.063),(0,.082)], segments=12,finish=2,closed=False)
    # Underbody emitter: 650 mm across, unobstructed bore, mounted at its top face.
    lathe('Beam_Housing', [(.166,-.114),(.28,-.114),(.325,-.086),(.325,-.028),
                           (.30,0),(.166,0)],segments=32,bands={0:1,4:1,5:1})
    lathe('Beam_Lens', [(0,0),(.046,0),(.051,.012),(.04,.025),(0,.031)],
          segments=16,finish=2,closed=False)
    # Lasers use +Z as muzzle direction; their local rear mounting pivot is z=0.
    lathe('Laser_Base', [(0,0),(.152,0),(.177,.028),(.177,.108),(.15,.132),(0,.132)],
          segments=8,closed=False)
    lathe('Laser_Barrel', [(0,.10),(.112,.10),(.133,.14),(.133,.40),
                           (.146,.434),(.146,.52),(.12,.55),(.099,.55),
                           (.099,.498),(0,.498)],segments=16,closed=False,
          bands={1:3,4:3,6:1,7:1,8:1})
    lathe('Laser_Lens', [(0,.497),(.092,.497),(.092,.518),(.072,.535),(0,.54)],
          segments=16,finish=2,closed=False)
    for name in ['Laser_Base','Laser_Barrel','Laser_Lens']:
        for v in MESHES[name]['vertices']:v[1]*=.65/.55
    # Rear saddle connects to the fixed hull; the muzzle clears the rotor below.
    MESHES['Laser_Pylon']=dict(
        vertices=[[x,y,z] for z in [-.28,-.125] for y in [.02,.10] for x in [-.06,.06]],
        faces=[[0,2,3,1],[4,5,7,6],[0,1,5,4],[2,6,7,3],[0,4,6,2],[1,3,7,5]],
        uvs=[[[0,0],[1,0],[1,1],[0,1]] for _ in range(6)],
        finishes=[1]*6,smooth=False)
    lathe('Shield_Base', [(0,0),(.138,0),(.175,.025),(.175,.082),
                          (.136,.13),(.08,.145),(0,.145)],segments=8,closed=False)
    lathe('Shield_Stem', [(0,.13),(.032,.13),(.032,.273),(.06,.273),
                          (.06,.307),(0,.307)],segments=8,finish=1,closed=False)
    lathe('Shield_Crystal', [(0,.298),(.043,.30),(.087,.36),(.098,.414),
                             (.083,.455),(.045,.482),(0,.488)],segments=10,
          finish=2,closed=False)
    # Broaden only the rotating annulus; the dome and bearing retain their size.
    for name in ['Outer_Ring']:
        m=MESHES[name]
        for v in m['vertices']:
            radius=math.hypot(v[0],v[2]);factor=widen_radius(radius)/radius
            v[0]*=factor;v[2]*=factor
    for name,m in MESHES.items():
        if name!='Outer_Ring':
            m['finishes']=[7 if f==0 else f for f in m['finishes']]
    # Planar paint on decks/dome/lenses avoids the previous compressed UV bands.
    planar=['Outer_Ring','Dome','Lens','Beam_Lens','Laser_Lens',
            'Antenna_Tip','Shield_Crystal']
    for name in planar:
        m=MESHES[name]
        radius=RING_RADIUS if name=='Outer_Ring' else max(math.hypot(v[0],v[2]) for v in m['vertices'])
        m['uvs']=[[[.5+m['vertices'][k][0]/(radius*2.04),
                     .5+m['vertices'][k][2]/(radius*2.04)] for k in face] for face in m['faces']]
    # Deck panels and wear are painted into the registered top-view UV map.


def assembly(upgraded=False):
    NODES.clear()
    root=node('UFO',extras={'units':'metres','up':'+Y','forward':'+Z','width':RING_RADIUS*2})
    body=node('Fixed_Hull',parent=root,extras={'role':'stationary'})
    for name in ['Hull','Dome_Collar','Dome','Bearing']:
        node(name,name,parent=body)
    ring=node('Outer_Ring_Pivot',parent=root,position=(0,.35,0),
              extras={'role':'rotor','axis':[0,1,0],'clearance_m':.015})
    node('Outer_Ring','Outer_Ring',parent=ring,position=(0,-.35,0))
    for i in range(6):
        a=i*TAU/6
        tilt=math.atan(math.tan(math.radians(23))*(1.1-.604)/(RING_RADIUS-.604))
        q=qmul(quat([0,1,0],-a),quat([0,0,1],-tilt))
        lens_radius=.604+(.851-.604)*(RING_RADIUS-.604)/(1.1-.604)
        socket=node(f'Socket_Rim_Lens_{i+1:02}',parent=ring,
            position=(lens_radius*math.cos(a),.533-.35,lens_radius*math.sin(a)),rotation=q,
            extras={'role':'socket','accepts':'rim-lens','normal':'+Y'})
        node(f'Rim_Bezel_{i+1:02}','Lens_Bezel',parent=socket)
        node(f'Rim_Lens_{i+1:02}','Lens',parent=socket)
    top=node('Socket_Top',parent=body,position=(0,1.012,0),
             extras={'role':'socket','accepts':'antenna|shield','normal':'+Y'})
    if upgraded:
        shield=node('Shield_Module',parent=top)
        for name in ['Shield_Base','Shield_Stem','Shield_Crystal']:
            node(name,name,parent=shield)
    else:
        antenna=node('Antenna_Module',parent=top,rotation=quat([0,0,1],-.43))
        node('Antenna_Stem','Antenna_Stem',parent=antenna)
        node('Antenna_Tip','Antenna_Tip',parent=antenna,position=(0,.335,0))
    beam=node('Socket_Beam',parent=body,position=(0,.04,0),
              extras={'role':'socket','accepts':'beam-emitter','normal':'-Y'})
    if upgraded: add_beam(beam)
    for side in (-1,1):
        # Fixed hull sockets are below the ring's lowest surface at this radius.
        socket=node('Socket_Laser_'+('Left' if side<0 else 'Right'),parent=body,
                    position=(side*.36,-.16,.29),
                    extras={'role':'socket','accepts':'laser','normal':'+Z'})
        if upgraded: add_laser(socket)
    return copy.deepcopy(NODES)


def add_beam(parent):
    module=node('Beam_Module',parent=parent)
    node('Beam_Housing','Beam_Housing',parent=module)
    for i in range(4):
        a=i*TAU/4
        node(f'Beam_Lens_{i+1:02}','Beam_Lens',parent=module,
             position=(.25*math.cos(a),-.114,.25*math.sin(a)),rotation=quat([1,0,0],math.pi))


def add_laser(parent):
    module=node('Laser_Module',parent=parent,rotation=quat([1,0,0],math.pi/2))
    for name in ['Laser_Base','Laser_Barrel','Laser_Lens','Laser_Pylon']: node(name,name,parent=module)


def mesh_arrays(mesh):
    pos, norm, uv, indices = [], [], [], {}
    vertex_normals=np.zeros((len(mesh['vertices']),3))
    if mesh['smooth']:
        for face in mesh['faces']:
            v=np.array([mesh['vertices'][k] for k in face])
            n=sum((np.cross(v[j]-v[0],v[j+1]-v[0]) for j in range(1,len(face)-1)),np.zeros(3))
            for k in face:vertex_normals[k]+=n
        vertex_normals/=np.linalg.norm(vertex_normals,axis=1)[:,None]
    for face, coords, finish in zip(mesh['faces'],mesh['uvs'],mesh['finishes']):
        v=np.array([mesh['vertices'][k] for k in face])
        n=np.cross(v[1]-v[0],v[2]-v[0]); n/=np.linalg.norm(n)
        start=len(pos)
        pos.extend(v.tolist()); norm.extend(vertex_normals[face].tolist() if mesh['smooth'] else [n.tolist()]*len(face))
        uv.extend(coords)
        for j in range(1,len(face)-1):indices.setdefault(finish,[]).extend([start,start+j,start+j+1])
    return [np.array(pos,dtype='<f4'),np.array(norm,dtype='<f4'),
            np.array(uv,dtype='<f4'),{key:np.array(value,dtype='<u4') for key,value in indices.items()}]


def export_glb(nodes, path, animate=False):
    doc={'asset':{'version':'2.0','generator':'Human Resources / modular UFO builder'},
         'scene':0,'scenes':[{'name':'UFO_Modular','nodes':[0]}],
         'nodes':copy.deepcopy(nodes),'meshes':[],'accessors':[],'bufferViews':[],
         'materials':[],'textures':[],'images':[],
         'samplers':[{'magFilter':9729,'minFilter':9987,'wrapS':33071,'wrapT':33071}]}
    blob=bytearray()
    def append(data,target=None):
        blob.extend(b'\0'*((-len(blob))%4))
        view={'buffer':0,'byteOffset':len(blob),'byteLength':len(data)}
        if target: view['target']=target
        doc['bufferViews'].append(view);blob.extend(data)
        return len(doc['bufferViews'])-1
    def accessor(data,typ,component,target=None):
        a={'bufferView':append(data.tobytes(),target),'componentType':component,
           'count':len(data),'type':typ}
        if typ=='VEC3' or typ=='SCALAR':
            a['min']=np.atleast_1d(data.min(axis=0)).tolist()
            a['max']=np.atleast_1d(data.max(axis=0)).tolist()
        doc['accessors'].append(a);return len(doc['accessors'])-1
    used=list(dict.fromkeys(n['mesh_name'] for n in nodes if 'mesh_name' in n))
    finishes=sorted({f for name in used for f in MESHES[name]['finishes']})
    images=list(dict.fromkeys(MATERIALS[f]['texture'] for f in finishes if 'texture' in MATERIALS[f]))
    for file in images:
        doc['images'].append({'bufferView':append(encoded_texture(file)),'mimeType':'image/jpeg','name':Path(file).stem})
        doc['textures'].append({'source':len(doc['images'])-1,'sampler':0})
    for finish in finishes:
        source=MATERIALS[finish]
        pbr={'baseColorFactor':source['color'],'metallicFactor':source['metallic'],
             'roughnessFactor':source['roughness']}
        if 'texture' in source:pbr['baseColorTexture']={'index':images.index(source['texture'])}
        material={'name':source['name'],'pbrMetallicRoughness':pbr}
        if 'emissive' in source:
            material['emissiveFactor']=source['emissive']
            material['emissiveTexture']={'index':images.index(source['texture'])}
        if 'clearcoat' in source:
            material['extensions']={'KHR_materials_clearcoat':{'clearcoatFactor':source['clearcoat'],'clearcoatRoughnessFactor':.3}}
            doc['extensionsUsed']=['KHR_materials_clearcoat']
        doc['materials'].append(material)
    for name in used:
        p,n,u,i=mesh_arrays(MESHES[name])
        attrs={'POSITION':accessor(p,'VEC3',5126,34962),
               'NORMAL':accessor(n,'VEC3',5126,34962),
               'TEXCOORD_0':accessor(u,'VEC2',5126,34962)}
        doc['meshes'].append({'name':name,'primitives':[{'attributes':attrs,
            'indices':accessor(ind,'SCALAR',5125,34963),'material':finishes.index(finish)} for finish,ind in i.items()]})
    for n in doc['nodes']:
        if 'mesh_name' in n:n['mesh']=used.index(n.pop('mesh_name'))
    if animate:
        time=np.linspace(0,8,65,dtype='<f4')
        turns=np.array([quat([0,1,0],TAU*t/8) for t in time],dtype='<f4')
        rotor=next(i for i,n in enumerate(nodes) if n['name']=='Outer_Ring_Pivot')
        doc['animations']=[{'name':'Outer_Ring_Spin','samplers':[{
            'input':accessor(time,'SCALAR',5126),'output':accessor(turns,'VEC4',5126),'interpolation':'LINEAR'}],
            'channels':[{'sampler':0,'target':{'node':rotor,'path':'rotation'}}]}]
    doc['buffers']=[{'byteLength':len(blob)}]
    blob.extend(b'\0'*((-len(blob))%4))
    data=json.dumps(doc,separators=(',',':')).encode();data+=b' '*((-len(data))%4)
    body=struct.pack('<II',len(data),0x4E4F534A)+data+struct.pack('<II',len(blob),0x004E4942)+blob
    path.write_bytes(struct.pack('<4sII',b'glTF',2,len(body)+12)+body)


def world_matrices(nodes):
    worlds={}
    def walk(i,parent):
        n=nodes[i];q=n.get('rotation',[0,0,0,1]);mat=np.eye(4)
        mat[:3,:3]=np.array([rotate(q,v) for v in np.eye(3)]).T
        mat[:3,3]=n.get('translation',[0,0,0]);worlds[i]=parent@mat
        for child in n.get('children',[]):walk(child,worlds[i])
    walk(0,np.eye(4))
    return worlds


def export_obj(nodes,path):
    lines=['# Editable assembled quad parts. Hierarchy/local pivots: ufo-source.json / GLB.',
           'mtllib ufo-parts.mtl','s off']
    vo=0;to=0
    worlds=world_matrices(nodes)
    for i,n in enumerate(nodes):
        if 'mesh_name' not in n:continue
        m=MESHES[n['mesh_name']]
        vertices=np.array(m['vertices'])
        vertices=(worlds[i]@np.c_[vertices,np.ones(len(vertices))].T).T[:,:3]
        lines.append('o '+n['name'])
        lines.extend('v '+' '.join(f'{x:.7f}' for x in v) for v in vertices)
        for f,uv,finish in zip(m['faces'],m['uvs'],m['finishes']):
            lines.append('usemtl '+MATERIALS[finish]['name'])
            for u,v in uv:lines.append(f'vt {u:.7f} {1-v:.7f}')
            lines.append('f '+' '.join(f'{k+1+vo}/{to+j+1}' for j,k in enumerate(f)))
            to+=len(f)
        vo+=len(m['vertices'])
    path.write_text('\n'.join(lines)+'\n')
    material_lines=[]
    for m in MATERIALS:
        material_lines+=['newmtl '+m['name'],'Kd '+' '.join(str(c) for c in m['color'][:3]),
                         'Pr '+str(m['roughness']),'Pm '+str(m['metallic'])]
        if 'texture' in m:material_lines.append('map_Kd materials/'+m['texture'])
        material_lines.append('')
    (OUT/'ufo-parts.mtl').write_text('\n'.join(material_lines))


def audit(nodes):
    report={}
    for name,m in MESHES.items():
        vertices=np.array(m['vertices']);edges=Counter(); directed=Counter();volume=0;degenerate=0
        for face in m['faces']:
            for a,b in zip(face,face[1:]+face[:1]):
                edges[tuple(sorted([a,b]))]+=1;directed[(a,b)]+=1
            v=vertices[face]
            for j in range(1,len(face)-1):
                area=np.linalg.norm(np.cross(v[j]-v[0],v[j+1]-v[0]))/2
                degenerate+=int(area<1e-12)
                volume+=float(v[0]@np.cross(v[j],v[j+1]))/6
        assert all(n==2 for n in edges.values()), f'{name}: open/nonmanifold edges'
        assert all(directed[(b,a)]==count for (a,b),count in directed.items()), f'{name}: inconsistent winding'
        assert degenerate==0, f'{name}: degenerate faces'
        assert volume>0, f'{name}: inverted winding'
        assert np.isfinite(vertices).all()
        report[name]={'vertices':len(vertices),'quads':sum(len(f)==4 for f in m['faces']),
            'triangles':sum(len(f)-2 for f in m['faces']),'boundary_edges':0,
            'nonmanifold_edges':0,'degenerate_triangles':0,'signed_volume':round(volume,9)}
    worlds=world_matrices(nodes);points=[];total=0
    for i,n in enumerate(nodes):
        if 'mesh_name' in n:
            m=MESHES[n['mesh_name']];v=np.array(m['vertices'])
            points.extend((worlds[i]@np.c_[v,np.ones(len(v))].T).T[:,:3])
            total+=report[n['mesh_name']]['triangles']
    pts=np.array(points)
    assert np.isclose(np.ptp(pts,axis=0)[0],RING_RADIUS*2,atol=.01)
    # Account for the flat faces of the 48-sided inner bore at arbitrary yaw.
    return {'status':'passed','mesh_topology':report,'base_triangles':total,
            'dimensions_m':np.ptp(pts,axis=0).tolist(),
            'bounds_m':[pts.min(axis=0).tolist(),pts.max(axis=0).tolist()],
            'ring_nominal_radial_clearance_m':.017,
            'ring_min_radial_clearance_m':.604*math.cos(math.pi/48)-.587,
            'coordinate_system':'+Y up, +Z forward, metres',
            'pivot':'Outer_Ring_Pivot at [0, 0.35, 0]; rotate about local Y',
            'note':'Closed welded source surfaces, outward winding, finite geometry, UVs; GLB splits vertices at flat normals/UV seams.'}


def check_swept_clearance(nodes):
    """Prove a full-turn envelope separation, including fitted upgrades.

    All rotating surfaces stay outside the inner bore and above its floor.
    Clip fixed triangles at that floor; convex radius reaches its maximum on
    the resulting polygon's vertices. Anything below the floor is clear too.
    """
    rotor=next(i for i,n in enumerate(nodes) if n['name']=='Outer_Ring_Pivot')
    rotating=set()
    def descendants(i):
        rotating.add(i)
        for child in nodes[i].get('children',[]):descendants(child)
    descendants(rotor)
    worlds=world_matrices(nodes);fixed_radius=0;minimum_bore=float('inf');floor=0
    for i,n in enumerate(nodes):
        if 'mesh_name' not in n:continue
        m=MESHES[n['mesh_name']];v=np.array(m['vertices'])
        v=(worlds[i]@np.c_[v,np.ones(len(v))].T).T[:,:3]
        for face in m['faces']:
            points=v[face]
            if i in rotating:
                assert points[:,1].min()>=floor-1e-9
                for a,b in zip(points,np.roll(points,-1,axis=0)):
                    a,b=a[[0,2]],b[[0,2]];d=b-a
                    t=np.clip(-a@d/(d@d),0,1) if d@d>1e-18 else 0
                    minimum_bore=min(minimum_bore,float(np.linalg.norm(a+t*d)))
            else:
                clipped=[]
                for a,b in zip(points,np.roll(points,-1,axis=0)):
                    if a[1]>=floor:clipped.append(a)
                    if (a[1]<floor)!=(b[1]<floor):
                        clipped.append(a+(b-a)*(floor-a[1])/(b[1]-a[1]))
                if clipped:fixed_radius=max(fixed_radius,float(np.linalg.norm(np.array(clipped)[:,[0,2]],axis=1).max()))
    gap=minimum_bore-fixed_radius
    assert gap>=.015, f'Insufficient full-turn clearance: {gap}'
    return {'minimum_bore_radius_m':minimum_bore,'fixed_radius_above_rotor_floor_m':fixed_radius,
            'minimum_clearance_m':gap,'full_turn_degrees':360,'status':'passed'}


def main():
    OUT.mkdir(parents=True,exist_ok=True);texture();encoded_texture.cache_clear();make_geometry()
    base=assembly(); upgraded=assembly(True)
    report=audit(base)
    report['base_swept_clearance']=check_swept_clearance(base)
    report['upgraded_swept_clearance']=check_swept_clearance(upgraded)
    export_glb(base,OUT/'ufo-modular.glb',animate=True)
    export_glb(upgraded,OUT/'ufo-modular-upgraded.glb',animate=True)
    # Export modules with a common explicit pivot, without assembly offsets.
    for part in ['beam','laser','shield','antenna','rim-lens','outer-ring']:
        NODES.clear();root=node(part.replace('-','_').title())
        if part=='beam':add_beam(root)
        elif part=='laser':add_laser(root)
        elif part=='shield':
            for n in ['Shield_Base','Shield_Stem','Shield_Crystal']:node(n,n,parent=root)
        elif part=='antenna':
            node('Antenna_Stem','Antenna_Stem',parent=root)
            node('Antenna_Tip','Antenna_Tip',parent=root,position=(0,.335,0))
        elif part=='rim-lens':
            node('Lens_Bezel','Lens_Bezel',parent=root);node('Lens','Lens',parent=root)
        elif part=='outer-ring':
            # Include lenses and strips, with the same central rotor pivot.
            ring=next(i for i,n in enumerate(base) if n['name']=='Outer_Ring_Pivot')
            NODES.clear()
            def clone(index,parent=None):
                n=base[index];new=node(n['name'],n.get('mesh_name'),parent,
                    n.get('translation',(0,0,0)),n.get('rotation'),n.get('extras'))
                for c in n.get('children',[]):clone(c,new)
            clone(ring);NODES[0]['translation']=[0,0,0]
        export_glb(NODES,OUT/('part-'+part+'.glb'))
    source={'units':'metres','up':'+Y','forward':'+Z','meshes':MESHES,'materials':MATERIALS,
            'base_nodes':base,'upgraded_nodes':upgraded}
    (OUT/'ufo-source.json').write_text(json.dumps(source,separators=(',',':'))+'\n')
    (OUT/'model-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    export_obj(base,OUT/'ufo-parts.obj')
    export_obj(upgraded,OUT/'ufo-upgraded-parts.obj')
    print(json.dumps({k:v for k,v in report.items() if k!='mesh_topology'},indent=2))


if __name__=='__main__':main()
