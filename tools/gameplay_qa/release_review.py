#!/usr/bin/env python3
"""Compile independent release audit; no CMake, production or foreground writes."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys,shutil
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=Path,default=ROOT/'build/gameplay-qa/release-review')
parser.add_argument('--freeze',type=Path,help='Require every listed source_sha256 before and after the audit')
args=parser.parse_args()
OUT=args.output_dir if args.output_dir.is_absolute() else ROOT/args.output_dir
OUT.mkdir(parents=True,exist_ok=True)
freeze_path=args.freeze if not args.freeze or args.freeze.is_absolute() else ROOT/args.freeze
freeze_bytes=freeze_path.read_bytes() if freeze_path else None
freeze=json.loads(freeze_bytes) if freeze_bytes else None
def verify_freeze():
    if freeze is None:return None
    observed={path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() if (ROOT/path).is_file() else None for path in freeze['source_sha256']}
    mismatches={path:{'expected':expected,'actual':observed[path]} for path,expected in freeze['source_sha256'].items() if observed[path]!=expected}
    manifest_unchanged=freeze_path.read_bytes()==freeze_bytes
    return {'passed':not mismatches and manifest_unchanged,'manifest_unchanged':manifest_unchanged,'observed':observed,'mismatches':mismatches}
freeze_before=verify_freeze()
freeze_evidence={'manifest':str(freeze_path),'manifest_sha256':hashlib.sha256(freeze_bytes).hexdigest(),'source_count':len(freeze['source_sha256']),'scope':freeze.get('scope'),'before':freeze_before} if freeze else None
def write_freeze():
    if freeze_evidence is not None:(OUT/'freeze-verification.json').write_text(json.dumps(freeze_evidence,indent=2)+'\n')
write_freeze()
if freeze_before is not None and not freeze_before['passed']:
    sys.exit('Frozen source mismatch before audit; see freeze-verification.json')
sources=sorted((ROOT/'native/human_resources_gameplay/src').glob('*.cpp'))+sorted((ROOT/'native/human_resources_progression/src').glob('*.cpp'))+[ROOT/'native/human_resources_storage/src/storage.cpp',Path(__file__).with_suffix('.cpp')]
inputs=sources+list((ROOT/'native/human_resources_gameplay').rglob('*.h'))+list((ROOT/'native/human_resources_progression').rglob('*.h'))+list((ROOT/'native/human_resources_storage').rglob('*.h'))+list((ROOT/'design/balance').glob('*.json'))+[ROOT/'native/human_resources_gameplay/runtime-spatial.json',ROOT/'native/human_resources_gameplay/tools/generate_tuning.py',Path(__file__)]
def hashes():return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in inputs}
before=hashes()
subprocess.run([sys.executable,str(ROOT/'native/human_resources_gameplay/tools/generate_tuning.py'),'--root',str(ROOT),'--output',str(OUT/'include/human_resources/gameplay/tuning.h')],check=True)
cmd=[shutil.which('c++'),'-std=c++20','-O2','-fno-access-control']
for p in ['native/human_resources_gameplay/include','native/human_resources_gameplay/src','native/human_resources_progression/include','native/human_resources_storage/include','native/tests']:cmd+=['-I'+str(ROOT/p)]
cmd+=['-I'+str(OUT/'include')]+list(map(str,sources))+['-o',str(OUT/'release-review')]
compiled=subprocess.run(cmd)
if compiled.returncode:
    if freeze_evidence is not None:freeze_evidence['after']=verify_freeze();write_freeze()
    sys.exit(compiled.returncode)
result=subprocess.run([str(OUT/'release-review')],capture_output=True,text=True)
(OUT/'review.log').write_text(result.stdout+result.stderr);print(result.stdout,end='')
after=hashes();(OUT/'sources.json').write_text(json.dumps({'stable_during_build_and_run':before==after,'sources':before},indent=2)+'\n')
if freeze_evidence is not None:freeze_evidence['after']=verify_freeze();write_freeze()
if before!=after:sys.exit('Production sources changed during audit; see sources.json')
if freeze_evidence is not None and not freeze_evidence['after']['passed']:sys.exit('Frozen source mismatch after audit; see freeze-verification.json')
sys.exit(result.returncode)
