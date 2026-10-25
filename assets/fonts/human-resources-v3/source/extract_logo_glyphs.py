#!/usr/bin/env python3
"""Recover vector face outlines from the user-supplied Human Resources logo.

This is font-source extraction, not a modification of the reference artwork.
Separates cream letter faces from the coral extrusion and blue background.
"""
import argparse
import json
import math
from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image
from shapely import affinity
from shapely.geometry import box, Polygon
from shapely.ops import unary_union


def components(mask):
    seen=np.zeros(mask.shape,dtype=bool);found=[]
    height,width=mask.shape
    for y,x in zip(*np.nonzero(mask)):
        if seen[y,x]:
            continue
        seen[y,x]=True;queue=deque([(int(x),int(y))]);pixels=[]
        while queue:
            xx,yy=queue.popleft();pixels.append((xx,yy))
            for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
                nx,ny=xx+dx,yy+dy
                if 0<=nx<width and 0<=ny<height and mask[ny,nx] and not seen[ny,nx]:
                    seen[ny,nx]=True;queue.append((nx,ny))
        if len(pixels)>180:
            found.append(pixels)
    return found


def vectorize(pixels,expansion=1):
    rows={}
    for x,y in pixels:
        rows.setdefault(y,[]).append(x)
    runs=[]
    for y,xs in rows.items():
        xs.sort();start=last=xs[0]
        for x in xs[1:]:
            if x>last+1:
                runs.append(box(start,y,last+1,y+1));start=x
            last=x
        runs.append(box(start,y,last+1,y+1))
    shape=unary_union(runs)
    x0,y0,x1,y1=shape.bounds
    height=y1-y0
    shape=affinity.affine_transform(shape,[700/height,0,0,-700/height,-x0*700/height,y1*700/height])
    # Remove the common forward lean before deriving reusable glyph spacing.
    shape=affinity.affine_transform(shape,[1,-.28,0,1,0,0])
    shape=affinity.scale(shape,xfact=expansion,yfact=1,origin=(0,0))
    shape=affinity.translate(shape,xoff=-shape.bounds[0])
    # Remove pixel steps, then round only small contour changes. Significant
    # logo corners (the H flag, diagonal cuts and M notch) remain sharp.
    shape=shape.buffer(6.0,quad_segs=8).buffer(-6.0,quad_segs=8)
    shape=shape.simplify(7.0,preserve_topology=True)
    def smooth_ring(ring):
        points=list(ring.coords)[:-1]
        for _ in range(4):
            new=[]
            for i,current in enumerate(points):
                before=points[i-1];after=points[(i+1)%len(points)]
                incoming=(current[0]-before[0],current[1]-before[1])
                outgoing=(after[0]-current[0],after[1]-current[1])
                n1=math.hypot(*incoming);n2=math.hypot(*outgoing)
                angle=math.acos(max(-1,min(1,(incoming[0]*outgoing[0]+incoming[1]*outgoing[1])/max(n1*n2,1e-9))))
                if angle>math.radians(43) and n1>20 and n2>20:
                    new.append(current)
                else:
                    new.append(tuple(.19*before[j]+.81*current[j] for j in (0,1)))
                    new.append(tuple(.81*current[j]+.19*after[j] for j in (0,1)))
            points=new
        return points
    parts=[shape] if isinstance(shape,Polygon) else list(shape.geoms)
    shape=unary_union([Polygon(smooth_ring(p.exterior),[smooth_ring(r) for r in p.interiors]).buffer(0) for p in parts])
    shape=shape.simplify(.65,preserve_topology=True)
    polygons=[shape] if isinstance(shape,Polygon) else list(shape.geoms)
    outlines=[]
    for polygon in polygons:
        outlines.append({'outer':[[round(x,3),round(y,3)] for x,y in polygon.exterior.coords],
                         'holes':[[[round(x,3),round(y,3)] for x,y in ring.coords] for ring in polygon.interiors]})
    return {'width':round(shape.bounds[2],3),'polygons':outlines,
            'reference_box':[int(x0),int(y0),int(x1),int(y1)]}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    image=np.asarray(Image.open(args.image).convert('RGB'))
    r,g,b=[image[:,:,i].astype(int) for i in range(3)]
    mask=(r>195)&(g>174)&(b>135)&(r>=g)&(g>b)&(np.indices(r.shape)[0]>1060)
    parts=components(mask)
    upper=sorted([p for p in parts if min(y for _,y in p)<1200],key=lambda p:min(x for x,_ in p))
    lower=sorted([p for p in parts if min(y for _,y in p)>=1200],key=lambda p:min(x for x,_ in p))
    if len(upper)!=5 or len(lower)!=10:
        raise ValueError(f'This extractor expects the supplied 1024x1536 reference: found {len(upper)} upper and {len(lower)} lower components.')
    glyphs={ch:vectorize(p) for ch,p in zip('HUMAN',upper)}
    # Repeated letters allow standard forms to be separated from the oversized
    # initial R. The reference's second line contains ten distinct face shapes.
    for ch,index in [('R',6),('E',8),('S',9),('O',4),('C',7)]:
        glyphs[ch]=vectorize(lower[index],expansion=1.18)
    compact_u=vectorize(lower[5],expansion=1.18)
    glyphs['R.initial']=vectorize(lower[0],expansion=1.05)
    data={'source':'User-supplied game logo, 24 October 2026','image_size':[image.shape[1],image.shape[0]],
          'normalization':'700-unit cap height; source lean removed before common italic construction; second-line forms widened to match the hero row.',
          'glyphs':glyphs,'compact_u':compact_u}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(data,indent=2)+'\n')
    print('Recovered H U M A N R E S O C and the enlarged initial R directly from the logo.')
    print({ch:round(g['width']) for ch,g in glyphs.items()})


if __name__=='__main__':
    main()
