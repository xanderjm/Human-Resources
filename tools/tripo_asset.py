"""Validated direct-CLI model jobs with durable logs and no automatic resubmission."""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
ENV={**os.environ,'TRIPO_CLIENT_APP':'codex'}

def save(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    tmp=path.with_suffix(path.suffix+'.tmp')
    tmp.write_text(json.dumps(data,indent=2)+'\n')
    tmp.replace(path)

def asset(asset_id):
    d=json.loads((ROOT/'assets/production/inventory.json').read_text())
    matches=[a for a in d['assets'] if a['id']==asset_id]
    if len(matches)!=1:raise ValueError('Unknown asset ID')
    a=matches[0]
    if a['kind']!='tripo_model':raise ValueError('This asset is not a Tripo model')
    if a['reference']['status']!='approved':raise ValueError('Reference has not passed visual review')
    reference=(ROOT/a['reference']['path']).resolve()
    if not reference.is_file():raise ValueError('Approved reference is missing')
    out=ROOT/'assets/models'/asset_id
    rec=ROOT/'assets/production/records'/asset_id
    rec.mkdir(parents=True,exist_ok=True)
    return a,reference,out,rec

def command(a,reference,out):
    return ['tripo','make',str(reference),'--model','tripo-p1',
      '-p',f"face_limit={a['face_limit']}",'-p','texture=true','-p','pbr=false',
      '-p','texture_quality=standard','-p','texture_alignment=original_image','-p','export_uv=true',
      '--then','convert:format=GLTF,texture_size=1024','--out',str(out),'--name',a['id'],'--json']

def validate_dry_run(d,a):
    if d.get('valid') is not True or d.get('errors')!=[]:raise ValueError('Dry run did not validate cleanly')
    if d.get('model')!='P1-20260311':raise ValueError('Unexpected generation model')
    steps=d.get('steps',[])
    if len(steps)!=2:raise ValueError('Only generation and conversion are allowed')
    if steps[0].get('endpoint')!='/v3/generation/image-to-model':raise ValueError('Unexpected endpoint')
    p=steps[0]['payload']
    expected={'model':'P1-20260311','face_limit':a['face_limit'],'texture':True,'pbr':False,
      'texture_quality':'standard','texture_alignment':'original_image','export_uv':True}
    for k,v in expected.items():
        if p.get(k)!=v:raise ValueError(f'Wrong generation parameter: {k}')
    convert=steps[1]
    if convert.get('endpoint')!='/v3/models/convert':raise ValueError('Unexpected conversion endpoint')
    if convert['payload'].get('format')!='GLTF' or convert['payload'].get('texture_size')!=1024:
        raise ValueError('Wrong export format or texture size')
    if any(s.get('task_type') in ('rig','animation','texture_model') for s in steps):
        raise ValueError('Disallowed rigging/retexturing step')

def dry_run(a,reference,out,rec):
    result=subprocess.run(command(a,reference,out)+['--dry-run'],env=ENV,text=True,capture_output=True)
    (rec/'dry-run.stderr.txt').write_text(result.stderr)
    if result.returncode:raise RuntimeError(result.stderr or result.stdout)
    d=json.loads(result.stdout)
    validate_dry_run(d,a)
    save(rec/'dry-run.json',d)
    return d

def task_ids(value):
    ids=[]
    if isinstance(value,dict):
        for k,v in value.items():
            if k in ('task_id','taskId','source_task_id') and isinstance(v,str):ids.append(v)
            else:ids.extend(task_ids(v))
    elif isinstance(value,list):
        for v in value:ids.extend(task_ids(v))
    return ids

def stream(cmd,rec,state):
    proc=subprocess.Popen(cmd,env=ENV,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
    state['local_pid']=proc.pid
    state['command']=cmd
    save(rec/'production.json',state)
    events=json.loads((rec/'events.json').read_text()) if (rec/'events.json').exists() else []
    with (rec/'generation.ndjson').open('a') as log:
        for line in proc.stdout:
            log.write(line);log.flush()
            try:d=json.loads(line)
            except json.JSONDecodeError:
                match=re.search(r'task created\s+([0-9a-f-]{36})',line)
                if match and match.group(1) not in state['task_ids']:
                    state['task_ids'].append(match.group(1))
                    save(rec/'production.json',state)
                if line.strip():print(line.rstrip(),flush=True)
                continue
            events.append(d)
            for tid in task_ids(d):
                if tid not in state['task_ids']:state['task_ids'].append(tid)
            save(rec/'production.json',state)
            # Show the CLI's actual event, but preserve its full structured result locally.
            print(json.dumps(d),flush=True)
    code=proc.wait()
    state['return_code']=code
    downloaded=any(isinstance(e,dict) and e.get('model_file') and Path(e['model_file']).is_file() for e in events)
    state['status']=('downloaded_pending_model_audit' if downloaded else 'artifact_download_pending') if code==0 else 'needs_task_inspection'
    state['finished_at']=datetime.now(timezone.utc).isoformat()
    save(rec/'production.json',state)
    save(rec/'events.json',events)
    return code

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('action',choices=['dry-run','generate','resume','remake'])
    parser.add_argument('asset_id')
    parser.add_argument('--task-id')
    args=parser.parse_args()
    a,reference,out,rec=asset(args.asset_id)
    if args.action=='dry-run':
        d=dry_run(a,reference,out,rec)
        print(json.dumps({'asset':a['id'],'valid':d['valid'],'face_limit':a['face_limit'],'warnings':d.get('warnings',[])}));return
    if args.action=='resume':
        if not args.task_id:raise ValueError('Resume requires an explicitly verified existing task ID')
        state=json.loads((rec/'production.json').read_text())
        if args.task_id not in state['task_ids']:raise ValueError('Task ID is not in this asset job record')
        state['status']='resuming_existing_task'
        sys.exit(stream(['tripo','task','watch',args.task_id,'--download','--out',str(out),'--timeout','600','--json','--no-open'],rec,state))
    if args.action=='remake':
        prior=json.loads((rec/'production.json').read_text())
        audit=json.loads((rec/'model-audit.json').read_text())
        if audit.get('visual_review',{}).get('status')!='rejected':
            raise ValueError('Remake requires a recorded visual rejection, not an uncertain task state')
        if prior.get('return_code')!=0 or prior.get('reference')==str(reference):
            raise ValueError('Remake requires a completed prior task and a newly approved reference')
        attempts=rec/'attempts';attempts.mkdir(exist_ok=True)
        archive=attempts/f'attempt-{len(list(attempts.iterdir()))+1:02d}';archive.mkdir()
        for path in list(rec.iterdir()):
            if path.is_file() and not path.name.startswith('reference-'):
                shutil.move(str(path),str(archive/path.name))
    # A lock/state is a duplicate-submission guard, never evidence that a job is still live.
    lock=rec/'submission.lock'
    try:fd=os.open(lock,os.O_CREAT|os.O_EXCL|os.O_WRONLY)
    except FileExistsError:raise RuntimeError('Submission already attempted: inspect task IDs/history, never blindly resubmit')
    os.close(fd)
    try:dry_run(a,reference,out,rec)
    except Exception:
        lock.unlink();raise
    state={'asset':a['id'],'status':'submitting','task_ids':[],
      'started_at':datetime.now(timezone.utc).isoformat(),'reference':str(reference),
      'reference_version':a['reference']['version']}
    save(rec/'production.json',state)
    sys.exit(stream(command(a,reference,out)+['--yes','--no-open','--timeout','600'],rec,state))

if __name__=='__main__':
    main()
