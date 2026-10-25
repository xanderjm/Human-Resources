"""Inspect uncompressed GLB geometry, UVs and embedded textures; preserve raw downloads."""
import io
import json
import struct
import sys
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
def inspect(path):
    raw=path.read_bytes()
    magic,version,length=struct.unpack_from('<4sII',raw)
    if magic!=b'glTF' or version!=2 or length!=len(raw):raise ValueError('Invalid GLB header/length')
    chunks={};offset=12
    while offset<len(raw):
        size,typ=struct.unpack_from('<II',raw,offset);offset+=8
        chunks[typ]=raw[offset:offset+size];offset+=size
    doc=json.loads(chunks[0x4e4f534a]);blob=chunks.get(0x004e4942,b'')
    primitives=[p for m in doc.get('meshes',[]) for p in m['primitives']]
    if not primitives:raise ValueError('No mesh primitives')
    triangles=0;vertices=0;uv=True;bounds=[]
    for p in primitives:
        if p.get('mode',4)!=4:raise ValueError('Unexpected non-triangle primitive')
        pos=doc['accessors'][p['attributes']['POSITION']];vertices+=pos['count']
        triangles+=(doc['accessors'][p['indices']]['count'] if 'indices' in p else pos['count'])//3
        uv=uv and 'TEXCOORD_0' in p['attributes']
        bounds.append({'min':pos.get('min'),'max':pos.get('max')})
    textures=[]
    for im in doc.get('images',[]):
        if 'bufferView' not in im:raise ValueError('Unexpected external image in GLB')
        v=doc['bufferViews'][im['bufferView']];start=v.get('byteOffset',0)
        image=Image.open(io.BytesIO(blob[start:start+v['byteLength']]))
        textures.append({'size':list(image.size),'mode':image.mode})
    if not uv:raise ValueError('UV coordinates missing')
    if not textures or any(t['size']!=[1024,1024] for t in textures):raise ValueError('Missing or incorrect 1024px texture')
    for m in doc.get('materials',[]):
        if any(k in m for k in ['normalTexture','occlusionTexture']):raise ValueError('Unexpected PBR auxiliary maps')
        if 'metallicRoughnessTexture' in m.get('pbrMetallicRoughness',{}):raise ValueError('Unexpected PBR packed map')
    return {'triangles':triangles,'vertices':vertices,'mesh_primitives':len(primitives),'uv_present':uv,'textures':textures,'raw_accessor_bounds':bounds,'extensions':doc.get('extensionsUsed',[]),'bytes':len(raw)}

def main():
    asset_id=sys.argv[1];record=ROOT/'assets/production/records'/asset_id
    state=json.loads((record/'production.json').read_text())
    events=json.loads((record/'events.json').read_text())
    finals=[e for e in events if isinstance(e,dict) and e.get('model_file')]
    if not finals:raise ValueError('No downloaded model_file in CLI result')
    final=finals[-1];path=Path(final['model_file'])
    costs={}
    for event in events:
        if not isinstance(event,dict):continue
        for row in event.get('credits_breakdown',[]):costs[row['task_id']]=row['credits_consumed']
        if event.get('task_id') and event.get('credits_consumed') is not None and not event.get('credits_breakdown'):
            costs.setdefault(event['task_id'],event['credits_consumed'])
    source=next((e.get('source_task_id') for e in events if isinstance(e,dict) and e.get('source_task_id')),None)
    report=inspect(path)
    old_path=record/'model-audit.json'
    old=json.loads(old_path.read_text()) if old_path.exists() else {}
    inventory=json.loads((ROOT/'assets/production/inventory.json').read_text())
    asset=next(a for a in inventory['assets'] if a['id']==asset_id)
    ceiling=8000 if asset['category'] in ('characters','animals') else 3000
    budget={'requested_target':asset['face_limit'],'actual_triangles':report['triangles'],
        'target_met':report['triangles']<=asset['face_limit'],'category_ceiling':ceiling,
        'status':'passed' if report['triangles']<=ceiling else 'failed'}
    report.update(asset=asset_id,model_file=str(path.relative_to(ROOT)),requested_face_limit=asset['face_limit'],
        source_task_id=source,convert_task_id=final.get('task_id'),
        credits_consumed=sum(costs.values()) if costs else final.get('credits_consumed'),preview=final.get('preview'),
        triangle_budget_review=budget,
        structural_review={'status':budget['status'],'notes':'Triangle mode, valid GLB length, UVs and embedded 1024px base colour checked. Requested targets are approximate; actual triangle count and target deviations are reported separately, within the user-specified 3000-prop / 8000-character ceilings. Scale, pivot, topology and visual quality require separate review.'},
        visual_review={'status':'pending'},import_review={'status':'pending'})
    if old.get('source_task_id')==report['source_task_id']:
        for key in ('visual_review','import_review','raw_model_file','model_file'):
            if key in old:report[key]=old[key]
    (record/'model-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
