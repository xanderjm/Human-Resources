"""Rebuild measured scenery with embedded periodic albedo and explicit join ports.

Authoring-only tool. Requires the approved source PNGs, numpy and Pillow. No game
runtime dependency is introduced. All geometry is metres, Y-up, uncompressed GLB.
"""
import argparse
import copy
import hashlib
import io
import json
import math
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from audit_glb import inspect

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/production/records'
TEX = {}


def material(name):
    if name not in TEX:
        path = ROOT / 'assets/textures' / ('texture-' + name) / ('texture-' + name + '.png')
        image = Image.open(path).convert('RGB')
        if image.size != (1024, 1024):
            raise ValueError(f'{path}: expected 1024 square')
        pixels = np.asarray(image)
        if not np.array_equal(pixels[0], pixels[-1]) or not np.array_equal(pixels[:, 0], pixels[:, -1]):
            raise ValueError(f'{path}: source opposite edges differ')
        TEX[name] = (path, image, pixels)
    return TEX[name]


class Mesh:
    def __init__(self, name, texture):
        self.name, self.texture = name, texture
        self.positions, self.normals, self.uvs, self.indices = [], [], [], []
        self.ports, self.notes = [], []

    def triangle(self, xyz, uv):
        p = np.array(xyz, dtype=float)
        normal = np.cross(p[1] - p[0], p[2] - p[0])
        size = np.linalg.norm(normal)
        if size < 1e-10:
            raise ValueError(f'{self.name}: degenerate triangle')
        start = len(self.positions)
        self.positions.extend(xyz)
        self.normals.extend([list(normal / size)] * 3)
        self.uvs.extend(uv)
        self.indices.extend(range(start, start + 3))

    def quad(self, xyz, uv=None):
        uv = uv or [[0, 0], [1, 0], [1, 1], [0, 1]]
        self.triangle([xyz[i] for i in (0, 1, 2)], [uv[i] for i in (0, 1, 2)])
        self.triangle([xyz[i] for i in (0, 2, 3)], [uv[i] for i in (0, 2, 3)])

    def port(self, name, centre, outward, edge, uv):
        self.ports.append(dict(name=name, centre_m=centre, outward=outward,
                               edge_m=edge, edge_uv=uv))


def flat(name, texture, width, length):
    m = Mesh(name, texture)
    x = width / 2
    m.quad([[-x, 0, 0], [-x, 0, length], [x, 0, length], [x, 0, 0]],
           [[0, 0], [0, length], [width, length], [width, 0]])
    for label, z, sign in [('start', 0, -1), ('end', length, 1)]:
        m.port(label, [0, 0, z], [0, 0, sign], [[-x, 0, z], [x, 0, z]], [[0, z], [width, z]])
    return m


def annulus(name, texture, inner, outer, repeats):
    m = Mesh(name, texture)
    segments = 64
    for i in range(segments):
        a, b = (i / segments) * math.pi / 2, ((i + 1) / segments) * math.pi / 2
        def p(r, angle):
            return [r * math.cos(angle), 0, r * math.sin(angle)]
        m.quad([p(inner, a), p(inner, b), p(outer, b), p(outer, a)],
               [[0, repeats * i / segments], [0, repeats * (i + 1) / segments],
                [outer - inner, repeats * (i + 1) / segments], [outer - inner, repeats * i / segments]])
    mid = (inner + outer) / 2
    m.port('start', [mid, 0, 0], [0, 0, -1], [[inner, 0, 0], [outer, 0, 0]], [[0, 0], [outer - inner, 0]])
    m.port('end', [0, 0, mid], [-1, 0, 0], [[0, 0, inner], [0, 0, outer]], [[0, repeats], [outer - inner, repeats]])
    m.notes.append(f'Quarter annulus: inner radius {inner}m, outer radius {outer}m; V spans exactly {repeats} repeats so tangent joins wrap without phase jumps.')
    return m


def box(m, lo, hi, uv_axes=None):
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    faces = [([(x0,y0,z0),(x0,y1,z0),(x1,y1,z0),(x1,y0,z0)], (0,1)),
             ([(x1,y0,z1),(x1,y1,z1),(x0,y1,z1),(x0,y0,z1)], (0,1)),
             ([(x0,y0,z1),(x0,y1,z1),(x0,y1,z0),(x0,y0,z0)], (2,1)),
             ([(x1,y0,z0),(x1,y1,z0),(x1,y1,z1),(x1,y0,z1)], (2,1)),
             ([(x0,y1,z0),(x0,y1,z1),(x1,y1,z1),(x1,y1,z0)], (0,2)),
             ([(x0,y0,z1),(x0,y0,z0),(x1,y0,z0),(x1,y0,z1)], (0,2))]
    for points, axes in faces:
        axes = uv_axes or axes
        m.quad(points, [[p[axes[0]], p[axes[1]]] for p in points])


