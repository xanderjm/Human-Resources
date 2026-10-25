#!/usr/bin/env python3
"""Logo-led third edition: traced brand lettering, completed alphabet and UI.

Reads logo-glyphs.json, never an installed or downloaded third-party font.
"""
import argparse
import json
import math
from pathlib import Path

from fontTools.svgLib.path import parse_path
from fontTools.ttLib import TTFont
from shapely import affinity
from shapely.geometry import Polygon, LineString, Point, box
from shapely.ops import unary_union

import build_fonts as core

DEFAULT=Path(__file__).resolve().parents[1]/'assets/fonts/human-resources-v3'
TRACED={}

# New letters complete the ten recovered letter faces. Soft bowls, tapered
# joins, diagonal terminal cuts and heavy stems follow the supplied logo.
LETTERS={
 'B':(615,'M0 0 L0 700 L365 700 C555 700 630 620 610 505 Q600 410 510 365 C660 310 655 170 605 95 Q550 0 370 0 Z M205 425 L315 425 Q405 425 410 500 Q415 555 325 555 L205 555 Z M205 150 L335 150 Q440 150 440 230 Q440 295 330 295 L205 295 Z'),
 'D':(650,'M0 0 L0 700 L335 700 C580 700 650 575 650 350 C650 120 575 0 330 0 Z M210 155 L305 155 Q430 155 430 350 Q430 545 305 545 L210 545 Z'),
 'F':(550,'M0 0 L0 700 L565 700 L525 535 L215 535 L215 410 L495 410 L460 250 L215 250 L215 0 Z'),
 'I':(230,'M0 0 L0 700 L230 700 L230 0 Z'),
 'J':(560,'M200 700 L560 700 L560 225 Q560 -10 270 -10 Q0 -10 0 210 L180 270 Q170 145 265 145 Q350 145 350 245 L350 535 L165 535 Z'),
 'K':(650,'M0 0 L0 700 L210 700 L210 425 L410 700 L665 700 L365 355 L675 0 L415 0 L210 270 L210 0 Z'),
 'L':(530,'M0 0 L0 700 L215 700 L215 170 L565 170 L520 0 Z'),
 'P':(620,'M0 0 L0 700 L355 700 C565 700 625 605 625 460 C625 315 540 235 350 235 L210 235 L210 0 Z M210 400 L315 400 Q420 400 420 470 Q420 545 315 545 L210 545 Z'),
 'T':(620,'M0 700 L650 700 L610 530 L425 530 L425 0 L210 0 L210 530 L-35 530 Z'),
 'V':(660,'M-15 700 L215 700 L330 210 L455 700 L685 700 L450 0 L210 0 Z'),
 'W':(900,'M-10 700 L205 700 L295 235 L405 555 L535 555 L645 235 L725 700 L930 700 L760 0 L565 0 L465 260 L360 0 L155 0 Z'),
 'X':(640,'M0 700 L250 700 L340 485 L445 700 L675 700 L470 350 L680 0 L430 0 L325 235 L220 0 L-25 0 L195 350 Z'),
 'Y':(660,'M-10 700 L230 700 L335 450 L445 700 L690 700 L440 275 L440 0 L220 0 L220 275 Z'),
 'Z':(620,'M0 700 L650 700 L615 535 L260 170 L655 170 L615 0 L-20 0 L15 165 L365 535 L-35 535 Z'),
}

