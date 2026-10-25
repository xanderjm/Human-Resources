"""Record actual native SDL_GPU frames and encode a reproducible development film.

Run with the existing art Python (Pillow/NumPy) to cook inputs. FFmpeg is a
host-only capture utility, never a runtime dependency or package input.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def capture(args):
    output=args.output.resolve();output.parent.mkdir(parents=True,exist_ok=True)
    if not args.skip_cook:
        subprocess.run([sys.executable, str(ROOT/'tools/cook_style_preview.py'), '--output', str(ROOT/'build/style-preview/content')],check=True,cwd=ROOT)
    if not args.skip_build:
        subprocess.run(['cmake','--preset','dev'],check=True,cwd=ROOT)
        subprocess.run(['cmake','--build','--preset','dev','--target','human_resources_style_preview'],check=True,cwd=ROOT)
    executable=ROOT/'build/dev'/('human_resources_style_preview.exe' if os.name=='nt' else 'human_resources_style_preview')
    width,height=(780,1688) if args.phone else (1280,720) if args.landscape else (720,1280)
    producer_hash=hashlib.sha256(executable.read_bytes()).hexdigest()
    frames=args.seconds*args.fps
    log=output.with_suffix('.native.log')
    temporary_output=output.with_name(output.stem+'.candidate.mp4')
    with tempfile.TemporaryDirectory(prefix='hr-capture-',dir=output.parent) as temp:
        raw=Path(temp)/'native.rgba'
        fifo=hasattr(os,'mkfifo')
        if fifo:os.mkfifo(raw)
        encode=['ffmpeg','-hide_banner','-loglevel','error','-y','-f','rawvideo','-pixel_format','rgba','-video_size',f'{width}x{height}','-framerate',str(args.fps),'-i',str(raw),'-an','-c:v','libx264','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(temporary_output)]
        encoder=subprocess.Popen(encode) if fifo else None
        try:
            with log.open('w') as stream:
                subprocess.run([str(executable),*(['--scenery',args.scenery,'--seed',str(args.seed)] if args.scenery else []),'--content',str(ROOT/'build/style-preview/content'),'--capture',str(raw),'--frames',str(frames),'--width',str(width),'--height',str(height),'--fps',str(args.fps),'--time',str(args.start)],cwd=ROOT,env={**os.environ,'SDL_ASSERT':'abort'},stdout=stream,stderr=subprocess.STDOUT,check=True,timeout=1800)
            if encoder:
                if encoder.wait(timeout=60):raise RuntimeError('FFmpeg failed')
            else:subprocess.run(encode,check=True)
        finally:
            if encoder and encoder.poll() is None:
                encoder.terminate();encoder.wait(timeout=10)
    # Verify decoded frame count, format and duration before publishing the file.
    info=json.loads(subprocess.check_output(['ffprobe','-v','error','-count_frames','-show_entries','stream=width,height,r_frame_rate,nb_read_frames,pix_fmt:format=duration','-of','json',str(temporary_output)],text=True))
    stream=info['streams'][0]
    if int(stream['nb_read_frames'])!=frames or stream['r_frame_rate']!=f'{args.fps}/1' or int(stream['width'])!=width or int(stream['height'])!=height:
        raise RuntimeError(f'Capture does not match the requested film: {info}')
    if hashlib.sha256(executable.read_bytes()).hexdigest()!=producer_hash:
        raise RuntimeError('Preview executable changed during recording; capture was not published')
    temporary_output.replace(output)
    evidence={'scenery':args.scenery,'seed':args.seed,'start_seconds':args.start,'scroll_speed_mps':2.4,'producer_sha256':producer_hash,'source':'native SDL_GPU offscreen color target, GPU readback; no generated/interpolated frames','video':str(output.relative_to(ROOT)) if output.is_relative_to(ROOT) else str(output),'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'probe':info,'native_log':log.read_text(),'content_manifest_sha256':hashlib.sha256((ROOT/'build/style-preview/content/source-manifest.json').read_bytes()).hexdigest()}
    output.with_suffix('.json').write_text(json.dumps(evidence,indent=2)+'\n')
    samples=[(args.scenery+'-'+stage,t) for stage,t in [('early',4),('middle',14),('late',24)]] if args.scenery else [('farmland',4),('suburbia',14),('desert',24)]
    for n,t in samples:
        if t<args.seconds:
            subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-ss',str(t),'-i',str(output),'-frames:v','1',str(output.parent/(output.stem+'-'+n+'.png'))],check=True)
    print(f'Native film verified: {output} ({frames} frames, {width}x{height}, {args.fps} fps)')


def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=None);p.add_argument('--landscape',action='store_true');p.add_argument('--phone',action='store_true');p.add_argument('--fps',type=int,choices=[30,60],default=30);p.add_argument('--scenery',choices=['farmland','suburbia','desert']);p.add_argument('--seed',type=int,default=514);p.add_argument('--start',type=int,default=0);p.add_argument('--seconds',type=int,default=30);p.add_argument('--skip-build',action='store_true');p.add_argument('--skip-cook',action='store_true');args=p.parse_args()
    if args.output is None:args.output=ROOT/'build/captures'/(f'scenery-{args.scenery}-seed-{args.seed}.mp4' if args.scenery else 'human-resources-style-study-01.mp4')
    if not 0<=args.seed<=4294967295 or not 0<=args.start<=4294967295:p.error('seed/start must fit an unsigned 32-bit integer')
    if not 1<=args.seconds<=120:p.error('--seconds must be between 1 and 120')
    for tool in ['ffmpeg','ffprobe']:
        if not shutil.which(tool):p.error(f'{tool} is required for host capture')
    capture(args)
if __name__=='__main__':main()
