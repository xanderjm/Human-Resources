"""Reproduce art assembly QA using actual normalized GLBs; no runtime integration."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from normalize_glb import matrix
from render_prop_review import primitives, render
from normalize_glb import unpack

ROOT=Path(__file__).resolve().parents[1]
DEFINITION=ROOT/'assets/production/assemblies.json'
VIEWS={'three-quarter':(1,.65,1),'opposite':(-1,.55,-1),'front':(0,.25,1),'side':(1,.25,0),'high':(1,1.6,1)}

def placement(translation,yaw=0):
    angle=np.radians(yaw);return matrix({'translation':translation,'rotation':[0,float(np.sin(angle/2)),0,float(np.cos(angle/2))]})

def assemble(recipe):
    worlds={};meshes=[]
    for node in recipe['nodes']:
        if 'attach_to_socket' in node:
            socket=next(s for s in recipe['sockets'] if s['id']==node['attach_to_socket'])
            parent=worlds[socket['parent_node']];world=parent@placement(socket['translation_m'],socket.get('yaw_degrees',0))
        else:world=placement(node.get('translation_m',[0,0,0]),node.get('yaw_degrees',0))
        worlds[node['id']]=world
        model=ROOT/node['model_file'];parts=list(primitives(*unpack(model)))
        for positions,uv,triangles,texture in parts:
            transformed=(world@np.column_stack([positions,np.ones(len(positions))]).T).T[:,:3]
            meshes.append((transformed,uv,triangles,texture))
    return meshes,worlds

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--edition',default='fit');args=parser.parse_args()
    document=json.loads(DEFINITION.read_text());recipes=document['assemblies'];sheet=Image.new('RGB',(1600,350*len(recipes)),(236,238,232));draw=ImageDraw.Draw(sheet)
    for row,recipe in enumerate(recipes):
        parts,worlds=assemble(recipe);points=np.concatenate([p[0] for p in parts]);recipe['measured_bounds_m']=[points.min(0).tolist(),points.max(0).tolist()]
        for col,(name,direction) in enumerate(VIEWS.items()):
            image=render(parts,direction);filename=f"assembly-{recipe['id']}-{args.edition}-{name}.png";image.save(ROOT/'assets/production/reviews'/filename)
            image.thumbnail((310,310));sheet.paste(image,(col*320+5,row*350+30));draw.text((col*320+8,row*350+8),recipe['id']+' / '+name,fill=(24,51,72))
        recipe['render_evidence']=[f"assets/production/reviews/assembly-{recipe['id']}-{args.edition}-{name}.png" for name in VIEWS]
        for node in recipe['nodes']:
            digest=hashlib.sha256((ROOT/node['model_file']).read_bytes()).hexdigest()
            if node.get('source_sha256') and node['source_sha256']!=digest:
                recipe['fit_review']['status']='pending'
                recipe['fit_review']['needs_review_reason']='A source model changed since the recorded visual fit review.'
            node['source_sha256']=digest
    output=ROOT/'assets/production/reviews'/f'assemblies-{args.edition}.png';sheet.save(output);document['contact_sheet']=str(output.relative_to(ROOT));DEFINITION.write_text(json.dumps(document,indent=2)+'\n');print(output)
if __name__=='__main__':main()