DIGITS={
 '1':(480,'M0 515 L180 700 L365 700 L365 155 L480 155 L445 0 L35 0 L65 155 L165 155 L165 460 L60 375 Z'),
 '2':(570,'M0 525 C0 770 570 780 570 510 Q570 390 420 285 L235 155 L580 155 L545 0 L0 0 L0 155 L330 420 Q425 490 355 545 Q230 615 190 480 Z'),
 '3':(570,'M0 600 Q110 730 315 710 Q565 700 565 515 Q565 405 455 350 Q585 315 585 170 Q585 -10 305 -10 Q105 -10 0 105 L120 245 Q205 145 305 155 Q390 155 385 220 Q380 285 230 275 L230 420 Q380 410 380 490 Q380 565 290 555 Q195 550 125 475 Z'),
 '4':(580,'M315 0 L315 150 L-15 150 L-15 310 L305 700 L510 700 L510 315 L590 315 L555 150 L510 150 L510 0 Z M180 315 L315 315 L315 485 Z'),
 '5':(550,'M30 700 L555 700 L520 535 L205 535 L195 425 Q565 505 565 205 Q565 -10 285 -10 Q80 -10 0 100 L120 245 Q195 145 290 155 Q390 155 375 240 Q360 335 160 270 L0 315 Z'),
 '6':(580,'M550 640 Q420 750 250 700 Q-10 630 -10 290 Q-10 -10 285 -10 Q580 -10 580 235 Q580 465 210 410 Q235 600 450 500 Z M205 245 Q205 145 290 145 Q385 145 385 245 Q385 330 295 330 Q210 330 205 245 Z'),
 '7':(580,'M0 700 L605 700 L570 535 L315 0 L90 0 L350 530 L-35 530 Z'),
 '8':(580,'M290 710 C-25 710 -30 410 115 350 C-75 260 -15 -10 290 -10 C600 -10 650 265 470 350 C620 420 605 710 290 710 Z M290 555 Q390 555 380 480 Q375 415 290 415 Q205 415 200 480 Q190 555 290 555 Z M290 285 Q395 285 390 220 Q385 145 290 145 Q195 145 190 220 Q185 285 290 285 Z'),
 '9':(580,'M30 60 Q160 -50 330 0 Q590 70 590 410 Q590 710 295 710 Q0 710 0 465 Q0 235 370 290 Q345 100 130 200 Z M375 455 Q375 555 290 555 Q195 555 195 455 Q195 370 285 370 Q370 370 375 455 Z'),
}

# More humanist UI shapes replace the previous octagonal construction. Curves
# are asymmetric, apertures are generous, and the lowercase a is double-storey.
UI={
 'a':(410,'M25 440 C120 540 385 550 400 365 L400 0 M400 265 C80 320 -35 200 20 70 C65 -40 315 -15 400 110'),
 'b':(405,'M0 0 L0 725 M0 385 C145 595 415 500 405 245 C395 -25 120 -85 0 115'),
 'c':(385,'M380 425 C190 590 -10 450 0 240 C10 -10 235 -75 385 80'),
 'd':(405,'M405 0 L405 725 M405 385 C260 595 -10 500 0 245 C10 -25 285 -85 405 115'),
 'e':(415,'M0 255 L415 255 C405 595 -10 575 0 250 C5 -10 250 -55 395 80'),
 'f':(265,'M90 0 L90 565 Q90 760 265 700 M0 480 L255 480'),
 'g':(405,'M405 500 L405 -30 C405 -240 100 -250 15 -140 M405 385 C260 595 -10 500 0 245 C10 -25 285 -85 405 115'),
 'h':(410,'M0 0 L0 725 M0 335 C70 560 410 555 410 335 L410 0'),
 'm':(660,'M0 0 L0 500 M0 335 C60 555 320 555 320 335 L320 0 M320 335 C380 555 660 555 660 335 L660 0'),
 'n':(410,'M0 0 L0 500 M0 335 C70 560 410 555 410 335 L410 0'),
 'o':(415,'M200 505 C55 510 -5 400 0 240 C5 80 65 -5 215 0 C365 5 420 110 415 270 C410 430 345 500 200 505 Z'),
 'p':(405,'M0 -210 L0 500 M0 385 C145 595 415 500 405 245 C395 -25 120 -85 0 115'),
 'q':(405,'M405 -210 L405 500 M405 385 C260 595 -10 500 0 245 C10 -25 285 -85 405 115'),
 'r':(270,'M0 0 L0 500 M0 310 Q65 535 270 465'),
 's':(360,'M355 430 C250 555 -5 510 0 365 C5 220 360 275 360 125 C360 -40 95 -30 0 75'),
 'u':(410,'M0 500 L0 165 C0 -55 340 -60 410 165 M410 500 L410 0'),
}

