"""Offline orthographic review of actual GLB triangles and embedded base-colour texture.

Development evidence only. Uses a small CPU depth buffer, without runtime shaders.
"""
import argparse
import io
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from normalize_glb import unpack, accessor, matrix

ROOT = Path(__file__).resolve().parents[1]
VIEWS = {'three-quarter':(1,.65,1),'opposite':(-1,.55,-1),'front':(0,.30,1),'right':(1,.30,0)}

def primitives(doc, chunks):
    blob=next(b for t,b in chunks if t==0x004e4942)
    def visit(index,parent):
        node=doc['nodes'][index]; world=parent@matrix(node)
        if 'mesh' in node:
            for primitive in doc['meshes'][node['mesh']]['primitives']:
                pos=accessor(doc,blob,primitive['attributes']['POSITION'])
                uv=accessor(doc,blob,primitive['attributes']['TEXCOORD_0'])
                ind=accessor(doc,blob,primitive['indices']).reshape(-1,3)
                transformed=(world@np.column_stack([pos,np.ones(len(pos))]).T).T[:,:3]
                material=doc['materials'][primitive.get('material',0)]
                texture=doc['textures'][material['pbrMetallicRoughness']['baseColorTexture']['index']]
                image=doc['images'][texture['source']]; view=doc['bufferViews'][image['bufferView']]
                raw=blob[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
                pixels=np.asarray(Image.open(io.BytesIO(raw)).convert('RGB'))
                yield transformed,uv,ind,pixels
        for child in node.get('children',[]):yield from visit(child,world)
    for node in doc['scenes'][doc.get('scene',0)]['nodes']:yield from visit(node,np.eye(4))

def render(parts,direction,size=512):
    points=np.concatenate([p[0] for p in parts]);centre=(points.min(0)+points.max(0))/2
    camera=np.array(direction,dtype=float);camera/=np.linalg.norm(camera)
    right=np.cross([0,1,0],camera);right/=np.linalg.norm(right);up=np.cross(camera,right)
    basis=np.array([right,up,camera]).T
    positions=[(p[0]-centre)@basis for p in parts]
    allpoints=np.concatenate(positions);span=np.ptp(allpoints[:,:2],axis=0).max()*1.14
    pix=size/span;screenCentre=(allpoints[:,:2].min(0)+allpoints[:,:2].max(0))/2
    canvas=np.full((size,size,3),[246,247,242],dtype=np.uint8);depth=np.full((size,size),-np.inf)
    light=np.array([-.5,.85,.6]);light/=np.linalg.norm(light)
    for (points,uv,triangles,texture),viewpoints in zip(parts,positions):
        screen=(viewpoints[:,:2]-screenCentre)*pix;screen[:,0]+=size/2;screen[:,1]=size/2-screen[:,1]
        for indices in triangles:
            vertices=screen[indices];x0,y0=np.floor(vertices.min(0)).astype(int);x1,y1=np.ceil(vertices.max(0)).astype(int)
            x0=max(0,x0);y0=max(0,y0);x1=min(size-1,x1);y1=min(size-1,y1)
            if x0>x1 or y0>y1:continue
            a,b,c=vertices;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(den)<1e-10:continue
            yy,xx=np.mgrid[y0:y1+1,x0:x1+1];x=xx+.5;y=yy+.5
            w0=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/den
            w1=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/den;w2=1-w0-w1
            z=w0*viewpoints[indices[0],2]+w1*viewpoints[indices[1],2]+w2*viewpoints[indices[2],2]
            mask=(w0>=-1e-8)&(w1>=-1e-8)&(w2>=-1e-8)&(z>depth[yy,xx])
            if not mask.any():continue
            mapped=w0[mask,None]*uv[indices[0]]+w1[mask,None]*uv[indices[1]]+w2[mask,None]*uv[indices[2]]
            tx=np.clip((mapped[:,0]*(texture.shape[1]-1)).astype(int),0,texture.shape[1]-1)
            ty=np.clip((mapped[:,1]*(texture.shape[0]-1)).astype(int),0,texture.shape[0]-1)
            normal=np.cross(points[indices[1]]-points[indices[0]],points[indices[2]]-points[indices[0]])
            norm=np.linalg.norm(normal);normal=normal/norm if norm else normal
            shade=.75+.25*max(0,float(normal@light))
            canvas[yy[mask],xx[mask]]=np.clip(texture[ty,tx]*shade,0,255).astype(np.uint8);depth[yy[mask],xx[mask]]=z[mask]
    return Image.fromarray(canvas)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('assets',nargs='+');parser.add_argument('--edition',default='raw');args=parser.parse_args()
    sheet=Image.new('RGB',(1600,350*len(args.assets)),(236,238,232));draw=ImageDraw.Draw(sheet)
    inventory={a['id']:a for a in json.loads((ROOT/'assets/production/inventory.json').read_text())['assets']}
    for row,id in enumerate(args.assets):
        rec=ROOT/'assets/production/records'/id;audit=json.loads((rec/'model-audit.json').read_text());model=ROOT/audit['model_file'];parts=list(primitives(*unpack(model)))
        for col,(name,direction) in enumerate(VIEWS.items()):
            image=render(parts,direction);image.save(rec/f'review-{args.edition}-{name}.png');image.thumbnail((310,310));sheet.paste(image,(col*320+5,row*350+30));draw.text((col*320+8,row*350+8),id+' / '+name,fill=(24,51,72))
        reference=Image.open(ROOT/inventory[id]['reference']['path']).convert('RGB');reference.thumbnail((310,310));sheet.paste(reference,(1285,row*350+30));draw.text((1288,row*350+8),'Approved reference',fill=(24,51,72))
    output=ROOT/'assets/production/reviews'/f'props-six-{args.edition}.png';sheet.save(output);print(output)
if __name__=='__main__':main()
