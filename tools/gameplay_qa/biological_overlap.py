#!/usr/bin/env python3
"""Measure uncaptured camera-visible body overlap in real native public Game runs."""
from pathlib import Path
import hashlib,json,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/gameplay-qa/biological-overlap';OUT.mkdir(parents=True,exist_ok=True)
measurement=subprocess.run([sys.executable,str(ROOT/'tools/gameplay_qa/measure_body_footprints.py')],cwd=ROOT,text=True,capture_output=True,check=True)
geometry=json.loads(measurement.stdout);(OUT/'footprints.json').write_text(json.dumps(geometry,indent=2)+'\n')
rows=[]
for name in ['farmer-body','cow','sheep','pig','soldier-body']:
    f=geometry['models'][name];rows.append([*f['box_half_extents_xz'],*f['box_center_xz'],f['neutral_core_radius']])
def number(x): return format(x,'.9g')+('F' if '.' in format(x,'.9g') or 'e' in format(x,'.9g') else '.0F')
(OUT/'footprints.h').write_text('struct Footprint {float x,z,cx,cz,core;};\ninline constexpr Footprint footprints[]{'+','.join('{'+','.join(map(number,row))+'}' for row in rows)+'};\n')
sources=sorted((ROOT/'native/human_resources_gameplay/src').glob('*.cpp'))+[ROOT/'native/human_resources_presentation/src/style_scene.cpp',Path(__file__).with_suffix('.cpp')]
inputs=sources+list((ROOT/'native/human_resources_gameplay').rglob('*.h'))+[ROOT/'native/human_resources_gameplay/runtime-spatial.json',ROOT/'native/human_resources_gameplay/tools/generate_tuning.py',ROOT/'native/human_resources_product/src/scene.cpp',ROOT/'tools/gameplay_qa/measure_body_footprints.py']
inputs+=[ROOT/'design/balance'/name for name in ['archero-ufo-system.json','endless-upgrade-system.json','endless-encounter-assumptions.json']]
inputs+=[ROOT/f['path'] for f in geometry['models'].values()]
def hashes():return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in inputs}
before=hashes()
subprocess.run([sys.executable,str(ROOT/'native/human_resources_gameplay/tools/generate_tuning.py'),'--root',str(ROOT),'--output',str(OUT/'include/human_resources/gameplay/tuning.h')],check=True)
cmd=[shutil.which('c++'),'-std=c++20','-O2']
for p in ['native/human_resources_gameplay/include','native/human_resources_presentation/include']:cmd+=['-I'+str(ROOT/p)]
cmd+=['-I'+str(OUT),'-I'+str(OUT/'include')]+list(map(str,sources))+['-o',str(OUT/'overlap')]
subprocess.run(cmd,check=True)
result=subprocess.run([str(OUT/'overlap'),str(OUT/'pairs.csv')],capture_output=True,text=True,check=True)
(OUT/'review.log').write_text(result.stdout);print(result.stdout,end='')
after=hashes();stable=before==after
(OUT/'sources.json').write_text(json.dumps({'stable_during_build_and_run':stable,'sources':before},indent=2)+'\n')
if not stable:sys.exit('Production inputs changed during measurement; rerun before current-source claims.')