FIELD={
 'A':(500,'M0 0 Q75 385 230 690 Q270 740 290 675 L500 0 M90 205 L425 245'),
 'C':(480,'M475 595 C270 820 -30 605 0 300 C20 -40 310 -80 480 95'),
 'E':(420,'M425 695 L15 665 L0 0 L430 30 M10 345 L345 380'),
 'H':(500,'M0 0 L25 710 M480 0 L500 700 M10 325 L490 380'),
 'M':(620,'M0 0 L30 700 L300 205 L570 710 L620 0'),
 'N':(520,'M0 0 L20 700 L505 0 L520 720'),
 'O':(515,'M220 710 C-30 690 -55 280 55 75 C140 -80 445 -15 510 250 C580 510 455 745 220 710 Z'),
 'R':(480,'M0 0 L25 700 C620 780 605 290 25 325 M225 345 L500 0'),
 'S':(470,'M470 615 C300 800 -15 650 10 480 C35 310 505 365 470 155 C440 -65 110 -55 0 100'),
 'a':(390,'M385 0 L400 505 M390 390 C185 665 -65 405 15 130 C70 -75 330 -25 390 140'),
 'c':(370,'M370 420 C180 620 -30 395 10 180 C50 -40 250 -35 380 90'),
 'e':(390,'M0 210 Q445 220 365 415 C270 625 -30 475 0 235 C20 -30 275 -55 390 90'),
 'f':(270,'M35 -170 L90 550 Q105 765 280 695 M0 435 L265 475'),
 'g':(390,'M385 505 L385 -30 C385 -260 105 -245 0 -140 M385 390 C180 665 -65 405 15 130 C70 -75 330 -25 385 140'),
 'h':(405,'M0 0 L35 725 M15 310 C105 560 410 590 405 320 L395 0'),
 'm':(645,'M0 0 L15 505 M10 330 C45 540 310 580 310 325 L300 0 M310 325 C390 560 645 555 645 310 L635 0'),
 'n':(405,'M0 0 L15 505 M10 330 C100 560 410 590 405 320 L395 0'),
 'o':(390,'M160 510 C-30 475 -35 195 60 45 C150 -95 375 45 395 265 C410 460 310 555 160 510 Z'),
 'r':(270,'M0 0 L15 500 M10 285 Q100 555 280 455'),
 's':(345,'M340 440 C150 575 -10 440 5 345 C25 225 375 265 345 100 C320 -45 110 -10 0 65'),
 't':(255,'M105 665 L70 115 Q45 -20 260 35 M0 430 L255 470'),
 'u':(395,'M15 505 L0 165 C-10 -80 320 -25 390 165 M405 505 L375 0'),
 'y':(400,'M0 505 L210 0 M405 510 L170 -100 Q105 -250 -5 -165'),
}

STYLES=[
 core.Style('HR Abduction Logo','Black Italic',900,165,1,38,'logo'),
 core.Style('HR Abduction Compact','Black Italic',900,165,.78,37,'compact'),
 core.Style('HR Beam UI','Medium',500,73,1,49,'ui'),
 core.Style('HR Beam UI','SemiBold',600,89,1,49,'ui'),
 core.Style('HR Beam UI','Bold',700,106,1,49,'ui'),
 core.Style('HR Field Lettering','Regular',400,95,.96,58,'field'),
]


def filled(path):
    pen=core.FlattenPen();parse_path(path,pen);pen._endPath();result=Polygon()
    for points in pen.paths:
        p=Polygon(points)
        result=result.symmetric_difference(p if p.is_valid else p.buffer(0))
    return result


def decode(record):
    return unary_union([Polygon(p['outer'],p['holes']) for p in record['polygons']])


