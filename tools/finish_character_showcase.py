"""Apply approved typography, encode the film, and package the animation handoff."""
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import zipfile
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from normalize_glb import unpack
from validate_character_animation import sampled_world

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/animation'
sys.path.insert(0, str(ROOT/'tools/animation'))
from showcase_timeline import timeline, locate

FONTS = ROOT/'assets/fonts/human-resources-v3/ttf'
NAVY = (16, 29, 45)
CREAM = (255, 241, 214)
MINT = (139, 235, 194)
CORAL = (242, 135, 119)


def font(name, size):
    return ImageFont.truetype(str(FONTS/name), size)


def centered(draw, xy, text, face, color):
    box = draw.textbbox((0,0), text, font=face)
    draw.text((xy[0]-(box[2]-box[0])/2, xy[1]), text, font=face, fill=color)


def overlay_base(image, header_height=190):
    image = image.convert('RGBA')
    over = Image.new('RGBA', image.size)
    draw = ImageDraw.Draw(over)
    for y in range(720):
        alpha = 235*(1-y/header_height) if y < header_height else (220*min(1,(y-590)/100) if y > 590 else 0)
        if alpha > 0:
            draw.line((0,y,1280,y), fill=(*NAVY, int(alpha)))
    return Image.alpha_composite(image, over)


def rig_projection(c, time, center):
    """Project the delivered GLB skeleton with the film's actual camera/display transform."""
    doc, chunks = unpack(OUT/'exports'/(c['id']+'.glb'))
    blob = next(b for t,b in chunks if t == 0x004e4942)
    animation = next(a for a in doc['animations'] if a['name'] == 'idle')
    world = sampled_world(doc, blob, animation, time)
    joints = set(doc['skins'][0]['joints'])
    parent = {child:i for i,n in enumerate(doc['nodes']) for child in n.get('children',[])}
    camera = np.array([0,-9,4.8])
    target = np.array([0,0,1.15])
    forward = (target-camera)/np.linalg.norm(target-camera)
    right = np.cross(forward,[0,0,1]);right/=np.linalg.norm(right)
    up = np.cross(right,forward)
    yaw = math.radians(32 if c['rig'] == 'quadruped' else -12)
    rot = np.array([[math.cos(yaw),-math.sin(yaw),0],
                    [math.sin(yaw),math.cos(yaw),0],[0,0,1]])
    def project(index):
        p = world[index][:3,3]
        p = np.array([p[0],-p[2],p[1]])
        p = rot@p*(1.5/c['height'])+np.array([center,0,0])
        q = p-target
        return (640+q@right*1280/7.8,360-q@up*1280/7.8)
    return [(project(i),project(parent[i])) for i in joints
            if parent.get(i) in joints and doc['nodes'][i].get('name') != 'socket_head']


