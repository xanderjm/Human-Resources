#!/usr/bin/env python3
"""Second edition: original invasion-poster, panic, bureau and Earth-sign fonts.

Reuses the first edition's Unicode/OpenType compiler, not its finished fonts.
All principal alphabets below are new drawings. Reference analysis is bundled.
"""
import argparse
import json
import math
from pathlib import Path

from fontTools.svgLib.path import parse_path
from fontTools.ttLib import TTFont
from shapely import affinity
from shapely.geometry import LineString, Polygon, box
from shapely.ops import unary_union

import build_fonts as core

DEFAULT=Path(__file__).resolve().parents[1]/'assets/fonts/human-resources-v2'

# Filled, deliberately non-monoline silhouettes: splayed stems, off-axis counters,
# broad shoulders and pointed feet. These are original drawings, not logo traces.
TAKEOVER={
 'A':(600,'M-10 0 L185 680 L365 715 L610 0 L425 0 L373 180 L178 155 L145 0 Z M215 300 L332 320 L269 525 Z'),
 'B':(540,'M0 -5 L25 695 L310 710 L500 640 L530 475 L440 365 L550 255 L525 65 L340 -10 Z M180 425 L315 440 L350 515 L310 565 L183 552 Z M165 150 L330 165 L370 235 L320 300 L173 287 Z'),
 'C':(560,'M555 600 L405 718 L125 675 L-10 500 L10 160 L155 -10 L425 20 L570 150 L425 290 L350 180 L200 195 L170 470 L250 545 L370 545 L425 455 Z'),
 'D':(560,'M0 0 L25 700 L300 712 L510 565 L560 210 L390 0 Z M185 155 L302 175 L360 260 L350 470 L280 550 L190 548 Z'),
 'E':(510,'M0 0 L20 700 L510 720 L475 545 L190 535 L184 420 L430 445 L410 280 L175 280 L170 155 L505 180 L475 -5 Z'),
 'F':(500,'M0 0 L25 700 L510 720 L475 540 L193 535 L185 410 L435 435 L410 270 L175 272 L170 0 Z'),
 'G':(600,'M575 600 L415 715 L125 680 L-10 495 L10 160 L165 -10 L455 15 L585 150 L570 400 L315 385 L315 245 L410 255 L405 175 L215 185 L175 465 L245 545 L375 540 L425 460 Z'),
 'H':(580,'M0 0 L20 700 L205 715 L185 450 L390 470 L385 700 L565 680 L590 0 L390 0 L390 290 L178 270 L175 0 Z'),
 'I':(290,'M0 700 L290 720 L270 550 L225 548 L210 150 L280 155 L275 0 L-5 0 L0 155 L58 152 L70 545 L10 540 Z'),
 'J':(480,'M140 700 L480 710 L460 165 L315 -10 L90 10 L-10 185 L120 290 L180 180 L265 180 L280 550 L140 540 Z'),
 'K':(560,'M0 0 L20 700 L195 715 L178 425 L370 710 L570 680 L320 370 L590 -10 L380 0 L180 280 L172 0 Z'),
 'L':(480,'M0 0 L25 700 L210 715 L175 175 L490 190 L460 -10 Z'),
 'M':(730,'M0 0 L30 700 L210 720 L365 385 L530 710 L715 680 L745 0 L550 0 L535 365 L365 110 L195 380 L180 0 Z'),
 'N':(590,'M0 0 L20 700 L200 715 L410 320 L395 700 L570 685 L595 0 L420 -5 L180 410 L175 0 Z'),
 'O':(620,'M105 660 L375 720 L565 575 L630 245 L470 30 L180 -15 L-10 155 L0 485 Z M200 470 L175 235 L265 155 L385 180 L430 290 L405 470 L320 550 L245 535 Z'),
 'P':(540,'M0 0 L20 700 L320 715 L515 590 L545 400 L410 270 L180 275 L175 0 Z M190 440 L325 425 L365 475 L340 550 L193 552 Z'),
 'Q':(620,'M105 660 L375 720 L565 575 L630 245 L505 75 L640 -65 L400 -35 L340 10 L180 -15 L-10 155 L0 485 Z M200 470 L175 235 L265 155 L385 180 L430 290 L405 470 L320 550 L245 535 Z'),
 'R':(560,'M0 0 L20 700 L325 715 L520 590 L540 400 L405 295 L600 -20 L385 0 L205 277 L180 275 L175 0 Z M190 440 L325 425 L365 475 L340 550 L193 552 Z'),
 'S':(550,'M540 625 L450 715 L110 680 L-15 565 L55 370 L350 280 L385 190 L180 150 L115 240 L-15 70 L110 -15 L475 30 L575 145 L520 390 L245 465 L185 540 L405 555 L455 470 Z'),
 'T':(570,'M-10 700 L585 720 L550 535 L385 535 L385 0 L175 -5 L195 530 L10 520 Z'),
 'U':(590,'M0 700 L195 715 L180 230 L250 160 L365 180 L405 250 L385 700 L575 680 L600 185 L445 5 L190 -15 L-5 165 Z'),
 'V':(600,'M-10 700 L195 715 L305 230 L400 700 L610 680 L420 -10 L210 0 Z'),
 'W':(800,'M-10 700 L170 715 L240 270 L375 555 L470 555 L590 270 L650 710 L825 680 L690 0 L515 -5 L420 240 L310 0 L145 10 Z'),
 'X':(580,'M0 700 L205 715 L300 485 L405 710 L590 680 L410 355 L615 0 L395 -5 L285 240 L175 0 L-20 0 L175 355 Z'),
 'Y':(590,'M0 700 L195 715 L305 460 L405 710 L610 680 L400 280 L395 0 L190 -5 L200 285 Z'),
 'Z':(550,'M0 700 L560 720 L550 535 L225 155 L560 180 L525 -5 L-10 0 L0 165 L320 540 L15 525 Z'),
}

