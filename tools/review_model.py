"""Record a human-visible multi-angle review after structural and import checks."""
import argparse
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('asset');p.add_argument('status',choices=['accepted','rejected'])
p.add_argument('--notes',required=True);p.add_argument('--views',nargs='+',required=True);args=p.parse_args()
path=ROOT/'assets/production/records'/args.asset/'model-audit.json'
audit=json.loads(path.read_text())
if args.status=='accepted':
    if any(audit.get(k,{}).get('status')!='passed' for k in ('structural_review','import_review')):
        raise ValueError('Acceptance requires passed structural and import audits')
    if not (ROOT/audit['model_file']).is_file():raise ValueError('Final model missing')
audit['visual_review']={'status':args.status,'notes':args.notes,'views':args.views}
path.write_text(json.dumps(audit,indent=2)+'\n')
inv_path=ROOT/'assets/production/inventory.json';inv=json.loads(inv_path.read_text())
a=next(a for a in inv['assets'] if a['id']==args.asset)
a['production']={'status':'complete' if args.status=='accepted' else 'downloaded_rejected',
    'model_file':audit['model_file'],'raw_model_file':audit.get('raw_model_file',audit['model_file']),
    'credits_consumed':audit['credits_consumed'],'source_task_id':audit['source_task_id'],
    'convert_task_id':audit['convert_task_id']}
a['qa']={'status':args.status,'notes':args.notes,'audit':str(path.relative_to(ROOT))}
inv_path.write_text(json.dumps(inv,indent=2)+'\n');print(args.asset+': '+args.status)