def annotate(image, group, chars, shot, local, elapsed, duration):
    beam = shot['clip'].startswith('beam_')
    image = overlay_base(image, 70 if beam else 190)
    draw = ImageDraw.Draw(image)
    title = font('HR-Abduction-Logo-Black-Italic.ttf', 36)
    heading = font('HR-Abduction-Compact-Black-Italic.ttf', 42)
    body = font('HR-Beam-UI-Medium.ttf', 20)
    label = font('HR-Beam-UI-SemiBold.ttf', 24)
    draw.text((34,22), 'HUMAN RESOURCES', font=title, fill=CREAM)
    chapter = '01  /  ANIMALS' if group == 'animals' else '02  /  HUMANOIDS'
    draw.text((963,31),chapter,font=body,fill=MINT)
    clip = shot['clip']
    names = {'idle':'IDLE / RIGS', 'walk':'WANDER / PATROL', 'run':'FLEE',
             'special':'CHARACTER BEHAVIOURS', 'alert':'NOTICE THE SAUCER', 'startle':'STARTLE',
             'hit_react':'HIT REACTION', 'beam_lift':'TRACTOR BEAM / LIFT',
             'beam_struggle':'TRACTOR BEAM / STRUGGLE', 'beam_release':'RELEASE / LAND'}
    loop_text = 'LOOP' if shot['loop'] else 'ONE SHOT'
    if beam:
        draw.rounded_rectangle((24,72,450,175),radius=10,fill=(*NAVY,230))
        draw.text((34,75),'TRACTOR BEAM',font=font('HR-Abduction-Compact-Black-Italic.ttf',32),fill=CREAM)
        sub = {'beam_lift':'LIFT','beam_struggle':'STRUGGLE','beam_release':'RELEASE / LAND'}[clip]
        draw.text((34,111),sub,font=font('HR-Abduction-Compact-Black-Italic.ttf',28),fill=CREAM)
        draw.text((36,148),loop_text,font=body,fill=MINT)
    else:
        draw.text((34,78), names[clip], font=heading, fill=CREAM)
        draw.text((36,135),loop_text, font=body,fill=MINT)
    display_names = {'cow':'COW','pig':'PIG','sheep':'SHEEP','farmer-body':'FARMER',
                     'resident-body':'RESIDENT','soldier-body':'SOLDIER'}
    for c,x in zip(chars,[270,640,1010]):
        centered(draw,(x,617),display_names[c['id']],label,CREAM)
        detail = c['special'].upper() if clip == 'special' else f'{c["height"]:g} m  /  10 clips'
        centered(draw,(x,650),detail,body,MINT)
    draw.line((35,699,1245,699),fill=(67,89,105),width=3)
    draw.line((35,699,35+1210*elapsed/duration,699),fill=CORAL,width=3)
    if clip == 'idle' and local < 1.8:
        skeleton = Image.new('RGBA', image.size)
        sd = ImageDraw.Draw(skeleton)
        alpha = int(200*min(1, (1.8-local)/.4))
        for c,x in zip(chars,[-2.25,0,2.25]):
            for a,b in rig_projection(c, local, x):
                sd.line((*a,*b),fill=(*MINT,alpha),width=2)
                sd.ellipse((a[0]-3,a[1]-3,a[0]+3,a[1]+3),fill=(*CREAM,alpha))
        image = Image.alpha_composite(image,skeleton)
    return image.convert('RGB')


def title_card(spec):
    image = Image.new('RGB',(1280,720),NAVY)
    for group, y in [('animals',150),('humans',426)]:
        source = Image.open(OUT/'showcase'/group/'00001.png').convert('RGB')
        strip = source.crop((70,260,1210,595)).resize((940,276),Image.Resampling.LANCZOS)
        image.paste(strip,(170,y))
    draw = ImageDraw.Draw(image)
    centered(draw,(640,24),'HUMAN RESOURCES',font('HR-Abduction-Logo-Black-Italic.ttf',61),CREAM)
    centered(draw,(640,106),'6 CHARACTERS  /  60 ANIMATIONS',font('HR-Beam-UI-Bold.ttf',26),MINT)
    return image


