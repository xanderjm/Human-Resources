"""Independent glTF subframe reference for native animation parity tests.

HRT1 is test evidence only, never a runtime package asset. It stores sparse
skinned positions/normals and a head/socket world transform at every key and
half-key across all clips. Uses source node/channel indices, not cooked order.
"""
import struct
import numpy as np
from normalize_glb import accessor, matrix
from validate_character_animation import load_skin


def quaternion(a,b,t):
    a=a.astype(float);b=b.astype(float);dot=np.dot(a,b)
    if dot<0:b=-b;dot=-dot
    if dot>.9995:q=a+(b-a)*t
    else:
        angle=np.arccos(np.clip(dot,0,1))
        q=(np.sin((1-t)*angle)*a+np.sin(t*angle)*b)/np.sin(angle)
    return q/np.linalg.norm(q)


def world_at(doc,blob,animation,t):
    nodes=[dict(n) for n in doc['nodes']]
    for channel in animation['channels']:
        sampler=animation['samplers'][channel['sampler']]
        times=accessor(doc,blob,sampler['input']).ravel()
        values=accessor(doc,blob,sampler['output'])
        i=max(0,min(len(times)-2,int(np.searchsorted(times,t,side='right')-1)))
        fraction=float(np.clip((t-times[i])/(times[i+1]-times[i]),0,1))
        path=channel['target']['path']
        value=quaternion(values[i],values[i+1],fraction) if path=='rotation' else values[i]+(values[i+1]-values[i])*fraction
        nodes[channel['target']['node']][path]=value
    worlds=[None]*len(nodes)
    def visit(n,parent):
        worlds[n]=parent@matrix(nodes[n])
        for child in nodes[n].get('children',[]):visit(child,worlds[n])
    for n in doc['scenes'][doc.get('scene',0)]['nodes']:visit(n,np.eye(4))
    return worlds


def write_reference(doc,blob,clips,name,output):
    joints,inverse,primitives=load_skin(doc,blob)
    positions=np.concatenate([p[0] for p in primitives])
    weights=np.concatenate([p[1] for p in primitives])
    influences=np.concatenate([p[2] for p in primitives])
    normals=np.concatenate([accessor(doc,blob,p['attributes']['NORMAL']) for p in doc['meshes'][0]['primitives']])
    selected=set(np.linspace(0,len(positions)-1,48,dtype=int).tolist())
    for joint in range(len(joints)):
        strength=np.where(influences==joint,weights,0).sum(1)
        if strength.max()>.1:selected.add(int(np.argmax(strength)))
    selected=np.array(sorted(selected),dtype='<u4')
    positions,weights,influences,normals=[a[selected] for a in [positions,weights,influences,normals]]
    socket_name='socket_head' if any(n.get('name')=='socket_head' for n in doc['nodes']) else 'head'
    socket=next(i for i,n in enumerate(doc['nodes']) if n.get('name')==socket_name)
    count=sum(len(c['frames'])*2-1 for c in clips)
    payload=bytearray(struct.pack('<4sII32s',b'HRT1',count,len(selected),socket_name.encode().ljust(32,b'\0')))
    payload.extend(selected.tobytes())
    for clip_index,clip in enumerate(clips):
        animation=next(a for a in doc['animations'] if a['name']==clip['name'])
        for time in np.linspace(0,clip['seconds'],len(clip['frames'])*2-1):
            # Use the same serialized float time that native seek receives.
            time=float(np.float32(time))
            world=world_at(doc,blob,animation,time)
            palette=np.array([world[j] for j in joints])@inverse
            normal_palette=np.linalg.inv(palette[:,:3,:3]).transpose(0,2,1)
            posed=np.zeros((len(selected),4));normal=np.zeros((len(selected),3))
            for k in range(4):
                posed+=np.einsum('nij,nj->ni',palette[influences[:,k]],positions)*weights[:,k,None]
                normal+=np.einsum('nij,nj->ni',normal_palette[influences[:,k]],normals)*weights[:,k,None]
            normal/=np.linalg.norm(normal,axis=1,keepdims=True)
            payload.extend(struct.pack('<If',clip_index,time))
            payload.extend(np.asarray(np.column_stack([posed[:,:3],normal]),dtype='<f4').tobytes())
            payload.extend(np.asarray(world[socket].ravel(order='F'),dtype='<f4').tobytes())
    (output/(name+'.hrt')).write_bytes(payload)
