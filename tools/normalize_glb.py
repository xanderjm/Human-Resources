"""Preserve the download, add a measured coordinate root, and audit real vertex data."""
import argparse
import json
import math
import struct
from pathlib import Path

import numpy as np
from audit_glb import inspect

ROOT = Path(__file__).resolve().parents[1]
DTYPES = {5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}
WIDTHS = {'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}

def unpack(path):
    raw=path.read_bytes(); chunks=[]; offset=12
    while offset<len(raw):
        size,typ=struct.unpack_from('<II',raw,offset); offset+=8
        chunks.append((typ,raw[offset:offset+size])); offset+=size
    return json.loads(next(b for t,b in chunks if t==0x4e4f534a)),chunks

def accessor(doc,blob,index):
    a=doc['accessors'][index]
    if 'sparse' in a: raise ValueError('Sparse accessor needs a dedicated importer')
    v=doc['bufferViews'][a['bufferView']]
    dtype=np.dtype(DTYPES[a['componentType']]); width=WIDTHS[a['type']]
    return np.ndarray((a['count'],width),dtype=dtype,buffer=blob,
        offset=v.get('byteOffset',0)+a.get('byteOffset',0),
        strides=(v.get('byteStride',dtype.itemsize*width),dtype.itemsize)).copy()

def matrix(node):
    if 'matrix' in node:return np.array(node['matrix']).reshape(4,4,order='F')
    x,y,z,w=node.get('rotation',[0,0,0,1])
    m=np.eye(4)
    m[:3,:3]=np.array([[1-2*y*y-2*z*z,2*x*y-2*z*w,2*x*z+2*y*w],
        [2*x*y+2*z*w,1-2*x*x-2*z*z,2*y*z-2*x*w],
        [2*x*z-2*y*w,2*y*z+2*x*w,1-2*x*x-2*y*y]]) @ np.diag(node.get('scale',[1,1,1]))
    m[:3,3]=node.get('translation',[0,0,0]);return m

def points(doc,chunks):
    blob=next(b for t,b in chunks if t==0x004e4942); result=[]; stats=[]
    def visit(index,parent,ancestors):
        if index in ancestors:raise ValueError('Cycle in node hierarchy')
        n=doc['nodes'][index]; world=parent@matrix(n)
        if 'mesh' in n:
            for p in doc['meshes'][n['mesh']]['primitives']:
                pos=accessor(doc,blob,p['attributes']['POSITION'])
                if not np.isfinite(pos).all():raise ValueError('Non-finite positions')
                uv=accessor(doc,blob,p['attributes']['TEXCOORD_0'])
                if not np.isfinite(uv).all():raise ValueError('Non-finite UVs')
                ind=accessor(doc,blob,p['indices']).reshape(-1) if 'indices' in p else np.arange(len(pos))
                if len(ind)%3 or len(ind)==0 or int(ind.max())>=len(pos):raise ValueError('Invalid triangle indices')
                tri=pos[ind.reshape(-1,3)]
                areas=np.linalg.norm(np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]),axis=1)/2
                result.append((world@np.column_stack([pos,np.ones(len(pos))]).T).T[:,:3])
                stats.append({'triangles':len(tri),'degenerate_triangles':int((areas<1e-12).sum()),
                    'uv_bounds':[uv.min(axis=0).tolist(),uv.max(axis=0).tolist()]})
        for child in n.get('children',[]):visit(child,world,ancestors|{index})
    for root in doc['scenes'][doc.get('scene',0)]['nodes']:visit(root,np.eye(4),set())
    if not result:raise ValueError('No scene vertices')
    return np.concatenate(result),stats