# Cartoon panic: tall, bent centerlines with sharp directional changes. Broad,
# blunt strokes produce narrow counters, distinct from Takeover's filled wedges.
PANIC={
 'A':(480,'M0 0 L190 700 L330 735 L500 10 M85 220 L420 285'),
 'B':(430,'M0 0 L35 700 L260 680 L420 570 L365 410 L35 355 M245 390 L445 250 L350 15 L0 0'),
 'C':(430,'M430 590 L310 710 L65 610 L0 170 L160 -10 L425 90'),
 'D':(440,'M0 0 L30 700 L295 650 L450 465 L385 105 L0 0'),
 'E':(380,'M390 710 L35 680 L0 0 L390 25 M15 350 L310 400'),
 'F':(370,'M390 710 L35 680 L0 0 M15 350 L310 400'),
 'G':(470,'M450 585 L310 710 L65 610 L0 170 L160 -10 L440 65 L450 340 L265 305'),
 'H':(450,'M0 0 L35 700 M450 0 L420 720 M15 310 L440 390'),
 'I':(230,'M0 680 L230 710 M130 700 L105 0 M0 -5 L230 15'),
 'J':(370,'M370 710 L340 130 L180 -15 L0 100 L15 220'),
 'K':(420,'M0 0 L30 710 M420 710 L20 255 M200 440 L445 0'),
 'L':(360,'M30 710 L0 0 L370 30'),
 'M':(590,'M0 0 L25 700 L290 270 L540 720 L590 0'),
 'N':(480,'M0 0 L30 710 L455 0 L480 720'),
 'O':(460,'M130 710 L365 650 L470 360 L340 0 L90 45 L0 330 Z'),
 'P':(420,'M0 0 L30 710 L280 670 L430 520 L340 325 L15 340'),
 'Q':(460,'M130 710 L365 650 L470 360 L340 0 L90 45 L0 330 Z M245 190 L490 -70'),
 'R':(440,'M0 0 L30 710 L280 670 L430 520 L340 325 L15 340 M225 345 L460 0'),
 'S':(430,'M425 615 L280 720 L25 575 L50 410 L355 285 L430 100 L170 -10 L0 90'),
 'T':(440,'M0 680 L450 720 M240 700 L195 0'),
 'U':(450,'M0 710 L20 165 L175 -10 L400 85 L450 720'),
 'V':(470,'M0 710 L225 -10 L470 720'),
 'W':(660,'M0 710 L125 0 L325 380 L520 -10 L660 720'),
 'X':(430,'M0 710 L430 0 M430 720 L0 0'),
 'Y':(450,'M0 710 L210 325 L450 720 M210 325 L175 0'),
 'Z':(420,'M0 680 L430 720 L0 0 L430 30'),
}

