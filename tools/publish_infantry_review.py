"""Package reviewed independent Tripo models, references and actual render proof."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
GROUP = 'infantry-weapons'


def read(path):
    return json.loads(path.read_text())


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    inventory = read(ROOT / 'assets/production/inventory.json')
    collection = inventory['collections'][GROUP]
    assets = {a['id']: a for a in inventory['assets']}
    ids = collection['assets']
    rows, prompts, files = [], [], set()
    lineup_record = ROOT / 'art/enemies/infantry/lineup-v1-generation.json'
    prompts.append({'kind': 'armed-lineup-guide', **read(lineup_record)})
    files.add(collection['overview'])
    for id in ids:
        asset = assets[id]
        rec = ROOT / 'assets/production/records' / id
        audit = read(rec / 'model-audit.json')
        if audit['visual_review']['status'] != 'accepted' or audit['import_review']['status'] != 'passed':
            raise ValueError(f'Final review is incomplete: {id}')
        model = ROOT / audit['model_file']
        approval = read(rec / f"reference-v{asset['reference']['version']}-approval.json")
        prompts.append({'kind': 'isolated-tripo-input', **approval})
        files.update([audit['model_file'], asset['reference']['path'],
                      str((rec / 'model-audit.json').relative_to(ROOT))])
        if asset.get('armed_reference'):
            files.add(asset['armed_reference']['path'])
            prompts.append({'kind': 'armed-individual-guide-only', **read(rec / 'armed-v1-approval.json')})
        rows.append({'id': id, 'title': asset['title'], 'model_file': audit['model_file'],
                     'sha256': digest(model), 'triangles': audit['triangles'],
                     'reference': asset['reference'], 'generation_model': 'P1-20260311',
                     'generation_task': audit['source_task_id'], 'conversion_task': audit['convert_task_id'],
                     'credits_consumed': audit['credits_consumed'],
                     'generation_original': audit.get('generation_raw_model_file', audit.get('raw_model_file')),
                     'import': audit['import_review'], 'review': audit['visual_review'],
                     'geometry_repair': audit.get('geometry_repair')})
    art = ROOT / 'art/enemies/infantry'
    (art / 'prompts.json').write_text(json.dumps({'tool': 'built-in image_gen', 'images': prompts}, indent=2) + '\n')
    (art / 'output-manifest.json').write_text(json.dumps({'title': collection['title'],
        'contract': collection['contract'], 'models': rows, 'models_count': len(rows),
        'new_tripo_credits': sum(row['credits_consumed'] for row in rows),
        'character_rigging_animation_equipping_or_seating': False}, indent=2) + '\n')
    subprocess.run([sys.executable, str(ROOT / 'tools/render_prop_review.py'), *ids,
                    '--edition', 'infantry-weapons-final'], cwd=ROOT, check=True)
    sheet = Image.new('RGB', (1536, 1032), '#f3f0e8')
    draw = ImageDraw.Draw(sheet)
    font = ROOT / 'assets/fonts/human-resources-v3/ttf/HR-Beam-UI-SemiBold.ttf'
    title = ImageFont.truetype(str(font), 32)
    label = ImageFont.truetype(str(font), 19)
    small = ImageFont.truetype(str(font), 15)
    draw.text((28, 21), 'INFANTRY + JEEP WEAPONS / ACTUAL 3D MODELS', font=title, fill='#20334a')
    draw.text((28, 69), 'Unarmed T-pose bodies / separate weapons / rigging and placement come later', font=small, fill='#50645f')
    order = [ids[0], ids[1], ids[2], 'jeep-minigun-head', 'weapon-carbine',
             'weapon-shotgun', 'weapon-rocket-launcher', 'jeep-minigun-mount']
    for i, id in enumerate(order):
        x, y = 20 + (i % 4) * 380, 114 + (i // 4) * 450
        image = Image.open(ROOT / 'assets/production/records' / id / 'review-infantry-weapons-final-three-quarter.png').convert('RGB')
        image.thumbnail((354, 354))
        sheet.paste(image, (x + (354-image.width)//2, y))
        draw.text((x + 5, y + 365), assets[id]['title'], font=label, fill='#20334a')
        row = next(r for r in rows if r['id'] == id)
        draw.text((x + 5, y + 398), f"{row['triangles']:,} triangles / reviewed GLB", font=small, fill='#50645f')
    preview = ROOT / 'assets/production/reviews/infantry-weapons-final.png'
    sheet.save(preview)
    files.update(['art/enemies/infantry/prompts.json', 'art/enemies/infantry/output-manifest.json',
                  'assets/production/reviews/infantry-weapons-final.png',
                  'assets/production/weapon-sockets.json', 'assets/production/infantry-weapons-plan.txt'])
    files.add('assets/production/records/jeep-minigun-head/component-fit.json')
    readme = ('Eight independent reviewed models: three unarmed true T-pose infantry bodies, '
              'three separate handheld weapons, rotary gun head and separate pedestal.\n'
              'Reuse the rifleman body for later jeep crew. Rigging, equipping, carrying/seated '
              'poses, animation and runtime firing are later work.\n'
              'Characters use metres, +Y up and +Z front. Model audits record exact origins. '
              'Weapon sockets record measured grips/muzzles. The launcher is a loaded static '
              'source with an integral coral nose. Fine paint detail is softer in Tripo output '
              'than the approved source images.\n'
              'The minigun component-fit record describes the checked neutral fit only; '
              'pitch movement requires later rigging review.\n'
              'Armed lineup/individual images are style guides only. Every character submitted '
              'to Tripo used the separate unarmed T-pose reference. Model generation: '
              'P1-20260311, textures enabled, PBR disabled, GLTF/1024 conversion.\n'
              'Original Tripo downloads, task IDs and any controlled geometry repair provenance '
              'are preserved in the workspace production records.\n')
    (art / 'README.txt').write_text(readme)
    files.add('art/enemies/infantry/README.txt')
    package = ROOT / 'assets/packages/infantry-weapons-v1.zip'
    package.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(package, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(files):
            archive.write(ROOT / name, name)
    print(json.dumps({'models': len(rows), 'package': str(package.relative_to(ROOT)),
                      'bytes': package.stat().st_size, 'sha256': digest(package),
                      'review': str(preview.relative_to(ROOT))}))


if __name__ == '__main__':
    main()