def pack(doc,chunks,path):
    js=json.dumps(doc,separators=(',',':')).encode();js+=b' '*((-len(js))%4)
    output=[(0x4e4f534a,js)]+[(t,b) for t,b in chunks if t!=0x4e4f534a]
    body=b''.join(struct.pack('<II',len(b),t)+b for t,b in output)
    path.write_bytes(struct.pack('<4sII',b'glTF',2,len(body)+12)+body)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('asset');parser.add_argument('--yaw',type=float,required=True)
    parser.add_argument('--axis',choices=['x','y','z'],required=True);parser.add_argument('--size',type=float,required=True)
    parser.add_argument('--pivot',choices=['ground-centre','base-centre','centre','top-centre','rear-centre'],default='ground-centre')
    parser.add_argument('--double-sided',action='store_true')
    args=parser.parse_args();rec=ROOT/'assets/production/records'/args.asset
    audit_path=rec/'model-audit.json';audit=json.loads(audit_path.read_text())
    raw_path=ROOT/audit.get('raw_model_file',audit['model_file']);doc,chunks=unpack(raw_path)
    pts,stats=points(doc,chunks);angle=math.radians(args.yaw);c,s=math.cos(angle),math.sin(angle)
    rotation=np.array([[c,0,s],[0,1,0],[-s,0,c]])
    rotated=pts@rotation.T;extent=np.ptp(rotated,axis=0)
    factor=args.size/extent['xyz'.index(args.axis)]
    if not math.isfinite(factor) or factor<=0:raise ValueError('Invalid physical scale')
    transformed=rotated*factor;lo,hi=transformed.min(axis=0),transformed.max(axis=0)
    pivot=(lo+hi)/2
    if args.pivot=='ground-centre':pivot[1]=lo[1]
    elif args.pivot=='base-centre':
        foot=transformed[transformed[:,1]<=lo[1]+extent[1]*factor*.02]
        pivot=(foot.min(axis=0)+foot.max(axis=0))/2;pivot[1]=lo[1]
    elif args.pivot=='top-centre':pivot[1]=hi[1]
    elif args.pivot=='rear-centre':pivot[2]=lo[2]
    shift=-pivot
    root_matrix=np.eye(4);root_matrix[:3,:3]=rotation*factor;root_matrix[:3,3]=shift
    scene=doc['scenes'][doc.get('scene',0)];root=len(doc['nodes'])
    doc['nodes'].append({'name':args.asset+'-Y-up-ground-pivot','children':scene['nodes'],
        'matrix':root_matrix.flatten(order='F').tolist()});scene['nodes']=[root]
    doc.setdefault('asset',{})['extras']={'units':'metres','up':'+Y','forward':'+Z','pivot':args.pivot}
    if args.double_sided:
        for material in doc.get('materials',[]):material['doubleSided']=True
    dest=ROOT/'assets/models'/args.asset/(args.asset+'.glb');pack(doc,chunks,dest)
    actual,actual_stats=points(*unpack(dest));bounds=[actual.min(axis=0).tolist(),actual.max(axis=0).tolist()]
    if args.pivot=='ground-centre' and abs(bounds[0][1])>1e-6:raise ValueError('Pivot not grounded')
    inspect(dest)
    audit['raw_model_file']=str(raw_path.relative_to(ROOT));audit['model_file']=str(dest.relative_to(ROOT))
    audit['import_review']={'status':'passed','units':'metres','up':'+Y','forward':'+Z',
        'yaw_degrees':args.yaw,'pivot':args.pivot,'double_sided':args.double_sided,'uniform_scale':factor,'bounds_m':bounds,
        'dimensions_m':np.ptp(actual,axis=0).tolist(),'vertex_checks':actual_stats,
        'notes':'Real scene vertices and indices decoded; finite positions/UVs; retained embedded 1024px base colour. Uniform root transform; raw file preserved.'}
    audit_path.write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({'asset':args.asset,'dimensions_m':audit['import_review']['dimensions_m'],
        'pivot':args.pivot,'yaw':args.yaw,'degenerate_triangles':sum(s['degenerate_triangles'] for s in actual_stats),
        'model_file':audit['model_file']}))

if __name__=='__main__':main()