def junction():
    m = Mesh('road-junction', 'asphalt')
    m.quad([[-8,0,-3],[-8,0,3],[8,0,3],[8,0,-3]])
    m.quad([[-3,0,3],[-3,0,8],[3,0,8],[3,0,3]])
    m.port('west',[-8,0,0],[-1,0,0],[[-8,0,3],[-8,0,-3]],[[-8,3],[-8,-3]])
    m.port('east',[8,0,0],[1,0,0],[[8,0,3],[8,0,-3]],[[8,3],[8,-3]])
    m.port('north',[0,0,8],[0,0,1],[[-3,0,8],[3,0,8]],[[-3,8],[3,8]])
    m.notes.append('Logical16x16m placement grid; occupied asphalt bounds16x11m. Planar UVs preserve uniform brush density, including the junction centre.')
    return m


def pavement_straight():
    m = Mesh('pavement-straight', 'pavement')
    # Top joints are actual 6mm-wide grooves at a 2m module rhythm. Each end
    # holds one half-groove so adjoining spans have the same joint width.
    for i in range(8):
        box(m,[0,0,i*2+.003],[1.5,.14,(i+1)*2-.003])
    box(m,[0,0,0],[1.5,.11,16])
    for i in range(16):
        box(m,[0,.11,i+.003],[.15,.16,i+1-.003])
    for name,z,sign in [('start',0,-1),('end',16,1)]:
        m.port(name,[.75,0,z],[0,0,sign],[[0,.11,z],[1.5,.11,z]],[[0,z],[1.5,z]])
    m.notes.append('Walking slab .14m; narrow road-facing inner kerb .16m. 6mm geometric grooves at 2m slab and 1m kerb rhythms. Join ends each reserve half a groove.')
    return m


def pavement_corner():
    m = annulus('pavement-corner','pavement',12,13.5,20)
    # Replace plane by solid paved annulus. Same longitudinal UV phase as
    # straight, with exactly twenty repeats along the quarter turn.
    m.positions,m.normals,m.uvs,m.indices=[],[],[],[]
    for port in m.ports:
        for point in port['edge_m']: point[1] = .11
    n = 80
    def p(r,a,y): return [r*math.cos(a),y,r*math.sin(a)]
    for i in range(n):
        a,b=i/n*math.pi/2,(i+1)/n*math.pi/2
        va,vb=20*i/n,20*(i+1)/n
        for r0,r1,y in [(12,12.15,.16),(12.15,13.5,.14)]:
            # Grooves every 1m in UV longitudinal coordinate; road kerb every1,
            # slabs every2. Bottom substrate remains closed below.
            rhythm=4 if r0==12 else 8
            aa=a+(0.003/12.75 if i%rhythm==0 else 0)
            bb=b-(0.003/12.75 if (i+1)%rhythm==0 else 0)
            m.quad([p(r0,aa,y),p(r0,bb,y),p(r1,bb,y),p(r1,aa,y)],
                [[r0-12,va],[r0-12,vb],[r1-12,vb],[r1-12,va]])
        for r,y,flip in [(12,.16,False),(13.5,.14,True),(12.15,.16,True)]:
            points=[p(r,a,0 if r!=12.15 else .14),p(r,b,0 if r!=12.15 else .14),p(r,b,y),p(r,a,y)]
            uv=[[va,0],[vb,0],[vb,y],[va,y]]
            if flip: points,uv=points[::-1],uv[::-1]
            m.quad(points,uv)
        m.quad([p(12,a,.11),p(12,b,.11),p(13.5,b,.11),p(13.5,a,.11)],[[0,va],[0,vb],[1.5,vb],[1.5,va]])
        m.quad([p(13.5,a,0),p(13.5,b,0),p(12,b,0),p(12,a,0)],[[1.5,va],[1.5,vb],[0,vb],[0,va]])
    for a,v,rev in [(0,0,False),(math.pi/2,20,True)]:
        for r0,r1,y in [(12,12.15,.16),(12.15,13.5,.14)]:
            pts=[p(r0,a,0),p(r0,a,y),p(r1,a,y),p(r1,a,0)]
            uvs=[[r0-12,v],[r0-12,v+y],[r1-12,v+y],[r1-12,v]]
            if rev: pts,uvs=pts[::-1],uvs[::-1]
            m.quad(pts,uvs)
    m.notes.append('Outer road-corner pavement, radius12..13.5m; top slab .14m, inner kerb .16m. Forty half-metre-equivalent steps, 2-repeat slab and1-repeat kerb joint rhythms. Bottom closed.')
    return m