BUREAU={
 'A':(520,'M0 0 L220 700 L300 700 L520 0 M95 235 L425 235'),
 'B':(480,'M0 0 L0 700 L330 700 L460 590 L460 470 L350 360 L0 360 M350 360 L480 250 L480 115 L350 0 L0 0'),
 'C':(500,'M500 610 L410 700 L100 700 L0 600 L0 100 L100 0 L410 0 L500 90'),
 'D':(500,'M0 0 L0 700 L340 700 L500 540 L500 160 L340 0 Z'),
 'E':(440,'M440 700 L0 700 L0 0 L440 0 M0 350 L365 350'),
 'G':(520,'M500 610 L410 700 L100 700 L0 600 L0 100 L100 0 L420 0 L520 100 L520 330 L310 330'),
 'J':(400,'M400 700 L400 110 L290 0 L100 0 L0 100 L0 180'),
 'O':(520,'M100 700 L420 700 L520 600 L520 100 L420 0 L100 0 L0 100 L0 600 Z'),
 'P':(480,'M0 0 L0 700 L350 700 L480 570 L480 455 L355 330 L0 330'),
 'Q':(520,'M100 700 L420 700 L520 600 L520 100 L420 0 L100 0 L0 100 L0 600 Z M320 160 L550 -70'),
 'R':(490,'M0 0 L0 700 L350 700 L480 570 L480 455 L355 330 L0 330 M250 330 L490 0'),
 'S':(480,'M480 600 L380 700 L100 700 L0 600 L0 450 L100 350 L380 350 L480 250 L480 100 L380 0 L100 0 L0 100'),
 'U':(500,'M0 700 L0 110 L110 0 L390 0 L500 110 L500 700'),
 'a':(400,'M20 440 L95 500 L305 500 L400 405 L400 0 M400 255 L95 255 L0 160 L0 85 L85 0 L300 0 L400 100'),
 'b':(400,'M0 0 L0 720 M0 380 L115 500 L300 500 L400 400 L400 100 L300 0 L115 0 L0 115'),
 'c':(380,'M380 425 L305 500 L100 500 L0 400 L0 100 L100 0 L305 0 L380 75'),
 'd':(400,'M400 0 L400 720 M400 380 L285 500 L100 500 L0 400 L0 100 L100 0 L285 0 L400 115'),
 'e':(400,'M0 255 L400 255 L400 400 L300 500 L100 500 L0 400 L0 100 L100 0 L300 0 L380 80'),
 'f':(250,'M80 0 L80 600 L180 700 L260 700 M0 480 L250 480'),
 'g':(400,'M400 500 L400 -100 L300 -200 L85 -200 L20 -135 M400 380 L285 500 L100 500 L0 400 L0 100 L100 0 L285 0 L400 115'),
 'h':(400,'M0 0 L0 720 M0 380 L115 500 L300 500 L400 400 L400 0'),
 'i':(80,'M40 0 L40 500 M40 670 L40 720'),
 'j':(170,'M170 500 L170 -100 L70 -200 L0 -200 M170 670 L170 720'),
 'l':(150,'M0 720 L0 85 L85 0 L150 0'),
 'm':(650,'M0 0 L0 500 M0 380 L115 500 L225 500 L315 400 L315 0 M315 380 L430 500 L550 500 L650 400 L650 0'),
 'n':(400,'M0 0 L0 500 M0 380 L115 500 L300 500 L400 400 L400 0'),
 'o':(400,'M100 500 L300 500 L400 400 L400 100 L300 0 L100 0 L0 100 L0 400 Z'),
 'p':(400,'M0 -210 L0 500 M0 380 L115 500 L300 500 L400 400 L400 100 L300 0 L115 0 L0 115'),
 'q':(400,'M400 -210 L400 500 M400 380 L285 500 L100 500 L0 400 L0 100 L100 0 L285 0 L400 115'),
 'r':(265,'M0 0 L0 500 M0 345 L140 485 L265 485'),
 's':(350,'M350 430 L280 500 L75 500 L0 425 L0 330 L75 255 L275 255 L350 180 L350 75 L275 0 L75 0 L0 75'),
 't':(260,'M90 660 L90 85 L175 0 L260 0 M0 475 L260 475'),
 'u':(400,'M0 500 L0 100 L100 0 L285 0 L400 120 M400 500 L400 0'),
 '0':(450,'M100 700 L350 700 L450 600 L450 100 L350 0 L100 0 L0 100 L0 600 Z M100 125 L350 575'),
 '2':(450,'M0 570 L0 610 L90 700 L350 700 L450 600 L450 480 L0 100 L0 0 L450 0'),
 '3':(450,'M0 625 L90 700 L350 700 L450 600 L450 450 L350 350 L185 350 M350 350 L450 250 L450 100 L350 0 L90 0 L0 75'),
 '5':(430,'M430 700 L0 700 L0 370 L330 370 L430 270 L430 100 L330 0 L85 0 L0 75'),
 '6':(450,'M420 650 L350 700 L120 700 L0 580 L0 100 L100 0 L350 0 L450 100 L450 270 L350 370 L100 370 L0 270'),
 '8':(450,'M100 700 L350 700 L450 600 L450 465 L335 350 L100 350 L0 465 L0 600 Z M100 350 L350 350 L450 250 L450 100 L350 0 L100 0 L0 100 L0 250 Z'),
 '9':(450,'M30 50 L100 0 L330 0 L450 120 L450 600 L350 700 L100 700 L0 600 L0 430 L100 330 L350 330 L450 430'),
}

