"""Encode the validated close-up/shared-world walk review and package its rigs."""
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile
from PIL import Image, ImageDraw
from capture_test_field import annotate, font

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/animation/walk-v3'
CHAPTERS = [('humans','SHARED HUMAN WALK','FARMER / RESIDENT / SOLDIER'),
            ('animals','SHARED ANIMAL WALK','COW / PIG / SHEEP'),
            ('infantry','SHARED HUMAN GAIT / EQUIPPED','RIFLEMAN / SHOTGUNNER / ROCKETEER'),
            ('world','TEST AREA','ALL NINE CHARACTERS / JEEP + TANK')]


def main():
    movie = OUT/'human-resources-improved-walks.mp4'
    command = ['ffmpeg','-hide_banner','-loglevel','warning','-y','-f','rawvideo',
               '-pix_fmt','rgb24','-s','1280x720','-r','30','-i','-','-an',
               '-c:v','libx264','-preset','medium','-crf','18','-pix_fmt','yuv420p',
               '-movflags','+faststart',str(movie)]
    # Check every expected input before opening the encoder or replacing a film.
    paths = [(group,frame,OUT/group/('frames' if group=='world' else '')/f'{frame:05d}.png')
             for group,_,_ in CHAPTERS for frame in range(1,181)]
    for _,_,path in paths:
        if not path.is_file():raise FileNotFoundError(path)
    with subprocess.Popen(command,stdin=subprocess.PIPE) as process:
        try:
            for chapter,(group,title,subtitle) in enumerate(CHAPTERS):
                for frame in range(1,181):
                    path = OUT/group/('frames' if group=='world' else '')/f'{frame:05d}.png'
                    with Image.open(path) as source:
                        if source.size!=(1280,720):raise ValueError('Invalid capture dimensions')
                        if group=='world':
                            meta = json.loads(path.with_suffix('.json').read_text())
                            image = annotate(source,meta,chapter*180+frame,720)
                        else:
                            image = source.convert('RGB')
                            draw = ImageDraw.Draw(image)
                            draw.rectangle((0,0,1280,87),fill=(16,29,45))
                            draw.text((28,17),title,font=font('HR-Abduction-Logo-Black-Italic.ttf',35),fill=(255,241,214))
                            draw.rectangle((0,654,1280,720),fill=(16,29,45))
                            draw.text((28,671),subtitle,font=font('HR-Beam-UI-SemiBold.ttf',22),fill=(139,235,194))
                            draw.text((970,675),'SIDE / THREE-QUARTER',font=font('HR-Beam-UI-SemiBold.ttf',16),fill=(255,241,214))
                    process.stdin.write(image.tobytes())
                    if frame in [1,90,175]:image.save(OUT/f'review-{group}-{frame:03d}.jpg',quality=95)
        finally:process.stdin.close()
        if process.wait()!=0:raise RuntimeError('Walk review encoder failed')
    probe = json.loads(subprocess.check_output(['ffprobe','-v','error','-count_frames',
        '-show_entries','stream=codec_name,width,height,r_frame_rate,nb_read_frames',
        '-show_entries','format=duration','-of','json',str(movie)],text=True))
    s = probe['streams'][0]
    if (s['width'],s['height'],s['r_frame_rate'],int(s['nb_read_frames']))!=(1280,720,'30/1',720):
        raise ValueError('Encoded walk review properties differ from capture')
    if abs(float(probe['format']['duration'])-24)>.001:raise ValueError('Invalid movie duration')
    exports = sorted((ROOT/'build/animation/exports').glob('*.glb'))
    infantry = sorted((ROOT/'build/animation/infantry/exports').glob('*.glb'))
    report = {'status':'passed','probe':probe,'sha256':hashlib.sha256(movie.read_bytes()).hexdigest(),
              'export_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in exports+infantry},
              'chapters':[{'name':g,'start_seconds':i*6,'seconds':6} for i,(g,_,_) in enumerate(CHAPTERS)],
              'producer_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (OUT/'film-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    files = exports+sorted((ROOT/'build/animation/blend').glob('*.blend'))
    files += infantry+sorted((ROOT/'build/animation/infantry/blend').glob('*.blend'))
    files += sorted((ROOT/'build/animation/infantry/reports').glob('*.json'))
    files += [ROOT/'build/animation/reports'/(p.stem+'.json') for p in exports]
    files += [ROOT/'build/animation/reports/validation.json']
    files += [movie,OUT/'film-validation.json',OUT/'walk-validation.json',
              ROOT/'docs/SHARED_WALKS.md',ROOT/'assets/animation/walk-profiles.json',
              ROOT/'assets/animation/characters.json',ROOT/'assets/animation/infantry.json',
              ROOT/'assets/animation/infantry-fit.json',ROOT/'docs/INFANTRY_ANIMATION_HANDOFF.md',
              OUT/'world/scene-report.json']
    files += [OUT/group/'framing-validation.json' for group,_,_ in CHAPTERS if group!='world']
    with zipfile.ZipFile(OUT/'human-resources-improved-walks.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for path in files:archive.write(path,path.relative_to(ROOT))
    print('Walk review complete:',movie)


if __name__=='__main__':main()