def upright_letter(ch):
    if ch in TRACED['glyphs']:
        record=TRACED['glyphs'][ch];shape=decode(record);width=record['width']
        if ch=='A':
            # Retain the logo's outside silhouette, but give everyday typing a
            # plain counter. Its illustrated UFO belongs in an optional glyph.
            solid=shape.convex_hull
            foot=filled(f'M{width*.29} -25 L{width*.73} -25 L{width*.64} 185 L{width*.39} 185 Z')
            counter=filled(f'M{width*.45} 335 L{width*.62} 335 L{width*.565} 535 Q{width*.535} 555 {width*.45} 335 Z')
            shape=solid.difference(unary_union([foot,counter]))
        return shape,width
    if ch=='G':
        c,width=upright_letter('C')
        return unary_union([c,box(width*.46,230,width*.98,370),box(width*.75,80,width*.98,350)]),width
    if ch=='Q':
        o,width=upright_letter('O')
        tail=filled(f'M{width*.47} 180 L{width*.72} 215 L{width+55} -85 L{width*.65} -40 Z')
        return unary_union([o,tail]),width+30
    width,path=LETTERS[ch]
    return filled(path),width


def logo_shape(ch,style):
    # Logo typography is intentionally unicase.
    key='I' if ch=='ı' else ch.upper() if ch.isascii() and ch.isalpha() else ch
    if key in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' and len(key)==1:
        shape,width=upright_letter(key)
    elif ch.isascii() and ch.isdigit():
        if ch=='0':
            shape,width=upright_letter('O')
        else:
            width,path=DIGITS[ch];shape=filled(path)
        natural=core.DRAWINGS[ch][0]+style.stroke
        shape=affinity.scale(shape,xfact=natural/width,yfact=1,origin=(0,0));width=natural
    elif ch in 'ÆæŒœ':
        a,aw=upright_letter('A' if ch in 'Ææ' else 'O');e,ew=upright_letter('E')
        shape=unary_union([a,affinity.translate(e,xoff=aw-55)]);width=aw+ew-55
    elif ch in 'ØøĐđÐðÞþŁł':
        letter='O' if ch in 'Øø' else 'D' if ch in 'ĐđÐð' else 'P' if ch in 'Þþ' else 'L'
        shape,width=upright_letter(letter)
        if ch in 'Øø':
            bar=LineString([(65,40),(width-65,660)]).buffer(33,cap_style=2)
        elif ch in 'Łł':
            bar=LineString([(-20,250),(300,480)]).buffer(35,cap_style=2)
        elif ch in 'ĐđÐð':
            bar=box(-30,295,300,405)
        else:
            bar=Polygon()
        shape=unary_union([shape,bar])
    else:
        width,path=core.DRAWINGS[ch]
        shape=core.shape_path(path,style.stroke)
        shape=affinity.scale(shape,xfact=1,yfact=1-style.stroke/700,origin=(0,0))
        shape=affinity.translate(shape,xoff=style.stroke/2,yoff=style.stroke/2)
        width+=style.stroke
    shape=affinity.scale(shape,xfact=style.width,yfact=1,origin=(0,0));width*=style.width
    # All slant is in the outlines; fake italics must not be applied by clients.
    shape=affinity.affine_transform(shape,[1,.31,0,1,style.side,0])
    advance=round(width+2*style.side)
    if ch.isascii() and ch.isdigit():
        target=round(490*style.width+style.stroke+style.side*2)
        shape=affinity.translate(shape,xoff=(target-advance)/2);advance=target
    return shape,advance


def make_base(ch,style):
    if style.character in ('logo','compact'):
        return logo_shape(ch,style)
    width,path=(FIELD if style.character=='field' else UI).get(ch,core.DRAWINGS[ch])
    if style.character=='field':
        shape=brush_path(path,style.stroke)
    else:
        shape=core.shape_path(path,style.stroke)
    if style.character=='field':
        # Less regular widths and hand-cut, broad pen endings for Earth signs.
        phase=(ord(ch)*19%11-5)/5
        shape=affinity.scale(shape,xfact=1+phase*.035,yfact=1,origin=(width/2,250))
        shape=affinity.skew(shape,xs=phase*2.5,origin=(0,0))
        shape=affinity.rotate(shape,phase*1.35,origin=(width/2,250))
    shape=affinity.scale(shape,xfact=style.width,yfact=1-style.stroke/700,origin=(0,0))
    shape=affinity.translate(shape,xoff=style.side+style.stroke/2,yoff=style.stroke/2)
    advance=round(width*style.width+style.stroke+style.side*2)
    if ch.isascii() and ch.isdigit():
        target=round(490*style.width+style.stroke+style.side*2)
        shape=affinity.translate(shape,xoff=(target-advance)/2);advance=target
    return shape,advance


