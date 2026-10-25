"""Persist a visually reviewed generated reference without replacing older versions."""
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
for asset_id in sys.argv[1:]:
    rec = ROOT / 'assets/production/records' / asset_id
    approvals = sorted(rec.glob('reference-v*-approval.json'), key=lambda p: int(p.stem.split('-')[1][1:]))
    approval = json.loads(approvals[-1].read_text())
    dest = ROOT / 'assets/references' / asset_id / f"v{approval['version']}.png"
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists():
        if dest.read_bytes() != Path(approval['source']).read_bytes():
            raise ValueError('Existing version differs; make a new version')
    else:
        shutil.copy2(approval['source'], dest)
    inventory_path = ROOT / 'assets/production/inventory.json'
    inventory = json.loads(inventory_path.read_text())
    asset = next(a for a in inventory['assets'] if a['id'] == asset_id)
    asset['reference'] = {'status': 'approved', 'version': approval['version'],
        'path': str(dest.relative_to(ROOT)), 'review': approval['review']}
    inventory_path.write_text(json.dumps(inventory, indent=2) + '\n')
    print(f"{asset_id}: approved reference v{approval['version']}")