STYLES=[
 core.Style('HR Bureau','Medium',500,67,.93,51,'bureau'),
 core.Style('HR Bureau','SemiBold',600,82,.93,51,'bureau'),
 core.Style('HR Bureau','Bold',700,98,.93,51,'bureau'),
 core.Style('HR Panic Broadcast','Bold',700,112,.92,48,'panic'),
 core.Style('HR Hostile Takeover','Black',900,145,1,47,'takeover'),
 core.Style('HR Earthling Signs','Regular',400,88,1.03,60,'earth'),
]


def filled(path):
    pen=core.FlattenPen()
    parse_path(path,pen)
    pen._endPath()
    result=Polygon()
    for points in pen.paths:
        polygon=Polygon(points)
        if not polygon.is_valid:
            polygon=polygon.buffer(0)
        result=result.symmetric_difference(polygon)
    return result


def angular_stroke(path,stroke):
    pen=core.FlattenPen();parse_path(path,pen);pen._endPath()
    strokes=[]
    for points in pen.paths:
        line=LineString(points)
        # Dots need their full square cap; a butt cap on a five-unit dot would
        # almost disappear even though the surrounding strokes are bold.
        cap=3 if line.length<stroke*.7 else 2
        strokes.append(line.buffer(stroke/2,cap_style=cap,join_style=2,mitre_limit=2.4))
    return unary_union(strokes).buffer(4,join_style=1).buffer(-4,join_style=1)


def source_drawing(ch,kind):
    lookup=PANIC if kind=='panic' else BUREAU
    return lookup.get(ch,BUREAU.get(ch,core.DRAWINGS[ch]))


