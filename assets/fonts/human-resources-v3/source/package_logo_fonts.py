#!/usr/bin/env python3
"""Build the third-edition wordmarks and redistributable font ZIP."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path

import uharfbuzz as hb
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

DEFAULT=Path(__file__).resolve().parents[1]/'assets/fonts/human-resources-v3'


def wordmarks(package):
    path=package/'ttf/HR-Abduction-Logo-Black-Italic.ttf'
    font=TTFont(path);glyphs=font.getGlyphSet()
    face=hb.Face(path.read_bytes());hfont=hb.Font(face);hfont.scale=(1000,1000)
    rows=[]
    for text,scale in [('HUMAN',1.48),('RESOURCES',.91)]:
        buf=hb.Buffer();buf.add_str(text);buf.guess_segment_properties()
        hb.shape(hfont,buf,{'kern':True,'ss01':True})
        rows.append((sum(p.x_advance for p in buf.glyph_positions)*scale,scale,buf))
    width=max(w for w,_,_ in rows)+280
    commands=[]
    for row,(w,scale,buf) in enumerate(rows):
        x=120+(width-280-w)/2
        for info,pos in zip(buf.glyph_infos,buf.glyph_positions):
            u=(x-120)/max(w,1)
            baseline=(1250 if row==0 else 2170)-(245 if row==0 else 180)*u
            pen=SVGPathPen(glyphs)
            glyphs[font.getGlyphName(info.codepoint)].draw(TransformPen(pen,(scale,0,0,-scale,x+pos.x_offset*scale,baseline-pos.y_offset*scale)))
            commands.append(pen.getCommands())
            x+=pos.x_advance*scale
    folder=package/'wordmarks';folder.mkdir(exist_ok=True)
    paths=''.join(f'<path d="{p}"/>' for p in commands)
    for label,color in [('Navy','#072A46'),('Ivory','#FFF1D1')]:
        svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width+200:.0f} 2390" role="img" aria-labelledby="title"><title id="title">Human Resources</title><g fill="{color}">{paths}</g></svg>\n'
        (folder/f'Human-Resources-{label}.svg').write_text(svg)
    underline=f'M340 2330 C{width*.42:.0f} 2100 {width*.81:.0f} 2310 {width-110:.0f} 2160 C{width-10:.0f} 2110 {width-100:.0f} 2035 {width-150:.0f} 2040 C{width+105:.0f} 2010 {width+160:.0f} 2250 {width-20:.0f} 2290 C{width*.65:.0f} 2410 {width*.40:.0f} 2210 500 2420 Z'
    svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width+240:.0f} 2490" role="img" aria-labelledby="title"><title id="title">Human Resources logo edition</title><defs><g id="ink">{paths}</g></defs><path d="{underline}" fill="#65E8C6"/><use href="#ink" transform="translate(28 45)" fill="#EF7157"/><use href="#ink" fill="#FFF1D1"/></svg>\n'
    (folder/'Human-Resources-Brand.svg').write_text(svg)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--package',type=Path,default=DEFAULT)
    package=parser.parse_args().package.resolve();wordmarks(package)
    source=package/'source';source.mkdir(exist_ok=True)
    for name in ('build_fonts.py','extract_logo_glyphs.py','build_logo_fonts.py','preview_logo_fonts.py','validate_fonts.py','validate_logo_fonts.py','package_logo_fonts.py'):
        origin=Path(__file__).resolve().parent/name;destination=source/name
        if origin.resolve()!=destination.resolve():
            shutil.copyfile(origin,destination)
    (source/'requirements.txt').write_text('fonttools==4.67.0\nbrotli==1.2.1\nshapely==2.2.0\npillow==13.0.0\nuharfbuzz==0.57.0\nnumpy==2.5.4\n')
    files=sorted(p for p in package.rglob('*') if p.is_file() and p.name!='SHA256SUMS.txt'
                 and '__pycache__' not in p.parts and not any(s.startswith('.') for s in p.relative_to(package).parts))
    (package/'SHA256SUMS.txt').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(package).as_posix()}\n' for p in files))
    files.append(package/'SHA256SUMS.txt')
    archive=package.parent/'Human-Resources-Logo-Fonts-v3.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for path in files:
            z.write(path,Path('Human-Resources-Logo-Fonts-v3')/path.relative_to(package))
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert sum(n.endswith('.ttf') for n in z.namelist())==6
        assert sum(n.endswith('.woff2') for n in z.namelist())==6
    print(f'{archive}\n{len(files)} files; {archive.stat().st_size:,} bytes; CRC validation passed.')


if __name__=='__main__':
    main()
