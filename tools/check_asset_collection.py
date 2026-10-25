"""Check the exported authoring collection, retaining exact file/hash evidence."""
import hashlib
import json
from pathlib import Path
import sys

import numpy as np
from PIL import Image
from audit_glb import inspect
from normalize_glb import points, unpack

ROOT = Path(__file__).resolve().parents[1]
ACCEPTED = {'accepted', 'passed_authoring_review', 'art-review-passed'}


def read(path):
    return json.loads(path.read_text())


def existing(path):
    file = Path(path)
    if not file.is_absolute():
        file = ROOT / file
    if not file.is_file():
        raise ValueError(f'Missing exported file: {file}')
    return file


def fingerprint(path):
    file = existing(path)
    return {'file': str(file.relative_to(ROOT)), 'bytes': file.stat().st_size,
            'sha256': hashlib.sha256(file.read_bytes()).hexdigest()}


def model_file(path, ceiling=None, require_static=False):
    file = existing(path)
    result = inspect(file)
    doc, chunks = unpack(file)
    if require_static and (doc.get('skins') or doc.get('animations')):
        raise ValueError('Source model unexpectedly contains rigging or animation')
    pts, stats = points(doc, chunks)
    if not np.isfinite(pts).all() or any(s['degenerate_triangles'] for s in stats):
        raise ValueError(f'Invalid vertex/triangle data: {path}')
    if ceiling and result['triangles'] > ceiling:
        raise ValueError(f'Triangle category ceiling exceeded: {path}')
    return {**fingerprint(path), 'triangles': result['triangles'],
            'bounds_m': [pts.min(0).tolist(), pts.max(0).tolist()]}


def texture_file(asset, rec):
    audit = read(rec / 'texture-audit.json')
    path = existing(asset['production']['file'])
    pixels = np.asarray(Image.open(path))
    if pixels.shape != (1024, 1024, 3):
        raise ValueError('Expected 1024 RGB tile')
    p = pixels.astype(np.int16)
    errors = [p[0] - p[-1], p[:, 0] - p[:, -1],
              (p[1] - p[0]) - (p[-1] - p[-2]),
              (p[:, 1] - p[:, 0]) - (p[:, -1] - p[:, -2])]
    if any(np.any(error) for error in errors):
        raise ValueError(f"Texture edge or slope mismatch: {asset['id']}")
    if fingerprint(path)['sha256'] != audit['sha256']:
        raise ValueError('Reviewed texture hash changed')
    existing(audit['tiled_preview'])
    return {**fingerprint(path), 'edge_and_first_difference_max': 0}


def graphic_file(asset, rec):
    audit = read(rec / 'native-audit.json')
    path = existing(asset['production']['file'])
    image = Image.open(path)
    if image.mode != 'RGBA' or image.getextrema()[-1][0] != 0:
        raise ValueError(f"Expected transparent RGBA sprite: {asset['id']}")
    if fingerprint(path)['sha256'] != audit['alpha_audit']['sha256']:
        raise ValueError('Reviewed sprite hash changed')
    existing(asset['production']['source'])
    if asset['id'] == 'fx-laser':
        p = np.asarray(image).astype(np.int16)
        if np.any(p[:, 0] != p[:, -1]) or np.any(p[:, 1]-p[:, 0] != p[:, -1]-p[:, -2]):
            raise ValueError('Laser repeat axis mismatch')
    extra = audit.get('supplemental_geometry')
    if extra:
        existing(extra['path'])
        existing(extra['shader_helper'])
    for variant in audit.get('variants', []):
        existing(variant['path'])
    return fingerprint(path)


def main():
    inventory = read(ROOT / 'assets/production/inventory.json')
    ids = [a['id'] for a in inventory['assets']]
    if len(ids) != len(set(ids)):
        raise ValueError('Duplicate inventory IDs')
    rows = []
    failures = []
    for asset in inventory['assets']:
        try:
            rec = ROOT / 'assets/production/records' / asset['id']
            existing(asset['reference']['path'])
            if asset['reference']['status'] != 'approved' or asset['qa']['status'] not in ACCEPTED:
                raise ValueError(f"Not approved for authoring review: {asset['qa']['status']}")
            kind = asset['kind']
            if kind in ('tripo_model', 'procedural_model'):
                audit = read(rec / 'model-audit.json')
                review = audit['visual_review']
                reviewed_hash = review.get('model_sha256', review.get('reviewed_model_sha256'))
                if reviewed_hash and fingerprint(asset['production']['model_file'])['sha256'] != reviewed_hash:
                    raise ValueError('Exported model changed after its visual review')
                ref_hash = review.get('reference_sha256', review.get('reviewed_reference_sha256'))
                if ref_hash and fingerprint(asset['reference']['path'])['sha256'] != ref_hash:
                    raise ValueError('Approved reference changed after model review')
                ceiling = 8000 if asset['category'] in ('characters', 'humans', 'animals') else 3000
                if kind == 'procedural_model':
                    ceiling = None
                if asset.get('group') == 'infantry-weapons':
                    state = read(rec / 'production.json')
                    if existing(state['reference']) != existing(asset['reference']['path']):
                        raise ValueError('Submitted image differs from approved isolated reference')
                    dry = read(rec / 'dry-run.json')
                    if not dry.get('valid') or dry.get('errors'):
                        raise ValueError('Tripo dry run did not pass')
                    if asset['id'].startswith('infantry-'):
                        approval = read(rec / 'reference-v1-approval.json')
                        if approval.get('input_contract') != 'unarmed_true_t_pose':
                            raise ValueError('Body input must be approved unarmed true T-pose')
                report = model_file(asset['production']['model_file'], ceiling,
                                    require_static=asset.get('group') == 'infantry-weapons')
                for variant in asset.get('orientation_variants', []):
                    model_file(variant['model_file'])
                if asset['id'] == 'ground-tile':
                    variants = read(rec / 'material-variants.json')
                    for path in variants.values():
                        if isinstance(path, str) and path.endswith('.glb'):
                            model_file(path)
            elif kind == 'tileable_texture':
                report = texture_file(asset, rec)
            else:
                report = graphic_file(asset, rec)
            rows.append({'id': asset['id'], 'kind': kind, **report})
        except Exception as error:
            failures.append({'id': asset['id'], 'error': str(error)})
    report = {'scope': 'Offline authoring collection; not device/runtime qualification',
              'expected': len(ids), 'passed': len(rows), 'failures': failures, 'files': rows}
    (ROOT / 'assets/production/collection-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'expected': len(ids), 'passed': len(rows), 'failures': failures}))
    if failures:
        sys.exit(1)


if __name__ == '__main__':
    main()