def brush_path(path,stroke):
    pen=core.FlattenPen();parse_path(path,pen);pen._endPath();pieces=[]
    for points in pen.paths:
        lengths=[0]
        for a,b in zip(points,points[1:]):
            lengths.append(lengths[-1]+math.dist(a,b))
        total=max(lengths[-1],1)
        radii=[stroke/2*(1.12-.27*d/total+.08*math.sin(d/total*math.pi)) for d in lengths]
        for i,(a,b) in enumerate(zip(points,points[1:])):
            length=math.dist(a,b)
            if length<1e-6:
                continue
            nx,ny=-(b[1]-a[1])/length,(b[0]-a[0])/length
            r1,r2=radii[i:i+2]
            pieces.append(Polygon([(a[0]+nx*r1,a[1]+ny*r1),(b[0]+nx*r2,b[1]+ny*r2),
                                   (b[0]-nx*r2,b[1]-ny*r2),(a[0]-nx*r1,a[1]-ny*r1)]))
        if total<stroke*.6:
            pieces.append(Point(points[0]).buffer(stroke/2,quad_segs=6))
        else:
            for point,radius in zip(points[1:-1],radii[1:-1]):
                pieces.append(Point(point).buffer(radius,quad_segs=4))
    return unary_union(pieces)


def finish_font(path,style):
    font=TTFont(path,recalcTimestamp=False)
    font['head'].fontRevision=3.0
    name=font['name'];ps=name.getDebugName(6)
    for platform,encoding,language in [(3,1,0x409),(1,0,0)]:
        name.setName('Version 3.000',5,platform,encoding,language)
        name.setName(f'3.000;HR26;{ps}',3,platform,encoding,language)
        name.setName('Logo-led game typeface. Brand glyphs reconstructed from the user-supplied logo; missing characters drawn for this project.',10,platform,encoding,language)
    if style.character in ('logo','compact'):
        font['OS/2'].fsSelection=(1<<7)|1
        font['OS/2'].sxHeight=700
        font['OS/2'].usWidthClass=3 if style.character=='compact' else 5
        font['head'].macStyle=2
        font['post'].italicAngle=-math.degrees(math.atan(.31))
        for platform,encoding,language in [(3,1,0x409),(1,0,0)]:
            name.setName(style.family+' Black',1,platform,encoding,language)
            name.setName('Italic',2,platform,encoding,language)
        # The original oversized initial R remains available as a real alternate.
        shape=decode(TRACED['glyphs']['R.initial'])
        shape=affinity.scale(shape,xfact=style.width,yfact=1,origin=(0,0))
        shape=affinity.affine_transform(shape,[1,.31,0,1,style.side,0])
        glyph=core.geometry_to_glyph(shape);glyph.recalcBounds(None)
        font['glyf']['R.initial']=glyph
        font['hmtx']['R.initial']=(round(TRACED['glyphs']['R.initial']['width']*style.width+2*style.side),glyph.xMin)
        # A saucer-and-beam alternate is monochrome by design, like any TTF glyph.
        a=decode(TRACED['glyphs']['A']).convex_hull;aw=TRACED['glyphs']['A']['width']
        foot=filled(f'M{aw*.29} -25 L{aw*.73} -25 L{aw*.64} 105 L{aw*.39} 105 Z')
        a=a.difference(foot)
        disc=Point(aw*.52,430).buffer(1,quad_segs=16)
        disc=affinity.scale(disc,xfact=145,yfact=36,origin=(aw*.52,430))
        dome=Point(aw*.52,455).buffer(1,quad_segs=16)
        dome=affinity.scale(dome,xfact=75,yfact=54,origin=(aw*.52,455)).intersection(box(0,455,aw,600))
        beam=filled(f'M{aw*.45} 410 L{aw*.59} 410 L{aw*.69} 35 L{aw*.29} 35 Z')
        star=filled(f'M{aw*.50} 300 L{aw*.53} 237 L{aw*.60} 220 L{aw*.53} 202 L{aw*.50} 140 L{aw*.47} 202 L{aw*.40} 220 L{aw*.47} 237 Z')
        stencil=unary_union([disc,dome,beam]).difference(star)
        a=a.difference(stencil)
        a=affinity.scale(a,xfact=style.width,yfact=1,origin=(0,0))
        a=affinity.affine_transform(a,[1,.31,0,1,style.side,0])
        glyph=core.geometry_to_glyph(a);glyph.recalcBounds(None)
        font['glyf']['A.abduction']=glyph
        font['hmtx']['A.abduction']=(font['hmtx'][core.glyph_name('A')][0],glyph.xMin)
        # Append two small GSUB stylistic-set lookups using feaLib's table model.
        from fontTools.ttLib.tables import otTables
        table=font['GSUB'].table
        for tag,subs in [('ss01',{core.glyph_name('A'):'A.abduction',core.glyph_name('a'):'A.abduction'}),
                         ('ss02',{core.glyph_name('R'):'R.initial',core.glyph_name('r'):'R.initial'})]:
            sub=otTables.SingleSubst();sub.mapping=subs
            lookup=otTables.Lookup();lookup.LookupType=1;lookup.LookupFlag=0;lookup.SubTable=[sub];lookup.SubTableCount=1
            index=len(table.LookupList.Lookup);table.LookupList.Lookup.append(lookup);table.LookupList.LookupCount+=1
            record=otTables.FeatureRecord();record.FeatureTag=tag;record.Feature=otTables.Feature()
            record.Feature.FeatureParams=None;record.Feature.LookupListIndex=[index];record.Feature.LookupCount=1
            table.FeatureList.FeatureRecord.append(record)
        old_records=list(table.FeatureList.FeatureRecord)
        sorted_records=sorted(enumerate(old_records),key=lambda item:item[1].FeatureTag)
        remap={old:new for new,(old,_) in enumerate(sorted_records)}
        table.FeatureList.FeatureRecord=[r for _,r in sorted_records]
        table.FeatureList.FeatureCount=len(sorted_records)
        for script_record in table.ScriptList.ScriptRecord:
            script=script_record.Script
            languages=([script.DefaultLangSys] if script.DefaultLangSys else [])+[r.LangSys for r in script.LangSysRecord]
            for language in languages:
                language.FeatureIndex=sorted([remap[i] for i in language.FeatureIndex]+[remap[len(old_records)-2],remap[len(old_records)-1]])
                language.FeatureCount=len(language.FeatureIndex)
                if language.ReqFeatureIndex!=0xFFFF:
                    language.ReqFeatureIndex=remap[language.ReqFeatureIndex]
    font.save(path)
    font.flavor='woff2';font.save(path.parents[1]/'woff2'/f'{path.stem}.woff2')
    return len(font.getGlyphOrder())


