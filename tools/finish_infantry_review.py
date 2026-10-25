"""Label, encode and source-check the 18-second equipped-infantry review.

Use the existing offline art Python (Pillow). --encode-only uses already rendered
frames; otherwise Blender is invoked in bounded chunks. Source meshes and native
runtime contracts are never modified. Fit stills retain their unlabelled PNGs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/animation/infantry'
FPS, COUNT, DIMENSIONS = 30, 540, (1280, 720)
FONTS = ROOT/'assets/fonts/human-resources-v3/ttf'
NAMES = {'infantry-rifleman-body': 'RIFLEMAN', 'infantry-shotgunner-body': 'SHOTGUNNER',
         'infantry-rocketeer-body': 'ROCKET TROOPER'}
CLIPS = {'idle': 'EQUIPPED IDLE', 'walk': 'WALK / THREE CYCLES', 'aim_enter': 'SHOULDER / READY',
         'aim': 'TWO-HAND AIM', 'fire': 'FIRE / THREE SHOTS', 'aim_exit': 'LOWER / RECOVER',
         'beam_lift': 'CAPTURE / LIFT', 'beam_struggle': 'CAPTURE / STRUGGLE',
         'beam_release': 'RELEASE / LAND'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def font(name, size):
    return ImageFont.truetype(str(FONTS/name), size)


def annotate(image, meta, frame=None):
    image = image.convert('RGBA')
    overlay = Image.new('RGBA', image.size)
    draw = ImageDraw.Draw(overlay)
    navy, cream, mint = (16, 29, 45), (255, 241, 214), (139, 235, 194)
    for y in range(88):
        draw.line((0, y, 1280, y), fill=(*navy, int(230*(1-y/88))))
    for y in range(640, 720):
        draw.line((0, y, 1280, y), fill=(*navy, int(220*(y-640)/80)))
    draw.text((28, 15), 'HUMAN RESOURCES', font=font('HR-Abduction-Logo-Black-Italic.ttf', 32), fill=cream)
    draw.text((1010, 25), 'NORMAL SPEED / 30 FPS' if frame else 'EXPORTED GLB / FIT STILL',
              font=font('HR-Beam-UI-SemiBold.ttf', 17), fill=mint)
    draw.text((30, 58), CLIPS.get(meta['clip'], meta['clip'].replace('_', ' ').upper()),
              font=font('HR-Beam-UI-SemiBold.ttf', 19), fill=cream)
    face = font('HR-Beam-UI-SemiBold.ttf', 22)
    for actor in meta['actors']:
        text = NAMES.get(actor['id'], actor['id'])
        width = draw.textbbox((0, 0), text, font=face)[2]
        x = max(width/2+16, min(1264-width/2, actor['label_x']))
        draw.text((x-width/2, 657), text, font=face, fill=cream)
    draw.text((30, 691), 'Original body + weapon sources / physical scale / offline authoring review',
              font=font('HR-Beam-UI-Medium.ttf', 15), fill=mint)
    if frame:
        draw.line((30, 716, 1250, 716), fill=(64, 89, 102), width=3)
        draw.line((30, 716, 30+1220*frame/COUNT, 716), fill=(242, 135, 119), width=3)
    return Image.alpha_composite(image, overlay).convert('RGB')


def render(blender, output, chunk):
    for start in range(1, COUNT+1, chunk):
        end = min(COUNT, start+chunk-1)
        command = [str(blender), '--background', '--factory-startup', '--python-exit-code', '1',
                   '--python', str(ROOT/'tools/render_infantry_review.py'), '--',
                   '--start', str(start), '--end', str(end), '--output', str(output)]
        print(f'Rendering infantry frames {start}–{end}/{COUNT}', flush=True)
        with (output/f'review-render-{start:05d}-{end:05d}.log').open('w') as log:
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)


def validate_inputs(output):
    fingerprint = None
    metadata = []
    for frame in range(1, COUNT+1):
        stem = output/'review-frames'/f'{frame:05d}'
        meta = json.loads(stem.with_suffix('.json').read_text())
        if meta['frame'] != frame or abs(meta['seconds']-(frame-1)/FPS) > 1e-7:
            raise ValueError('Wrong frame timing: '+str(stem))
        if meta['unblended_still']:
            raise ValueError('A fit still was substituted for film footage')
        if fingerprint is None:
            fingerprint = meta['source_fingerprint']
        if meta['source_fingerprint'] != fingerprint:
            raise ValueError('Mixed source/renderer versions in review frames')
        if len(meta['actors']) != 3:
            raise ValueError('Film must contain all three infantry variants')
        with Image.open(stem.with_suffix('.png')) as image:
            if image.size != DIMENSIONS:
                raise ValueError('Wrong frame dimensions: '+str(stem))
            image.verify()
        metadata.append(meta)
    provenance = json.loads((output/'review-scenes'/(fingerprint+'.json')).read_text())
    if hashlib.sha256(json.dumps(provenance, sort_keys=True).encode()).hexdigest() != fingerprint:
        raise ValueError('Render provenance fingerprint mismatch')
    if digest(ROOT/'tools/render_infantry_review.py') != provenance['renderer_sha256']:
        raise ValueError('Renderer changed since the frames were produced')
    for name, expected in provenance['sources'].items():
        if digest(ROOT/name) != expected:
            raise ValueError('Review source changed: '+name)
    return metadata, fingerprint, provenance


def encode(output, metadata):
    path = output/'infantry-equipped-review.mp4'
    command = ['ffmpeg', '-hide_banner', '-loglevel', 'warning', '-y', '-f', 'rawvideo',
               '-pix_fmt', 'rgb24', '-s', '1280x720', '-r', str(FPS), '-i', '-', '-an',
               '-c:v', 'libx264', '-preset', 'medium', '-crf', '18', '-pix_fmt', 'yuv420p',
               '-movflags', '+faststart', str(path)]
    process = subprocess.Popen(command, stdin=subprocess.PIPE)
    try:
        for frame, meta in enumerate(metadata, 1):
            with Image.open(output/'review-frames'/f'{frame:05d}.png') as source:
                image = annotate(source, meta, frame)
            process.stdin.write(image.tobytes())
            if frame == 235:
                image.save(output/'infantry-equipped-review-poster.jpg', quality=95)
    except BaseException:
        process.stdin.close()
        process.terminate()
        process.wait()
        raise
    process.stdin.close()
    if process.wait() != 0:
        raise RuntimeError('Infantry review encoder failed')
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUT)
    parser.add_argument('--blender', type=Path, default=ROOT/'build/tools/blender/Blender.app/Contents/MacOS/Blender')
    parser.add_argument('--encode-only', action='store_true')
    parser.add_argument('--chunk', type=int, default=180)
    args = parser.parse_args()
    if not 1 <= args.chunk <= COUNT:
        parser.error('--chunk must be within 1..540')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if not args.encode_only:
        render(args.blender.resolve(), output, args.chunk)
    metadata, fingerprint, provenance = validate_inputs(output)
    film = encode(output, metadata)
    probe = json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-count_frames', '-select_streams', 'v:0',
        '-show_entries', 'stream=codec_name,width,height,r_frame_rate,nb_read_frames,pix_fmt',
        '-show_entries', 'format=duration', '-of', 'json', str(film)], text=True))
    stream = probe['streams'][0]
    if ((stream['width'], stream['height']) != DIMENSIONS or stream['r_frame_rate'] != '30/1'
            or int(stream['nb_read_frames']) != COUNT or stream['pix_fmt'] != 'yuv420p'
            or abs(float(probe['format']['duration'])-COUNT/FPS) > 1/FPS):
        raise ValueError('Encoded video dimensions, frame count or playback timing mismatch')
    for source in sorted((output/'fit-stills').glob('*.png')):
        if source.stem.endswith('-labelled'):
            continue
        meta = json.loads(source.with_suffix('.json').read_text())
        with Image.open(source) as image:
            annotate(image, meta).save(source.with_name(source.stem+'-labelled.jpg'), quality=95)
    report = {'status': 'passed', 'video': str(film.relative_to(ROOT)), 'sha256': digest(film),
              'source_fingerprint': fingerprint, 'sources': provenance['sources'],
              'producer_sha256': digest(Path(__file__)), 'probe': probe,
              'timeline': provenance['timeline'], 'actors': provenance['actors'],
              'notes': '18 seconds at normal source playback speed. Exported body GLBs and raw source weapons via animated socket delta. Walk boundaries use recorded0.18s local TRS snapshot crossfades. No native or artistic acceptance implied.'}
    (output/'review-film-validation.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
