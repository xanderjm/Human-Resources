"""Capture a native scenery seed/contact sheet at several independent stations."""
import argparse
from pathlib import Path
import subprocess
import os
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/captures/scenery-review');p.add_argument('--seed',type=int,default=514);p.add_argument('--times',type=int,nargs='+',default=[0,18,37]);p.add_argument('--landscape',action='store_true');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
w,h=(1280,720) if a.landscape else (720,1280)
thumb_w=360;thumb_h=round(h*thumb_w/w);sheet=Image.new('RGB',(1080,(thumb_h+28)*len(a.times)), '#142735');d=ImageDraw.Draw(sheet)
for row,t in enumerate(a.times):
 for col,theme in enumerate(['farmland','suburbia','desert']):
  stem=a.output/f'{theme}-seed-{a.seed}-t{t}';raw=stem.with_suffix('.rgba');log=stem.with_suffix('.log')
  with log.open('w') as out:
   subprocess.run([str(ROOT/'build/dev/human_resources_style_preview'),'--scenery',theme,'--seed',str(a.seed),'--time',str(t),'--frames','1','--capture',str(raw),'--width',str(w),'--height',str(h)],cwd=ROOT,env={**os.environ,'SDL_ASSERT':'abort'},stdout=out,stderr=subprocess.STDOUT,check=True,timeout=60)
  subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','rawvideo','-pixel_format','rgba','-video_size',f'{w}x{h}','-i',str(raw),'-frames:v','1',str(stem.with_suffix('.png'))],check=True)
  raw.unlink()
  image=Image.open(stem.with_suffix('.png')).convert('RGB');image.thumbnail((thumb_w,thumb_h));sheet.paste(image,(col*360,row*(thumb_h+28)+28));d.text((col*360+8,row*(thumb_h+28)+8),f'{theme} / seed {a.seed} / {t*2.4:.1f} m',fill='white')
sheet.save(a.output/'contact.png');print(a.output/'contact.png')
