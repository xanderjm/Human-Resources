"""Render resumable test-field chapters, encode, and verify the local film."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/animation/test-field'
FONTS = ROOT/'assets/fonts/human-resources-v3/ttf'
sys.path.insert(0,str(ROOT/'tools/animation'))
from test_field import validate_routes


def font(name,size):
    return ImageFont.truetype(str(FONTS/name),size)


def annotate(image,meta,frame,count):
    image = image.convert('RGBA')
    over = Image.new('RGBA',image.size)
    draw = ImageDraw.Draw(over)
    for y in range(105):
        draw.line((0,y,1280,y),fill=(16,29,45,int(230*(1-y/105))))
    for y in range(650,720):
        draw.line((0,y,1280,y),fill=(16,29,45,int(210*(y-650)/70)))
    cream,mint = (255,241,214),(139,235,194)
    draw.text((28,19),'HUMAN RESOURCES',font=font('HR-Abduction-Logo-Black-Italic.ttf',35),fill=cream)
    draw.text((1050,29),'TEST FIELD',font=font('HR-Beam-UI-SemiBold.ttf',20),fill=mint)
    draw.text((30,672),meta['chapter'],font=font('HR-Beam-UI-SemiBold.ttf',21),fill=cream)
    draw.line((30,710,1250,710),fill=(64,89,102),width=3)
    draw.line((30,710,30+1220*frame/count,710),fill=(242,135,119),width=3)
    names = {'farmer-body':'FARMER','resident-body':'RESIDENT','soldier-body':'SOLDIER',
             'infantry-rifleman-body':'RIFLEMAN','infantry-shotgunner-body':'SHOTGUNNER',
             'infantry-rocketeer-body':'ROCKETEER'}
    face = font('HR-Beam-UI-SemiBold.ttf',16)
    for label in meta['labels']:
        x,y = label['position'];text = names.get(label['id'],label['id'].upper())
        if not (18<x<1262 and 99<y<644):
            continue
        box = draw.textbbox((0,0),text,font=face)
        width = box[2]-box[0]
        draw.rounded_rectangle((x-width/2-6,y-4,x+width/2+6,y+20),radius=4,fill=(16,29,45,190))
        draw.text((x-width/2,y-1),text,font=face,fill=cream)
    return Image.alpha_composite(image,over).convert('RGB')


def render(blender,count):
    # Short processes avoid losing an entire capture to a host process limit.
    for start in range(1,count+1,300):
        end = min(count,start+299)
        log = OUT/f'render-{start:05d}-{end:05d}.log'
        command = [str(blender),'--background','--factory-startup','--python-exit-code','1',
                   '--python',str(ROOT/'tools/render_test_field.py'),'--',
                   '--start',str(start),'--end',str(end)]
        print(f'Rendering frames {start}–{end}/{count}',flush=True)
        with log.open('w') as stream:
            subprocess.run(command,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,check=True)


def encode(count):
    output = OUT/'human-resources-test-field.mp4'
    command = ['ffmpeg','-hide_banner','-loglevel','warning','-y','-f','rawvideo','-pix_fmt','rgb24',
               '-s','1280x720','-r','30','-i','-','-an','-c:v','libx264','-preset','medium',
               '-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(output)]
    process = subprocess.Popen(command,stdin=subprocess.PIPE)
    try:
        for frame in range(1,count+1):
            path = OUT/'frames'/f'{frame:05d}'
            meta = json.loads(path.with_suffix('.json').read_text())
            with Image.open(path.with_suffix('.png')) as im:
                if im.size != (1280,720):
                    raise ValueError('Unexpected rendered dimensions')
                image = annotate(im,meta,frame,count)
            process.stdin.write(image.tobytes())
            if frame in [1,180,450,750]:
                image.save(OUT/f'review-{frame:05d}.jpg',quality=95)
    finally:
        process.stdin.close()
    if process.wait()!=0:
        raise RuntimeError('Film encoder failed')
    return output


def verify(output,spec,route_result):
    command = ['ffprobe','-v','error','-count_frames','-show_entries',
               'stream=codec_name,width,height,r_frame_rate,nb_read_frames',
               '-show_entries','format=duration','-of','json',str(output)]
    probe = json.loads(subprocess.check_output(command,text=True))
    stream = probe['streams'][0]
    expected = round(spec['duration_seconds']*spec['fps'])
    if int(stream['nb_read_frames'])!=expected or (stream['width'],stream['height'])!=(1280,720):
        raise ValueError('Encoded frame count/dimensions mismatch')
    if stream['r_frame_rate']!='30/1' or abs(float(probe['format']['duration'])-30)>1/30:
        raise ValueError('Encoded timing mismatch')
    report = {'status':'passed','video':str(output.relative_to(ROOT)),
              'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'probe':probe,
              'routes':route_result,'characters':6,'vehicles':['jeep','tank'],
              'producer_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (OUT/'film-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2),flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--blender',type=Path,default=ROOT/'build/tools/blender/Blender.app/Contents/MacOS/Blender')
    parser.add_argument('--encode-only',action='store_true')
    args = parser.parse_args()
    spec = json.loads((ROOT/'assets/animation/test-field.json').read_text())
    chars = json.loads((ROOT/'assets/animation/characters.json').read_text())['characters']
    routes = validate_routes(spec,{c['id']:c for c in chars})
    OUT.mkdir(parents=True,exist_ok=True)
    count = round(spec['duration_seconds']*spec['fps'])
    if not args.encode_only:
        render(args.blender,count)
    output = encode(count)
    verify(output,spec,routes)


if __name__=='__main__':
    main()
