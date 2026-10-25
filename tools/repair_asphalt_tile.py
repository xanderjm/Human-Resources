"""Asphalt-only periodic patch quilting, preserving all other tile masters.

The former two-edge numeric projection left a visible broad stroke termination.
Replace its entire Y-wrap neighbourhood with a real interior painted strip,
joined on two closed minimum-error paths rather than a straight feather band.
The wrap itself then crosses natural brushwork. Raw source and pre-quilt master
remain untouched, and final exact edge projection only changes two texel rows.
"""
import argparse
import json
import shutil
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from seamless_textures import ROOT, digest, integer_boundary_projection, make_preview, statistics, write_audit

DIR=ROOT/'assets/textures/texture-asphalt'
RECORD=ROOT/'assets/production/records/texture-asphalt'


def closed_cut(cost):
    """Minimum RGB-error path around the periodic horizontal cylinder."""
    height,width=cost.shape
    anchor=int(np.argmin(cost.mean(axis=1)))
    best=None
    for start in sorted(set(max(0,min(height-1,anchor+d)) for d in (-12,0,12))):
        previous=np.full(height,np.inf);previous[start]=cost[start,0]
        back=np.zeros((width,height),dtype=np.int16)
        for x in range(1,width):
            choices=np.stack([np.roll(previous,d)+abs(d)*.2 for d in (-2,-1,0,1,2)])
            choices[0,-2:]=np.inf;choices[1,-1:]=np.inf
            choices[3,:1]=np.inf;choices[4,:2]=np.inf
            choice=np.argmin(choices,axis=0)
            previous=choices[choice,np.arange(height)]+cost[:,x]
            back[x]=np.arange(height)-np.array([-2,-1,0,1,2])[choice]
        score=previous[start]
        path=np.empty(width,dtype=np.int16);path[-1]=start
        for x in range(width-1,0,-1):path[x-1]=back[x,path[x]]
        # Flat endpoint neighbourhood keeps blend weights periodic in X too.
        path[:8]=path[-8:]=start
        if best is None or score<best[0]:best=(float(score),path)
    return best


def quilt(base):
    # Roll Y only: old wrapping stroke termination is now in the image centre.
    shifted=np.roll(base.astype(float),512,axis=0)
    best=None
    for centre in (192,320,448,576,704,832):
        donor=base[(np.arange(1024)-512+centre)%1024].astype(float)
        error=np.mean((shifted-donor)**2,axis=2)
        score_top,top=closed_cut(error[336:432])
        score_bottom,bottom=closed_cut(error[592:688])
        score=score_top+score_bottom
        if best is None or score<best[0]:best=(score,centre,donor,top+336,bottom+592)
    score,centre,donor,top,bottom=best
    yy=np.arange(1024)[:,None]
    def smooth(t):
        t=np.clip(t,0,1);return t*t*(3-2*t)
    alpha=smooth((yy-top[None,:]+14)/28)*smooth((bottom[None,:]-yy+14)/28)
    merged=shifted*(1-alpha[...,None])+donor*alpha[...,None]
    final=integer_boundary_projection(np.roll(merged,-512,axis=0))
    return final,{'donor_centre_row':int(centre),'top_cut_range':top[[np.argmin(top),np.argmax(top)]].tolist(),
        'bottom_cut_range':bottom[[np.argmin(bottom),np.argmax(bottom)]].tolist(),
        'feather_width_px':28,'closed_minimum_error_score':score,
        'method':'Periodic full-width interior brush-strip quilt; two closed minimum-error seams; local28px smooth feather; final2-texel exact periodic projection. No mirroring or whole-image contrast change.'}


def overview():
    canvas=Image.new('RGB',(1536,856),'#efecdf');draw=ImageDraw.Draw(canvas)
    for i,name in enumerate(('grass','dirt','soil','asphalt','pavement','sand','wood')):
        x=(i%4)*384;y=(i//4)*428
        path=ROOT/'assets/textures'/('texture-'+name)/'tiled-3x3-qa.png'
        draw.text((x+10,y+8),name.upper()+' | 3x3 repeat',fill='#28343b')
        tile=Image.open(path).convert('RGB').resize((384,384),Image.Resampling.LANCZOS)
        canvas.paste(tile,(x,y+30))
    draw.multiline_text((1160,480),'7 materials /1024x1024 RGB\nExact edge value+slope match\nPainterly albedo /repeat sampler\nAsphalt brush-wrap quilt repaired\nNative device review remains open',fill='#28343b',spacing=4)
    canvas.save(ROOT/'assets/textures/texture-repeat-overview.png')


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--accept',action='store_true');args=parser.parse_args()
    previous=DIR/'pre-quilt-master-v2.png'
    if not previous.exists():shutil.copy2(DIR/'texture-asphalt.png',previous)
    base=np.asarray(Image.open(previous).convert('RGB'))
    final,method=quilt(base)
    candidate=DIR/'quilt-candidate.png';Image.fromarray(final).save(candidate)
    make_preview(Image.fromarray(final),DIR/'quilt-candidate-3x3.png',512)
    # Boundary at centre: inspect native texels on either side of a genuine wrap.
    Image.fromarray(np.roll(final,512,axis=0)[320:704]).save(DIR/'quilt-wrap-neighbourhood.png')
    if args.accept:
        output=DIR/'texture-asphalt.png';shutil.copy2(candidate,output)
        preview=DIR/'tiled-3x3-qa.png';close=DIR/'tiled-3x3-close-qa.png'
        make_preview(Image.fromarray(final),preview,256);make_preview(Image.fromarray(final),close,512)
        metadata=json.loads((RECORD/'tile-v1-generated.json').read_text());raw=DIR/'raw-v1.png'
        source=np.asarray(Image.open(raw).convert('RGB').resize((1024,1024),Image.Resampling.LANCZOS)).astype(float)
        audit=write_audit('texture-asphalt',metadata,raw,output,preview,close,source,final)
        audit['method']=method['method'];audit['asphalt_quilt_override']={**method,'script':'tools/repair_asphalt_tile.py',
            'input_master':str(previous.relative_to(ROOT)),'input_sha256':digest(previous),
            'integrated_in':'tools/seamless_textures.py'}
        (RECORD/'texture-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
        overview()
    report={'candidate_sha256':digest(candidate),'previous_sha256':digest(previous),'method':method,
        'old_seams':statistics(base),'new_seams':statistics(final),
        'old_mean_row_gradient_near_wrap':float(np.mean(base[1].astype(float)-base[0].astype(float))),
        'new_mean_row_gradient_near_wrap':float(np.mean(final[1].astype(float)-final[0].astype(float)))}
    (RECORD/'asphalt-quilt-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))

if __name__=='__main__':main()
