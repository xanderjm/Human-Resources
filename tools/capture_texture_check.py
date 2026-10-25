"""Capture three native Metal/SDL_GPU views using the current canonical tile masters."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/captures/seamless-material-check'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    content = ROOT / 'build/style-preview/content'
    subprocess.run([sys.executable, 'tools/cook_style_preview.py', '--output', str(content)], cwd=ROOT, check=True)
    subprocess.run(['cmake', '--build', '--preset', 'dev', '--target', 'human_resources_style_preview'], cwd=ROOT, check=True)
    executable = ROOT / 'build/dev/human_resources_style_preview'
    report = {'scope': 'Native desktop authoring texture review; not gameplay or device qualification',
              'producer_sha256': digest(executable),
              'content_manifest_sha256': digest(content / 'source-manifest.json'), 'captures': []}
    for location in ('farmland', 'suburbia', 'desert'):
        raw = OUT / (location + '.rgba')
        log = OUT / (location + '.log')
        with log.open('w') as stream:
            subprocess.run([str(executable), '--content', str(content), '--capture', str(raw),
                '--frames', '1', '--fps', '30', '--width', '1280', '--height', '720',
                '--scenery', location, '--time', '37', '--seed', '514'], cwd=ROOT,
                stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=60)
        if raw.stat().st_size != 1280 * 720 * 4:
            raise ValueError('Native capture dimensions do not match RGBA byte count')
        output = OUT / (location + '.png')
        Image.frombytes('RGBA', (1280, 720), raw.read_bytes()).save(output)
        report['captures'].append({'location': location, 'png': str(output.relative_to(ROOT)),
            'sha256': digest(output), 'native_log': log.read_text()})
    (OUT / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Native captures saved to {OUT}')


if __name__ == '__main__':
    main()
