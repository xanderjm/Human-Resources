"""Merge independently reviewed authoring outputs into the shared collection index."""
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return json.loads(path.read_text())


def relative(path):
    path = Path(path)
    return str(path.relative_to(ROOT)) if path.is_absolute() else str(path)


def main():
    path = ROOT / 'assets/production/inventory.json'
    inventory = read(path)
    patch = {a['id']: a for a in read(ROOT / 'assets/production/measured-modules/inventory-patch.json')['assets']}
    for asset in inventory['assets']:
        rec = ROOT / 'assets/production/records' / asset['id']
        kind = asset['kind']
        if kind == 'procedural_model':
            overlay = patch[asset['id']]
            asset.update({k: v for k, v in overlay.items() if k != 'id'})
            asset['reference']['version'] = 1
        elif kind == 'native_graphic':
            audit = read(rec / 'native-audit.json')
            asset.update({k: audit[k] for k in ('reference', 'production', 'qa')})
            asset['reference']['version'] = 1
            asset['qa']['audit'] = relative(rec / 'native-audit.json')
            asset['variants'] = audit.get('variants', [])
            asset['sampling'] = audit.get('usage')
        elif kind == 'tileable_texture':
            audit = read(rec / 'texture-audit.json')
            asset['production'] = {'status': 'complete', 'file': relative(audit['final_file']),
                'sha256': audit['sha256'], 'preview': relative(audit['tiled_preview']),
                'raw_source': relative(audit['preserved_raw_file']), 'credits_consumed': 0}
            asset['qa'] = {'status': audit['visual_qa']['status'],
                'audit': relative(rec / 'texture-audit.json'), 'notes': audit['visual_qa']['notes'],
                'numerical_qa': audit['numerical_qa'], 'runtime_qualified': False}
        elif (rec / 'model-audit.json').exists():
            audit = read(rec / 'model-audit.json')
            review = audit['visual_review']
            passed = review['status'] in ('accepted', 'passed_authoring_review')
            if passed and any(audit.get(k, {}).get('status') != 'passed' for k in ('import_review', 'structural_review')):
                raise ValueError(f"{asset['id']}: visual pass without geometry/import pass")
            asset['production'] = {'status': 'complete' if passed else 'downloaded_' + review['status'],
                'model_file': audit['model_file'], 'raw_model_file': audit.get('raw_model_file'),
                'source_task_id': audit.get('source_task_id'), 'convert_task_id': audit.get('convert_task_id'),
                'credits_consumed': audit.get('credits_consumed', 0)}
            asset['qa'] = {'status': review['status'], 'audit': relative(rec / 'model-audit.json'),
                'notes': review.get('notes', ''), 'runtime_qualified': False}
            asset['actual_dimensions_m'] = audit.get('import_review', {}).get('dimensions_m')
        else:
            asset['production'] = {'status': read(rec / 'production.json')['status'] if (rec / 'production.json').exists() else 'not_started'}
            asset['qa'] = {'status': 'pending'}
    counts = Counter(a['kind'] for a in inventory['assets'])
    statuses = Counter(a['qa']['status'] for a in inventory['assets'])
    ready = sum(statuses[s] for s in ('accepted', 'passed_authoring_review', 'art-review-passed'))
    inventory['status'] = 'ready-for-owner-art-review' if ready == len(inventory['assets']) else 'production-and-review-in-progress'
    inventory['summary'] = {'assets': len(inventory['assets']), 'kinds': dict(counts),
        'qa': dict(statuses), 'authoring_review_passed': ready, 'runtime_qualified': False}
    inventory.pop('previous_goal_turn', None)
    path.write_text(json.dumps(inventory, indent=2) + '\n')
    print(json.dumps(inventory['summary']))


if __name__ == '__main__':
    main()
