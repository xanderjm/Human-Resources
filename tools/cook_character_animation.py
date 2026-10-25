"""Bounded offline GLB -> HRA1 skeleton/clips + matching HRM1 textured mesh.

Rebuilds missing/stale exports from the durable recipes and original source GLBs.
NumPy/Pillow and Blender are authoring-only. No parser/tool dependency ships.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
from contextlib import ExitStack
from transaction_character_animation import locked_output, staging, publish, snapshot
import subprocess
import sys
import numpy as np
from PIL import Image
from normalize_glb import accessor, matrix, unpack
from cook_style_preview import write_mesh
from art_tools import blender_executable

ROOT = Path(__file__).resolve().parents[1]
EXPORTS = ROOT/'build/animation/exports'
MAX_BYTES, MAX_NODES, MAX_VERTICES = 16*1024*1024, 64, 32768


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def name_bytes(name):
    if not name or len(name)>31 or any(not (c.isascii() and (c.isalnum() or c in '_.-')) for c in name):
        raise ValueError('Invalid HRA1 name: '+name)
    return name.encode('ascii').ljust(32,b'\0')


def fresh_exports(config):
    try:
        validation = json.loads((EXPORTS.parent/'reports/validation.json').read_text())
        hashes = {r['id']:r['sha256'] for r in validation['characters']}
        for c in config['characters']:
            path = EXPORTS/(c['id']+'.glb')
            report = json.loads((EXPORTS.parent/'reports'/(c['id']+'.json')).read_text())
            if (report['config'] != c or digest(path)!=hashes[c['id']] or
                    digest(ROOT/report['source'])!=report['source_sha256'] or
                    not report['authoring_inputs'] or
                    any(digest(ROOT/p)!=h for p,h in report['authoring_inputs'].items())):
                return False
        return True
    except (FileNotFoundError,KeyError,ValueError):
        return False


def ensure_exports(config,blender,rebuild):
    # Caller owns the published export lock through the final cook reads.
    output=EXPORTS.parent
    with staging(output) as stage:
        if rebuild or not fresh_exports(config):
            blender = blender_executable(blender)
            # Child builder locks its unique destination, never the parent's
            # published root. It has no privileged lock-bypass switch.
            subprocess.run([str(blender.resolve()),'--background','--factory-startup','--python-exit-code','1',
                            '--python',str(ROOT/'tools/build_character_animation.py'),'--',
                            '--output-root',str(stage),'--validator-python',sys.executable],cwd=ROOT,check=True)
        else:
            snapshot(output,stage,['exports','reports'])
        for tool in ['validate_character_animation.py','validate_shared_walks.py']:
            subprocess.run([sys.executable,str(ROOT/'tools'/tool),'--output-root',str(stage)],cwd=ROOT,check=True)
        for c in config['characters']:
            report=json.loads((stage/'reports'/(c['id']+'.json')).read_text())
            for name,expected in {**report['authoring_inputs'],report['source']:report['source_sha256']}.items():
                if digest(ROOT/name)!=expected:raise ValueError('Authoring input changed during validation: '+name)
        files=[p.relative_to(stage) for p in stage.rglob('*') if p.is_file() and
               not p.name.startswith('.animation-')]
        files.sort(key=lambda p:p.as_posix()=='reports/validation.json')
        publish(output,stage,files)


def hierarchy(doc):
    if not 0<len(doc['nodes'])<=MAX_NODES:
        raise ValueError('Node limit')
    order,parents,seen = [],{},set()
    def visit(index,parent):
        if index in seen or not 0<=index<len(doc['nodes']):
            raise ValueError('Cyclic, multiply parented or invalid hierarchy')
        seen.add(index);parents[index]=parent;order.append(index)
        for child in doc['nodes'][index].get('children',[]):visit(child,index)
    for index in doc['scenes'][doc.get('scene',0)]['nodes']:visit(index,-1)
    if len(seen)!=len(doc['nodes']):raise ValueError('Unreachable node')
    return order,parents


def transform(node):
    if 'matrix' in node:raise ValueError('HRA1 source requires explicit TRS nodes')
    value = np.array([*node.get('translation',[0,0,0]),*node.get('rotation',[0,0,0,1]),
                      *node.get('scale',[1,1,1])],dtype=np.float64)
    if (not np.isfinite(value).all() or np.abs(value[:3]).max()>10000 or
            abs(np.linalg.norm(value[3:7])-1)>.0004 or value[7:].min()<.5 or value[7:].max()>2):
        raise ValueError('Unsupported/nonfinite TRS')
    return value


def extract_clips(doc,blob,config,character,order):
    records = {r['name']:r for r in json.loads((EXPORTS.parent/'reports'/(character['id']+'.json')).read_text())['clips']}
    clips=[]
    for animation in doc['animations']:
        record=records[animation['name']];duration=record['seconds'];count=record['frames']
        if not 2<=count<=256 or not 0<duration<=10:raise ValueError('Clip limit')
        base=np.array([transform(n) for n in doc['nodes']])
        frames=np.tile(base,(count,1,1))
        visited=set()
        for channel in animation['channels']:
            sampler=animation['samplers'][channel['sampler']]
            if sampler.get('interpolation','LINEAR')!='LINEAR':raise ValueError('Only baked LINEAR clips')
            times=accessor(doc,blob,sampler['input']).ravel()
            if len(times)!=count or not np.allclose(times,np.linspace(0,duration,count),atol=1e-6):
                raise ValueError('Expected uniform closed 30Hz keys')
            index=channel['target']['node'];path=channel['target']['path'];key=(index,path)
            if key in visited:raise ValueError('Duplicate animation channel')
            visited.add(key)
            values=accessor(doc,blob,sampler['output'])
            start,end={'translation':(0,3),'rotation':(3,7),'scale':(7,10)}[path]
            if values.shape!=(count,end-start):raise ValueError('Animation shape')
            frames[:,index,start:end]=values
        frames=frames[:,order,:]
        if (not np.isfinite(frames).all() or np.abs(frames[:,:,:3]).max()>10000 or
                np.max(np.abs(np.linalg.norm(frames[:,:,3:7],axis=2)-1))>.0004 or
                frames[:,:,7:].min()<.5 or frames[:,:,7:].max()>2):
            raise ValueError('Clip transform outside runtime limits')
        clips.append({'name':animation['name'],'seconds':duration,'stride_m':record['stride_m'],
                      'loop':record['loop'],'frames':frames})
    if len(clips)!=len(config['clips']) or len(clips)>16:raise ValueError('Clip count')
    return clips


def mesh_data(doc,blob):
    if len(doc['skins'])!=1 or len(doc['meshes'])!=1 or len(doc['meshes'][0]['primitives'])!=1:
        raise ValueError('Expected one complete skin and one textured triangle primitive')
    p=doc['meshes'][0]['primitives'][0]
    if p.get('mode',4)!=4 or 'targets' in p:raise ValueError('Only skinned triangles without morph targets')
    a=p['attributes']
    pos=accessor(doc,blob,a['POSITION']);normal=accessor(doc,blob,a['NORMAL'])
    uv=accessor(doc,blob,a['TEXCOORD_0']);joints=accessor(doc,blob,a['JOINTS_0']).astype(int)
    weights=accessor(doc,blob,a['WEIGHTS_0']);indices=accessor(doc,blob,p['indices']).ravel()
    if (not 0<len(pos)<=MAX_VERTICES or len(indices)>100000 or len(indices)%3 or
            indices.min()<0 or indices.max()>=len(pos) or uv.min()<-.001 or uv.max()>1.001 or
            joints.min()<0 or joints.max()>=len(doc['skins'][0]['joints']) or
            not np.allclose(weights.sum(1),1,atol=2e-5) or np.any(weights<0)):
        raise ValueError('Mesh/influence bounds')
    if not all(np.isfinite(v).all() for v in [pos,normal,uv,weights]):raise ValueError('Nonfinite mesh')
    mat=doc['materials'][p.get('material',0)];pbr=mat.get('pbrMetallicRoughness',{})
    texture=pbr.get('baseColorTexture')
    if not texture or texture.get('texCoord',0)!=0 or texture.get('extensions'):
        raise ValueError('Expected embedded untransformed basecolour')
    image_info=doc['images'][doc['textures'][texture['index']]['source']]
    view=doc['bufferViews'][image_info['bufferView']];start=view.get('byteOffset',0)
    with Image.open(io.BytesIO(blob[start:start+view['byteLength']])) as image:
        if image.width>2048 or image.height>2048:raise ValueError('Texture limit')
        pixels=image.convert('RGBA').resize((504,504),Image.Resampling.LANCZOS)
    atlas=Image.fromarray(np.pad(np.asarray(pixels),((4,4),(4,4),(0,0)),mode='edge'))
    mapped_uv=(np.clip(uv,0,1)*504+4)/512
    factor=pbr.get('baseColorFactor',[1,1,1,1]);emission=max(mat.get('emissiveFactor',[0,0,0]))
    hrm=np.column_stack([pos,normal,mapped_uv,np.tile(factor,(len(pos),1)),
                         np.full(len(pos),emission),np.zeros(len(pos))])
    return pos,normal,joints,weights,indices,atlas,hrm


def cook(character,config,output,validation_output=None):
    source=EXPORTS/(character['id']+'.glb')
    if source.stat().st_size>MAX_BYTES:raise ValueError('Source GLB limit')
    doc,chunks=unpack(source);blob=next(b for t,b in chunks if t==0x004e4942)
    if doc.get('extensionsRequired'):raise ValueError('Required GLB extensions unsupported')
    order,parents=hierarchy(doc);remap={old:new for new,old in enumerate(order)}
    clips=extract_clips(doc,blob,config,character,order)
    pos,normal,joints,weights,indices,atlas,hrm=mesh_data(doc,blob)
    skin=doc['skins'][0]
    if not 0<len(skin['joints'])<=64:raise ValueError('Joint limit')
    inverse=accessor(doc,blob,skin['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
    payload=bytearray(struct.pack('<4sIIII',b'HRA1',len(order),len(skin['joints']),len(pos),len(clips)))
    for old in order:
        node=doc['nodes'][old]
        parent=-1 if parents[old]<0 else remap[parents[old]]
        payload.extend(struct.pack('<i32s10f',parent,name_bytes(node['name']),*transform(node)))
    for old,m in zip(skin['joints'],inverse):
        payload.extend(struct.pack('<I16f',remap[old],*m.ravel(order='F')))
    for p,n,j,w in zip(pos,normal,joints,weights):payload.extend(struct.pack('<6f4H4f',*p,*n,*j,*w))
    for clip in clips:
        frames=clip['frames']
        payload.extend(struct.pack('<32sffII',name_bytes(clip['name']),clip['seconds'],clip['stride_m'],int(clip['loop']),len(frames)))
        payload.extend(np.asarray(frames,dtype='<f4').tobytes())
    if len(payload)>MAX_BYTES:raise ValueError('Cooked animation byte limit')
    path=output/(character['id']+'.hra');temporary=path.with_suffix('.hra.tmp')
    temporary.write_bytes(payload);temporary.replace(path)
    write_mesh(output/(character['id']+'.hrm'),hrm,indices,atlas)
    if validation_output:
        from animation.runtime_reference import write_reference
        write_reference(doc,blob,clips,character['id'],validation_output)
    authoring=json.loads((EXPORTS.parent/'reports'/(character['id']+'.json')).read_text())
    return {'id':character['id'],'source':str(source.relative_to(ROOT)),'source_sha256':digest(source),
            'source_model':authoring['source'],'source_model_sha256':authoring['source_sha256'],
            'authoring_inputs':authoring['authoring_inputs'],
            'nodes':len(order),'joints':len(skin['joints']),'vertices':len(pos),'triangles':len(indices)//3,
            'animation_bytes':len(payload),'hra_sha256':digest(path),
            'hrm_sha256':digest(output/(character['id']+'.hrm')),
            'clips':[{k:v for k,v in c.items() if k!='frames'} for c in clips]}


def main():
    global EXPORTS
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    p.add_argument('--validation-output',type=Path)
    p.add_argument('--blender',type=Path)
    p.add_argument('--export-root',type=Path,default=ROOT/'build/animation')
    p.add_argument('--rebuild-exports',action='store_true');a=p.parse_args()
    EXPORTS=a.export_root.resolve()/'exports'
    config=json.loads((ROOT/'assets/animation/characters.json').read_text())
    export_root=EXPORTS.parent
    outputs=[a.output]+([a.validation_output] if a.validation_output else [])
    resolved=[p.resolve() for p in outputs]
    if len(set(resolved))!=len(resolved) or export_root.resolve() in resolved:
        raise ValueError('Cook and validation destinations must differ from each other and the export root')
    with ExitStack() as stack:
        # Global export lock is always first; output locks have stable order.
        stack.enter_context(locked_output(export_root))
        for output in sorted(resolved):stack.enter_context(locked_output(output))
        config=json.loads((ROOT/'assets/animation/characters.json').read_text())
        ensure_exports(config,a.blender,a.rebuild_exports)
        cooked=stack.enter_context(staging(a.output))
        reference=stack.enter_context(staging(a.validation_output)) if a.validation_output else None
        records=[cook(c,config,cooked,reference) for c in config['characters']]
        inputs=[Path(__file__),ROOT/'tools/art_tools.py',ROOT/'tools/art-toolchain.json',ROOT/'tools/art-requirements.txt',ROOT/'tools/animation/runtime_reference.py',ROOT/'tools/cook_style_preview.py',ROOT/'tools/normalize_glb.py',ROOT/'tools/transaction_character_animation.py']
        manifest={'schema':1,'format':'HRA1 + matching HRM1','models':records,
                  'tool_sha256':{str(p.relative_to(ROOT)):digest(p) for p in inputs}}
        (cooked/'animation-provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')
        names=[p.relative_to(cooked) for p in cooked.rglob('*') if p.is_file()]
        names.sort(key=lambda p:p.as_posix()=='animation-provenance.json')
        publish(a.output,cooked,names)
        if reference:publish(a.validation_output,reference,[p.relative_to(reference) for p in reference.rglob('*') if p.is_file()])
    print('Cooked six native animated character pairs into',a.output)



if __name__=='__main__':main()