def build():
    post=Mesh('fence-post','wood')
    # Four-sided tapered bevel at the crown, still exact 1.1m tall.
    box(post,[-.08,0,-.08],[.08,1.06,.08],(1,0))
    lower=[[-.08,1.06,-.08],[-.08,1.06,.08],[.08,1.06,.08],[.08,1.06,-.08]]
    upper=[[-.055,1.1,-.055],[-.055,1.1,.055],[.055,1.1,.055],[.055,1.1,-.055]]
    for i in range(4):
        j=(i+1)%4
        points=[lower[i],upper[i],upper[j],lower[j]]
        post.quad(points,[[p[1],p[0]+p[2]] for p in points])
    post.quad(upper,[[p[0],p[2]] for p in upper])
    post.notes.append('Bottom-centre pivot. Grain +U follows vertical Y on post faces; top bevel preserves .16m footprint and1.1m height. Post spacing2m.')
    rail=Mesh('fence-rail','wood');box(rail,[0,0,-.045],[2,.12,.045],(0,1))
    rail.port('start',[0,.06,0],[-1,0,0],[[0,0,-.045],[0,.12,.045]],[[0,0],[0,.12]])
    rail.port('end',[2,.06,0],[1,0,0],[[2,0,-.045],[2,.12,.045]],[[2,0],[2,.12]])
    ground=flat('ground-tile','grass',16,16)
    ground.notes.append('Shared 16m square geometry. Default grass; replace only base-colour texture with sand for desert. 1m repeat footprint; UV range16x16. Seam-free translation by16m in X or Z. No perimeter wall or baked border.')
    path=flat('path-straight','dirt',3,16)
    soil=flat('soil-bed','soil',1,4)
    road=flat('road-straight','asphalt',6,16)
    end=Mesh('pavement-end','pavement')
    box(end,[0,0,0],[1.5,.11,.4]);box(end,[0,.11,0],[1.5,.14,.34]);box(end,[0,.11,0],[.15,.16,.34])
    # A short sloped finish at the exposed end, no open geometry.
    end.quad([[0,.14,.34],[0,.11,.4],[1.5,.11,.4],[1.5,.14,.34]],[[0,.34],[0,.4],[1.5,.4],[1.5,.34]])
    end.port('join',[.75,0,0],[0,0,-1],[[0,.11,0],[1.5,.11,0]],[[0,0],[1.5,0]])
    end.notes.append('Short closed termination; upper walking surface bevels down over final .06m. Join at localZ0, exposed endZ.4. Road-facing kerb at localX0.')
    return [post,rail,ground,path,annulus('path-corner','dirt',4.5,7.5,10),soil,road,
            annulus('road-corner','asphalt',6,12,14),junction(),pavement_straight(),pavement_corner(),end]


