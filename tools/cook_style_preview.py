"""Cook the development-only visual study. Existing NumPy/Pillow authoring tools only.

HRM1: little-endian magic, vertex/index counts, RGBA width/height; 14-float
vertices (position, normal, UV, base factor, emission, rotor membership), u32
indices, then tightly packed RGBA8. No source GLB parsing occurs at runtime.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from normalize_glb import unpack, accessor, matrix

ROOT = Path(__file__).resolve().parents[1]
ASSETS = [
    'ufo-modular', 'cow', 'sheep', 'pig', 'farmer-body', 'straw-hat',
    'resident-body', 'nightcap', 'soldier-body', 'jeep-chassis', 'jeep-wheel',
    'tank-hull', 'tank-turret', 'missile', 'tree-round-large', 'tree-round-small',
    'bush-low', 'hedge-module', 'field-rock-a', 'field-rock-b', 'grass-tuft',
    'crop-seedling', 'barn', 'silo', 'hay-bale', 'windmill-mast', 'windmill-rotor',
    'house-coral', 'house-teal', 'streetlamp', 'mailbox-red', 'mailbox-teal',
    'wheelie-bin', 'cactus-tall', 'cactus-short', 'cliff-module-a', 'cliff-module-b',
    'desert-boulder-a', 'desert-boulder-b', 'desert-scrub', 'service-canopy',
    'fuel-pump-red', 'fuel-pump-blue', 'road-bollard']


def write_mesh(path, vertices, indices, image):
    vertices = np.asarray(vertices, dtype='<f4').reshape(-1, 14)
    indices = np.asarray(indices, dtype='<u4').reshape(-1)
    if not np.isfinite(vertices).all() or not len(indices) or len(indices) % 3:
        raise ValueError(f'Invalid geometry: {path}')
    if int(indices.max()) >= len(vertices):
        raise ValueError('Index out of range')
    image = image.convert('RGBA')
    path.write_bytes(struct.pack('<4sIIII', b'HRM1', len(vertices), len(indices), *image.size)
                     + vertices.tobytes() + indices.tobytes() + image.tobytes())


def cook_glb(asset, output, inventory, *, source_path=None, exclude_nodes=(), opaque_material=False):
    source = ROOT / 'assets/models' / asset / (asset + '.glb')
    pending = False
    if asset == 'ufo-modular' and source_path is None:
        source = source.with_name('ufo-modular-upgraded.glb')
    if source_path is not None:
        source = ROOT / source_path
    if not source.exists():
        candidates = sorted((ROOT / 'assets/models' / asset / 'tripo-out').glob('*/model.glb'))
        if len(candidates) != 1:
            raise ValueError(f'Need an unambiguous source for {asset}')
        source = candidates[0]
        pending = True
    doc, chunks = unpack(source)
    blob = next(b for t, b in chunks if t == 0x004e4942)
    # One bounded atlas per model. Keep gutters; remap each material's UVs.
    mats = doc.get('materials', [{}])
    cells = math.ceil(math.sqrt(len(mats)))
    cell = 512 if cells == 1 else 256
    gutter = 4
    atlas = Image.new('RGBA', (cells * cell, cells * cell), 'white')
    for mi, mat in enumerate(mats):
        pbr = mat.get('pbrMetallicRoughness', {})
        texture = pbr.get('baseColorTexture')
        image = Image.new('RGBA', (cell - 2*gutter, cell - 2*gutter), 'white')
        if texture:
            if texture.get('texCoord', 0) != 0 or texture.get('extensions'):
                raise ValueError('Unsupported texture coordinates/transform')
            im = doc['images'][doc['textures'][texture['index']]['source']]
            view = doc['bufferViews'][im['bufferView']]
            start = view.get('byteOffset', 0)
            image = Image.open(io.BytesIO(blob[start:start+view['byteLength']])).convert('RGBA')
            image = image.resize((cell-2*gutter, cell-2*gutter), Image.Resampling.LANCZOS)
        # Extend edge pixels across the gutter rather than bleeding adjacent materials.
        tile = Image.fromarray(np.pad(np.asarray(image), ((gutter,gutter),(gutter,gutter),(0,0)), mode='edge'))
        atlas.paste(tile, ((mi % cells)*cell, (mi//cells)*cell))
    verts, inds = [], []
    def visit(index, parent, rotor, ancestors):
        if index in ancestors:
            raise ValueError('Cyclic GLB hierarchy')
        node = doc['nodes'][index]
        if node.get('name') in exclude_nodes:
            return
        transform = parent @ matrix(node)
        rotor = rotor or node.get('name') == 'Outer_Ring_Pivot'
        if 'mesh' in node:
            for primitive in doc['meshes'][node['mesh']]['primitives']:
                if primitive.get('mode',4) != 4 or 'targets' in primitive:
                    raise ValueError('Only static triangle geometry is supported')
                a = primitive['attributes']
                pos = accessor(doc,blob,a['POSITION'])
                normals = accessor(doc,blob,a['NORMAL'])
                if 'TEXCOORD_0' in a:
                    uv = accessor(doc,blob,a['TEXCOORD_0'])
                elif mats[primitive.get('material',0)].get('pbrMetallicRoughness',{}).get('baseColorTexture'):
                    raise ValueError('Textured GLB requires UV coordinates')
                else:
                    uv = np.zeros((len(pos),2))
                if uv.min() < -0.001 or uv.max() > 1.001:
                    raise ValueError(f'Atlas requires in-range UVs: {asset}')
                pos = (transform @ np.column_stack([pos,np.ones(len(pos))]).T).T[:,:3]
                normals = normals @ np.linalg.inv(transform[:3,:3])
                normals /= np.maximum(np.linalg.norm(normals,axis=1,keepdims=True),1e-10)
                mi = primitive.get('material',0)
                mat = mats[mi]
                uv = (np.clip(uv,0,1)*(cell-2*gutter)+gutter+np.array([mi%cells,mi//cells])*cell)/(cells*cell)
                factor = [1,1,1,1] if opaque_material else mat.get('pbrMetallicRoughness',{}).get('baseColorFactor',[1,1,1,1])
                emissive = max(mat.get('emissiveFactor',[0,0,0]))
                data = np.column_stack([pos,normals,uv,np.tile(factor,(len(pos),1)),
                                        np.full(len(pos),emissive),np.full(len(pos),float(rotor))])
                indices = accessor(doc,blob,primitive['indices']).reshape(-1) if 'indices' in primitive else np.arange(len(pos))
                if np.linalg.det(transform[:3,:3]) < 0:
                    indices = indices.reshape(-1,3)[:,[0,2,1]].reshape(-1)
                inds.extend((indices+sum(len(v) for v in verts)).tolist())
                verts.append(data)
        for child in node.get('children',[]):
            visit(child,transform,rotor,ancestors | {index})
    for root in doc['scenes'][doc.get('scene',0)]['nodes']:
        visit(root,np.eye(4),False,set())
    vertices = np.concatenate(verts)
    if pending:
        if asset in ['windmill-rotor','service-canopy']:
            rotation=np.array([[0,0,1],[0,1,0],[-1,0,0]])
            vertices[:,:3]=vertices[:,:3]@rotation.T
            vertices[:,3:6]=vertices[:,3:6]@rotation.T
        dims = inventory[asset]['dimensions_m']
        axis = int(np.argmax(dims))
        extent = np.ptp(vertices[:,:3],axis=0)
        scale = dims[axis] / extent[axis]
        vertices[:,:3] *= scale
        lo,hi = vertices[:,:3].min(axis=0), vertices[:,:3].max(axis=0)
        vertices[:,:3] -= [(lo[0]+hi[0])/2,(lo[1]+hi[1])/2 if asset=='windmill-rotor' else lo[1],(lo[2]+hi[2])/2]
    write_mesh(output/(asset+'.hrm'),vertices,inds,atlas)
    return {'id':asset,'source':str(source.relative_to(ROOT)), 'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'triangles':len(inds)//3,'preview_normalized':pending,
            'excluded_nodes':list(exclude_nodes),'opaque_material':opaque_material,
            'review':inventory.get(asset,{}).get('qa',{}).get('status','modular assembly'),
            'bounds':[vertices[:,:3].min(axis=0).tolist(),vertices[:,:3].max(axis=0).tolist()]}


def primitives(output):
    white=Image.new('RGBA',(4,4),'white')
    faces=[([-1,0,-1],[-1,0,1],[1,0,1],[1,0,-1],[0,1,0])]
    def mesh(name,faces,image):
        v=[]; ind=[]
        for a,b,c,d,n in faces:
            start=len(v)
            for pos,uv in zip([a,b,c,d],[[0,0],[0,1],[1,1],[1,0]]):
                v.append([*pos,*n,*uv,1,1,1,1,0,0])
            ind += [start,start+1,start+2,start,start+2,start+3]
        write_mesh(output/(name+'.hrm'),v,ind,image)
    mesh('plane',faces,white)
    cube=[]
    for axis in range(3):
        for sign in [-1,1]:
            n=[0,0,0]; n[axis]=sign
            points=[]
            for u,v in [(-1,-1),(-1,1),(1,1),(1,-1)]:
                p=[0,0,0];p[axis]=sign;p[(axis+1)%3]=u;p[(axis+2)%3]=v;points.append(p)
            if sign<0:points.reverse()
            cube.append([*points,n])
    mesh('cube',cube,white)
    for name in ['ring','beam']:
        v=[];ind=[]
        for i in range(96):
            a=i*2*math.pi/96;b=(i+1)*2*math.pi/96
            if name=='ring':
                points=[(math.cos(a)*r,0,math.sin(a)*r) for r in [.96,1.04]]+[(math.cos(b)*r,0,math.sin(b)*r) for r in [1.04,.96]]
            else:
                points=[(math.cos(a),0,math.sin(a)),(math.cos(a)*.12,1,math.sin(a)*.12),(math.cos(b)*.12,1,math.sin(b)*.12),(math.cos(b),0,math.sin(b))]
            start=len(v)
            for point in points:v.append([*point,0,1,0,0,0,1,1,1,1,1,0])
            ind += [start,start+1,start+2,start,start+2,start+3]
        write_mesh(output/(name+'.hrm'),v,ind,white)
    for name in ['grass','dirt','soil','asphalt','pavement','sand','wood']:
        source=ROOT/'assets/textures'/('texture-'+name)/('texture-'+name+'.png')
        image=Image.open(source).convert('RGB')
        # Consume the canonical seamless export, never the uncorrected draft.
        pixels=np.asarray(image)
        if image.size!=(1024,1024) or not (np.array_equal(pixels[0],pixels[-1]) and np.array_equal(pixels[:,0],pixels[:,-1])):
            raise ValueError(f'Ground texture must be a reviewed seamless 1024px export: {source}')
        mesh('ground-'+name,faces,image)


def hud(output):
    fontroot=ROOT/'assets/fonts/human-resources-v3/ttf'
    fonts=list(fontroot.glob('*.ttf'))
    ui=next(p for p in fonts if 'Beam' in p.name and 'SemiBold' in p.name)
    small=ImageFont.truetype(str(ui),17); large=ImageFont.truetype(str(ui),28)
    for i,(title,sub) in enumerate([('SAGE FARMLAND','OPEN PADDOCK / CROPS / FARM EDGE'),('SLATE SUBURBIA','BROAD STREET / GARDEN EDGES'),('MAUVE DESERT','SAND CLEARING / ROADSIDE / ROCK WALL')]):
        image=Image.new('RGBA',(720,1280));d=ImageDraw.Draw(image)
        navy=(12,36,58,242);mint=(98,244,202,255)
        for box in [(22,20,232,66),(257,20,515,66),(540,20,698,66)]:d.rounded_rectangle(box,23,fill=navy)
        d.ellipse((34,31,58,55),fill='#ffce51');d.text((69,25),'12,480',font=large,fill='white')
        d.text((271,27),'LV 07',font=large,fill='white');d.rounded_rectangle((366,34,500,52),9,fill='#334f60');d.rounded_rectangle((366,34,439,52),9,fill=mint)
        for x in [562,606,650]:d.polygon([(x,32),(x+11,29),(x+21,32),(x+20,46),(x+11,56),(x+2,46)],fill=mint)
        d.rounded_rectangle((22,1182,698,1255),12,fill=(12,36,58,222))
        d.text((40,1192),title,font=large,fill='white');d.text((40,1228),sub+'   /   NATIVE STYLE STUDY 01',font=small,fill='#b7cec7')
        # Store RGBA alongside a plane for the renderer's screen pass.
        vertex=[[0,0,0,0,1,0,0,0,1,1,1,1,0,0]]
        write_mesh(output/f'hud-{i}.hrm',vertex,[0,0,0],image)
        landscape=Image.new('RGBA',(1280,720))
        landscape.paste(image.crop((0,0,720,86)),(280,0))
        landscape.paste(image.crop((0,1180,720,1280)),(280,620))
        write_mesh(output/f'hud-land-{i}.hrm',vertex,[0,0,0],landscape)


def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    inventory={a['id']:a for a in json.loads((ROOT/'assets/production/inventory.json').read_text())['assets']}
    report=[cook_glb(a,args.output,inventory) for a in ASSETS]
    primitives(args.output);hud(args.output)
    materials=[]
    for name in ['grass','dirt','soil','asphalt','pavement','sand','wood']:
        source=ROOT/'assets/textures'/('texture-'+name)/('texture-'+name+'.png')
        materials.append({'id':name,'source':str(source.relative_to(ROOT)),
            'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'audit':f'assets/production/records/texture-{name}/texture-audit.json'})
    (args.output/'source-manifest.json').write_text(json.dumps({'schema':1,'purpose':'development-only style preview; pending models are not production approved','models':report,'materials':materials},indent=2)+'\n')
    print(f'Cooked {len(report)} textured models plus terrain and HUD into {args.output}')
if __name__=='__main__':main()