def encode(spec, shots):
    destination = OUT/'human-resources-character-showcase.mp4'
    count = round(shots[-1]['end']*30)
    total = count*2+75+60
    command = ['ffmpeg','-hide_banner','-loglevel','warning','-y','-f','rawvideo',
               '-pix_fmt','rgb24','-s','1280x720','-r','30','-i','-', '-an',
               '-c:v','libx264','-preset','medium','-crf','18','-pix_fmt','yuv420p',
               '-movflags','+faststart',str(destination)]
    process = subprocess.Popen(command,stdin=subprocess.PIPE)
    poster = title_card(spec)
    poster.save(OUT/'showcase-poster.jpg',quality=95)
    try:
        for _ in range(75):
            process.stdin.write(poster.tobytes())
        for group, chars in [('animals',spec['characters'][:3]),('humans',spec['characters'][3:])]:
            for frame in range(1,count+1):
                path = OUT/'showcase'/group/f'{frame:05d}.png'
                if not path.exists():
                    raise ValueError('Missing rendered frame '+str(path))
                shot, local = locate(shots,(frame-1)/30)
                image = annotate(Image.open(path),group,chars,shot,local,(frame-1)/30,count/30)
                process.stdin.write(image.tobytes())
                # Preserve a labelled still from the middle of each actual clip.
                mid = round((shot['start']+shot['end'])/2*30)+1
                if frame == mid:
                    image.save(OUT/'showcase'/f'{group}-{shot["clip"]}.jpg',quality=94)
        end = Image.new('RGB',(1280,720),NAVY)
        draw = ImageDraw.Draw(end)
        centered(draw,(640,220),'READY FOR ABDUCTION',font('HR-Abduction-Logo-Black-Italic.ttf',61),CREAM)
        centered(draw,(640,324),'6 skinned GLBs  /  6 editable Blender files  /  60 named clips',
                 font('HR-Beam-UI-Medium.ttf',25),MINT)
        centered(draw,(640,378),'Original models and textures preserved',font('HR-Beam-UI-Medium.ttf',23),CREAM)
        for _ in range(60):
            process.stdin.write(end.tobytes())
    finally:
        process.stdin.close()
    if process.wait() != 0:
        raise RuntimeError('Film encoder failed')
    return destination,total


def write_inventory(spec):
    rows = []
    for c in spec['characters']:
        report = json.loads((OUT/'reports'/(c['id']+'.json')).read_text())
        rows.append({'id':c['id'], 'source_sha256':report['source_sha256'],
                     'export_sha256':hashlib.sha256((OUT/'exports'/(c['id']+'.glb')).read_bytes()).hexdigest(),
                     'joints':report['bones'], 'triangles':report['triangles'],
                     'clips':[a['name'] for a in report['clips']]})
    result = {'characters':rows, 'total_characters':len(rows),
              'total_clips':sum(len(r['clips']) for r in rows)}
    (OUT/'reports/inventory.json').write_text(json.dumps(result,indent=2)+'\n')


def package(spec, film):
    write_inventory(spec)
    archive = OUT/'human-resources-character-animation.zip'
    files = [*sorted((OUT/'exports').glob('*.glb')),*sorted((OUT/'blend').glob('*.blend')),
             *sorted((OUT/'reports').glob('*.json')),film,OUT/'showcase-poster.jpg']
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for path in files:
            z.write(path,str(path.relative_to(OUT)))
        for path in [ROOT/'docs/CHARACTER_ANIMATION.md',ROOT/'assets/animation/characters.json']:
            z.write(path,str(path.relative_to(ROOT)))
        for path in [ROOT/'tools/build_character_animation.py',ROOT/'tools/validate_character_animation.py',
                     ROOT/'tools/render_character_showcase.py',ROOT/'tools/finish_character_showcase.py',
                     ROOT/'tools/normalize_glb.py',ROOT/'tools/audit_glb.py',
                     *sorted((ROOT/'tools/animation').glob('*.py'))]:
            z.write(path,str(path.relative_to(ROOT)))
    return archive


def main():
    spec = json.loads((ROOT/'assets/animation/characters.json').read_text())
    shots = timeline(spec)
    film, frames = encode(spec,shots)
    archive = package(spec,film)
    manifest = {'film':str(film.relative_to(ROOT)),'frames':frames,'fps':30,
                'dimensions':[1280,720], 'duration_seconds':frames/30,
                'film_sha256':hashlib.sha256(film.read_bytes()).hexdigest(),
                'archive':str(archive.relative_to(ROOT)),
                'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
                'export_sha256':{c['id']:hashlib.sha256((OUT/'exports'/(c['id']+'.glb')).read_bytes()).hexdigest()
                                 for c in spec['characters']},
                'render_source_sha256':hashlib.sha256((ROOT/'tools/render_character_showcase.py').read_bytes()).hexdigest(),
                'notes':'Silent offline authoring showcase; actors normalized to 1.5m display height. All ten clips shown for each of six exported GLBs.'}
    (OUT/'delivery.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))


if __name__ == '__main__':
    main()