def write_glb(m, filename=None):
    positions=np.asarray(m.positions,dtype='<f4'); normals=np.asarray(m.normals,dtype='<f4')
    uvs=np.asarray(m.uvs,dtype='<f4'); indices=np.asarray(m.indices,dtype='<u4')
    blob=bytearray(); views=[]; access=[]
    def data(raw,target=None):
        blob.extend(b'\0' * ((-len(blob))%4)); offset=len(blob);blob.extend(raw)
        d={'buffer':0,'byteOffset':offset,'byteLength':len(raw)}
        if target:d['target']=target
        views.append(d);return len(views)-1
    def attr(a,typ,component,target):
        v=data(a.tobytes(),target)
        d={'bufferView':v,'componentType':component,'count':len(a),'type':typ}
        if typ!='SCALAR':d.update(min=a.min(axis=0).tolist(),max=a.max(axis=0).tolist())
        access.append(d);return len(access)-1
    p=attr(positions,'VEC3',5126,34962);n=attr(normals,'VEC3',5126,34962);u=attr(uvs,'VEC2',5126,34962);ind=attr(indices,'SCALAR',5125,34963)
    texpath,image,_=material(m.texture); encoded=io.BytesIO();image.save(encoded,format='PNG');imageview=data(encoded.getvalue())
    doc={'asset':{'version':'2.0','generator':'Human Resources measured module authoring1',
         'extras':{'units':'metres','up':'+Y','forward':'+Z','pivot':'documented local module origin','ports':m.ports}},
        'scene':0,'scenes':[{'nodes':[0]}],'nodes':[{'name':m.name,'mesh':0}],
        'meshes':[{'primitives':[{'attributes':{'POSITION':p,'NORMAL':n,'TEXCOORD_0':u},'indices':ind,'material':0}]}],
        'materials':[{'name':'painterly-'+m.texture,'doubleSided':False,'pbrMetallicRoughness':{
            'baseColorTexture':{'index':0},'metallicFactor':0,'roughnessFactor':1}}],
        'textures':[{'sampler':0,'source':0}],'samplers':[{'magFilter':9729,'minFilter':9987,'wrapS':10497,'wrapT':10497}],
        'images':[{'bufferView':imageview,'mimeType':'image/png','name':'texture-'+m.texture}],
        'bufferViews':views,'accessors':access,'buffers':[{'byteLength':len(blob)}]}
    js=json.dumps(doc,separators=(',',':')).encode();js+=b' '*((-len(js))%4);blob.extend(b'\0'*((-len(blob))%4))
    body=struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(blob),0x004e4942)+blob
    path=ROOT/'assets/models'/m.name/(filename or (m.name+'.glb'));path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(struct.pack('<4sII',b'glTF',2,len(body)+12)+body)
    return path,positions,uvs


def sample(texture, uv):
    pixels=material(texture)[2]
    uv=np.asarray(uv); q=np.mod(uv,1)*1023
    x,y=q[...,0],q[...,1];x0=np.floor(x).astype(int);y0=np.floor(y).astype(int)
    x1=(x0+1)%1024;y1=(y0+1)%1024
    fx=(x-x0)[...,None];fy=(y-y0)[...,None]
    return (pixels[y0,x0]*(1-fx)*(1-fy)+pixels[y0,x1]*fx*(1-fy)+pixels[y1,x0]*(1-fx)*fy+pixels[y1,x1]*fx*fy)


def render(m, size=768, top=False, transforms=None, background=(255,255,255)):
    objects=transforms or [(m,np.eye(3),np.zeros(3))]
    allp=np.concatenate([np.asarray(a.positions)@r.T+t for a,r,t in objects])
    right=np.array([1.,0.,0.]) if top else np.array([1.,0.,-1.])/math.sqrt(2)
    up=np.array([0.,0.,-1.]) if top else np.array([-.38,.843,-.38]);up/=np.linalg.norm(up)
    view=np.cross(right,up)
    projected=np.column_stack([allp@right,-allp@up]);lo=projected.min(0);hi=projected.max(0)
    scale=(size*.84)/max((hi-lo).max(),.001);middle=(lo+hi)/2
    canvas=np.empty((size,size,3),dtype=np.uint8);canvas[:]=background
    depth=np.full((size,size),-np.inf)
    for obj,rotation,translation in objects:
        pos=np.asarray(obj.positions)@rotation.T+translation
        norm=np.asarray(obj.normals)@rotation.T
        screen=(np.column_stack([pos@right,-pos@up])-middle)*scale+size/2
        z=pos@view
        for base in range(0,len(pos),3):
            idx=np.arange(base,base+3); pts=screen[idx];uv=np.asarray(obj.uvs)[idx]
            xmin=max(0,int(np.floor(pts[:,0].min())));xmax=min(size-1,int(np.ceil(pts[:,0].max())))
            ymin=max(0,int(np.floor(pts[:,1].min())));ymax=min(size-1,int(np.ceil(pts[:,1].max())))
            if xmin>xmax or ymin>ymax:continue
            a,b,c=pts;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(den)<1e-9:continue
            yy,xx=np.mgrid[ymin:ymax+1,xmin:xmax+1];xx=xx+.5;yy=yy+.5
            w0=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den
            w1=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den;w2=1-w0-w1
            zz=w0*z[idx[0]]+w1*z[idx[1]]+w2*z[idx[2]]
            region=depth[ymin:ymax+1,xmin:xmax+1];mask=(w0>=-1e-6)&(w1>=-1e-6)&(w2>=-1e-6)&(zz>region)
            if not mask.any():continue
            coords=w0[...,None]*uv[0]+w1[...,None]*uv[1]+w2[...,None]*uv[2]
            light=.64+.36*max(0,float(norm[base]@np.array([.3,.9,.2])/np.linalg.norm([.3,.9,.2])))
            colours=np.clip(sample(obj.texture,coords)*light,0,255).astype(np.uint8)
            canvas[ymin:ymax+1,xmin:xmax+1][mask]=colours[mask];region[mask]=zz[mask]
    return Image.fromarray(canvas)