def main():
    global TRACED
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=DEFAULT)
    parser.add_argument('--glyphs',type=Path)
    args=parser.parse_args();out=args.output.resolve()
    source=args.glyphs or out/'source/logo-glyphs.json'
    TRACED=json.loads(source.read_text())
    for name in ('ttf','woff2','source','preview','wordmarks'):
        (out/name).mkdir(parents=True,exist_ok=True)
    core.OUT=out;core.make_base=make_base
    records=[]
    for style in STYLES:
        record=core.build(style)
        record['glyphs']=finish_font(out/'ttf'/f'{record["file"]}.ttf',style)
        record['italic']=style.character in ('logo','compact')
        records.append(record)
    (out/'manifest.json').write_text(json.dumps({'version':'3.000','license':'MIT','fonts':records},indent=2)+'\n')
    (out/'fonts.css').write_text('\n'.join("@font-face { font-family: '%s'; src: url('woff2/%s.woff2') format('woff2'); font-weight: %s; font-style: %s; font-display: swap; }"%(r['family'],r['file'],r['weight'],'italic' if r['italic'] else 'normal') for r in records)+'\n')
    print('Built logo edition: six TTF/WOFF2 pairs; both logo widths include ss01 and ss02 alternates.')


if __name__=='__main__':
    main()