def make_base(ch,style):
    kind=style.character
    # The title is intentionally unicase, like a film-poster lettering alphabet.
    title_ch='I' if ch=='ı' else ch.upper() if ch.isascii() and ch.isalpha() else ch
    if kind=='takeover' and title_ch in TAKEOVER:
        width,path=TAKEOVER[title_ch]
        geometry=filled(path)
        geometry=affinity.translate(geometry,xoff=style.side+15)
        advance=width+2*style.side+20
        return geometry,advance

    if kind=='takeover' and ch in 'ÆæŒœØøĐđÐðÞþŁł':
        if ch in 'ÆæŒœ':
            first='A' if ch in 'Ææ' else 'O'
            a,aw=make_base(first,style)
            b,bw=make_base('E',style)
            return unary_union([a,affinity.translate(b,xoff=aw-55)]),aw+bw-55
        letter='O' if ch in 'Øø' else 'D' if ch in 'ĐđÐð' else 'P' if ch in 'Þþ' else 'L'
        shape,advance=make_base(letter,style)
        if ch in 'Øø':
            bar=angular_stroke('M75 65 L570 640',65)
        elif ch in 'Łł':
            bar=angular_stroke('M0 250 L340 490',70)
        elif ch in 'ĐđÐð':
            bar=angular_stroke('M0 350 L320 350',75)
        else:
            return shape,advance
        return unary_union([shape,affinity.translate(bar,xoff=style.side)]),advance

    if kind=='panic' and ch in PANIC:
        width,path=PANIC[ch]
    else:
        width,path=source_drawing(ch,'bureau' if kind=='bureau' else kind)

    # Display punctuation uses directional, flat-ended strokes too.
    geometry=angular_stroke(path,style.stroke)
    if kind=='earth':
        # The Earth world gets a mid-century slab-letter construction: real
        # horizontal serifs, rather than the former sign face's generic italic.
        stem_positions={
            'B':[0],'D':[0],'E':[0],'F':[0],'H':[0,500],
            'I':[130],'K':[0],'L':[0],'M':[0,620],'N':[0,520],
            'P':[0],'R':[0],'T':[250],
            'b':[0],'d':[400],'h':[0],'i':[40],'k':[0],'l':[0],
            'm':[0,310,650],'n':[0,400],'p':[0],'q':[400],
            'r':[0],'u':[0,400],
        }
        serifs=[]
        for x in stem_positions.get(ch,[]):
            top=700 if ch.isupper() else (720 if ch in 'bdhkl' else 500)
            bottom=-210 if ch in 'pq' else 0
            serifs.extend([box(x-82,bottom-style.stroke/2,x+82,bottom+style.stroke/2+18),
                           box(x-82,top-style.stroke/2-18,x+82,top+style.stroke/2)])
        geometry=unary_union([geometry,*serifs])
    geometry=affinity.scale(geometry,xfact=style.width,yfact=1-style.stroke/700,origin=(0,0))
    geometry=affinity.translate(geometry,xoff=style.side+style.stroke/2,yoff=style.stroke/2)
    if kind=='panic':
        # Genuine lowercase retains readability, with a lightly kinked stance.
        phase=(ord(ch)*11%9-4)/4
        geometry=affinity.skew(geometry,xs=phase*2.0,origin=(0,0))
        if ch.islower():
            geometry=affinity.rotate(geometry,phase*1.1,origin=(width/2,250))
    elif kind=='earth':
        geometry=affinity.skew(geometry,xs=-1.4,origin=(0,0))
    advance=round(width*style.width+style.stroke+style.side*2)
    if ch.isascii() and ch.isdigit():
        target=round(490*style.width+style.stroke+style.side*2)
        geometry=affinity.translate(geometry,xoff=(target-advance)/2)
        advance=target
    return geometry,advance


def set_version(path):
    font=TTFont(path,recalcTimestamp=False)
    font['head'].fontRevision=2.0
    if 'Hostile' in path.stem:
        font['OS/2'].sxHeight=700
    font['name'].setName('Version 2.000',5,3,1,0x409)
    font['name'].setName('Version 2.000',5,1,0,0)
    ps=font['name'].getDebugName(6)
    for platform,encoding,language in [(3,1,0x409),(1,0,0)]:
        font['name'].setName(f'2.000;HR26;{ps}',3,platform,encoding,language)
        font['name'].setName('Original invasion-media-referenced game typeface. See RESEARCH.md for visual analysis. All outlines are original.',10,platform,encoding,language)
    font.save(path)
    font.flavor='woff2'
    font.save(path.parents[1]/'woff2'/f'{path.stem}.woff2')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=DEFAULT)
    out=parser.parse_args().output.resolve()
    for name in ['ttf','woff2','preview','source','wordmarks']:
        (out/name).mkdir(parents=True,exist_ok=True)
    core.OUT=out
    core.make_base=make_base
    records=[]
    for style in STYLES:
        record=core.build(style)
        set_version(out/'ttf'/f'{record["file"]}.ttf')
        records.append(record)
    (out/'manifest.json').write_text(json.dumps({'version':'2.000','license':'MIT','fonts':records},indent=2)+'\n')
    css=[]
    for record in records:
        css.append("@font-face { font-family: '%s'; src: url('woff2/%s.woff2') format('woff2'); font-weight: %s; font-style: normal; font-display: swap; }"%(record['family'],record['file'],record['weight']))
    (out/'fonts.css').write_text('\n'.join(css)+'\n')
    print('Built six invasion edition TTF and WOFF2 pairs.')


if __name__=='__main__':
    main()