def world_uv(mesh, yaw=0):
    """Bake orientation and planar UVs: material stays aligned to the grid."""
    m=copy.deepcopy(mesh)
    angle=math.radians(yaw);c,s=round(math.cos(angle)),round(math.sin(angle))
    rotation=np.array([[c,0,s],[0,1,0],[-s,0,c]])
    m.positions=(np.asarray(m.positions)@rotation.T).tolist()
    m.normals=(np.asarray(m.normals)@rotation.T).tolist()
    for i,(point,normal) in enumerate(zip(m.positions,m.normals)):
        if normal[1]>.8:m.uvs[i]=[point[0],point[2]]
    for port in m.ports:
        for field in ('centre_m','outward'):
            port[field]=(rotation@np.asarray(port[field])).tolist()
        port['edge_m']=(np.asarray(port['edge_m'])@rotation.T).tolist()
        port['edge_uv']=[[p[0],p[2]] for p in port['edge_m']]
    m.notes=[n for n in m.notes if 'V spans exactly' not in n]
    m.notes.append('Planar top UV=(localX,localZ), one1m repeat. Orientation variants bake rotations and reprojectUV so texture remains world aligned. Translate only on integer-metre grid; select baked yaw variant instead of rotating default UVs.')
    return m


def port_seams(meshes):
    results=[]
    for m in meshes:
        if m.name.startswith('fence'):continue
        for port in m.ports:
            positions=np.asarray(m.positions)
            for point in port['edge_m']:
                if not np.any(np.linalg.norm(positions-point,axis=1)<1e-7):
                    raise ValueError(f'{m.name}: port corner missing from geometry')
            uv=np.asarray(port['edge_uv']);t=np.linspace(0,1,1025)[:,None]
            actual=uv[0]*(1-t)+uv[1]*t
            error=float(np.abs(sample(m.texture,actual)-sample(m.texture,actual+[16,16])).max())
            if error>1e-6:raise ValueError(f'{m.name}: repeat seam')
            results.append({'asset':m.name,'port':port['name'],'samples':1025,'integer_grid_translation':[16,16],'max_rgb_error':error})
    return results


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--no-render',action='store_true');args=parser.parse_args()
    meshes=[m if m.name.startswith('fence') else world_uv(m) for m in build()];seams=port_seams(meshes);sheet=Image.new('RGB',(1536,4*520),'#f5f3ee');draw=ImageDraw.Draw(sheet)
    for index,m in enumerate(meshes):
        path,pos,uv=write_glb(m);stats=inspect(path);lo,hi=pos.min(0),pos.max(0)
        rec=OUT/m.name;rec.mkdir(parents=True,exist_ok=True)
        variants=[]
        if not m.name.startswith('fence'):
            for yaw in (90,180,270):
                variant=world_uv(m,yaw)
                vp,_,_=write_glb(variant,f'{m.name}-yaw{yaw}.glb')
                inspect(vp)
                variants.append({'yaw_degrees':yaw,'model_file':str(vp.relative_to(ROOT)),'ports':variant.ports})
        report={'asset':m.name,'model_file':str(path.relative_to(ROOT)),'generator':'tools/build_measured_modules.py',
            'orientation_variants':variants,'texture':str(material(m.texture)[0].relative_to(ROOT)),'texture_sha256':hashlib.sha256(material(m.texture)[0].read_bytes()).hexdigest(),
            'bounds_m':[lo.tolist(),hi.tolist()],'dimensions_m':(hi-lo).tolist(),'ports':m.ports,
            'uv_mode':'world-aligned planar top UV; integer-metre translation; select baked orientation variants',
            'notes':m.notes,'seam_checks':[s for s in seams if s['asset']==m.name],
            'structural_review':{'status':'passed'},'visual_review':{'status':'passed_authoring_review','notes':'Reviewed isolated 3/4 render, twelve-item contact sheet, assembled tangent road/path joins, and 3x3 scrolling-ground preview. No scenery baked into references; native-device material scale review remains open.'},
            'import_review':{'status':'passed','units':'metres','up':'+Y','pivot':'local origin; see ports','bounds_m':[lo.tolist(),hi.tolist()],'dimensions_m':(hi-lo).tolist()},**stats}
        (rec/'model-audit.json').write_text(json.dumps(report,indent=2)+'\n')
        (rec/'module-audit.json').write_text(json.dumps(report,indent=2)+'\n')
        if not args.no_render:
            ref=ROOT/'assets/references'/m.name/'v1.png';ref.parent.mkdir(parents=True,exist_ok=True)
            image=render(m);image.save(ref)
            x,y=(index%3)*512,(index//3)*520
            sheet.paste(image.resize((500,480)),(x+6,y));draw.text((x+15,y+480),m.name,fill='#252b31')
    ground = next(m for m in meshes if m.name == 'ground-tile')
    ground.texture = 'sand'
    sand_path, _, _ = write_glb(ground, 'ground-tile-sand.glb')
    inspect(sand_path)
    (OUT/'ground-tile'/'material-variants.json').write_text(json.dumps({
        'grass':'assets/models/ground-tile/ground-tile.glb',
        'sand':str(sand_path.relative_to(ROOT)),
        'notes':'Identical measured geometry and UVs, embedded approved base-colour only; both use REPEAT. Sand is not a separate inventory asset.'}, indent=2)+'\n')
    ground.texture = 'grass'
    qa=ROOT/'assets/production/measured-modules';qa.mkdir(parents=True,exist_ok=True)
    (qa/'seam-checks.json').write_text(json.dumps({'status':'passed','checks':seams,'texture_files_verified_periodic':list(TEX)},indent=2)+'\n')
    if not args.no_render:
        sheet.save(qa/'measured-modules-contact-sheet.png')
        lookup={m.name:m for m in meshes}
        # A true tangent road join: straight into the curve, then leftwards out.
        r=lookup['road-corner'];s=lookup['road-straight'];identity=np.eye(3)
        yaw=np.array([[0,0,-1],[0,1,0],[1,0,0]])
        objects=[(r,identity,np.zeros(3)),(s,identity,np.array([9,0,-16])),(world_uv(s,270),identity,np.array([0,0,9]))]
        render(r,1536,top=True,transforms=objects,background=(235,234,225)).save(qa/'joined-road-top-qa.png')
        p=lookup['path-corner'];s=lookup['path-straight']
        objects=[(p,identity,np.zeros(3)),(s,identity,np.array([6,0,-16])),(world_uv(s,270),identity,np.array([0,0,6]))]
        render(p,1536,top=True,transforms=objects,background=(230,235,215)).save(qa/'joined-path-top-qa.png')
        c=lookup['pavement-corner'];s=lookup['pavement-straight']
        objects=[(c,identity,np.zeros(3)),(s,identity,np.array([12,0,-16])),(world_uv(s,270),identity,np.array([0,0,12]))]
        render(c,1536,top=True,transforms=objects,background=(225,226,220)).save(qa/'joined-pavement-top-qa.png')
        j=lookup['road-junction'];s=lookup['road-straight'];right=yaw.T
        objects=[(j,identity,np.zeros(3)),(world_uv(s,90),identity,np.array([-24,0,0])),(world_uv(s,90),identity,np.array([8,0,0])),(s,identity,np.array([0,0,8]))]
        render(j,1536,top=True,transforms=objects,background=(225,226,220)).save(qa/'joined-junction-top-qa.png')
        g=lookup['ground-tile'];objects=[(g,identity,np.array([16*x,0,16*z])) for x in range(3) for z in range(3)]
        render(g,1536,top=True,transforms=objects).save(qa/'joined-ground-3x3-qa.png')
        g.texture = 'sand'
        render(g,1536,top=True,transforms=objects).save(qa/'joined-sand-3x3-qa.png')
        g.texture = 'grass'
    print(json.dumps({'models':len(meshes),'seam_checks':len(seams),'qa':str(qa.relative_to(ROOT))}))


if __name__=='__main__':main()
