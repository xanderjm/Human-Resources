#!/usr/bin/env python3
"""Package the invasion edition, original sources, research and title wordmarks."""
import argparse
import hashlib
import shutil
import zipfile
from pathlib import Path

import uharfbuzz as hb
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

DEFAULT=Path(__file__).resolve().parents[1]/'assets/fonts/human-resources-v2'


def wordmarks(package):
    path=package/'ttf/HR-Hostile-Takeover-Black.ttf'
    font=TTFont(path);glyphs=font.getGlyphSet()
    face=hb.Face(path.read_bytes());hbfont=hb.Font(face);hbfont.scale=(1000,1000)
    lines=[]
    for text in ('HUMAN','RESOURCES'):
        buf=hb.Buffer();buf.add_str(text);buf.guess_segment_properties();hb.shape(hbfont,buf,{'kern':True})
        lines.append((sum(p.x_advance for p in buf.glyph_positions),buf))
    width=max(w for w,_ in lines);commands=[]
    for row,(w,buf) in enumerate(lines):
        factor=1.3 if row==0 else 1.0
        x=100+(width-w*factor)/2
        for info,pos in zip(buf.glyph_infos,buf.glyph_positions):
            relative=(x-100)/width
            baseline=790+row*850+60*(2*relative-1)**2
            pen=SVGPathPen(glyphs)
            transform=TransformPen(pen,(factor,0,0,-1,x+pos.x_offset,baseline-pos.y_offset))
            glyphs[font.getGlyphName(info.codepoint)].draw(transform)
            commands.append(pen.getCommands());x+=pos.x_advance*factor
    folder=package/'wordmarks';folder.mkdir(exist_ok=True)
    for name,color in [('Navy','#152F43'),('Ivory','#F8EED8')]:
        paths=''.join(f'<path d="{command}"/>' for command in commands)
        svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width+260} 1810" role="img" aria-labelledby="title"><title id="title">Human Resources</title><g fill="{color}">{paths}</g></svg>\n'
        (folder/f'Human-Resources-{name}.svg').write_text(svg)
    paths=''.join(f'<path d="{command}"/>' for command in commands)
    svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width+320} 1870" role="img" aria-labelledby="title"><title id="title">Human Resources invasion poster title</title><defs><g id="letters">{paths}</g></defs><use href="#letters" fill="#EB785E" stroke="#152F43" stroke-width="28" stroke-linejoin="round" transform="translate(42 62)"/><use href="#letters" fill="#F8EED8" stroke="#152F43" stroke-width="22" stroke-linejoin="round"/></svg>\n'
    (folder/'Human-Resources-Poster.svg').write_text(svg)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--package',type=Path,default=DEFAULT)
    package=parser.parse_args().package.resolve()
    wordmarks(package)
    source=package/'source';source.mkdir(exist_ok=True)
    names=('build_fonts.py','build_invasion_fonts.py','preview_invasion_fonts.py','validate_fonts.py','package_invasion_fonts.py')
    for name in names:
        origin=Path(__file__).resolve().parent/name;destination=source/name
        if origin.resolve()!=destination.resolve():
            shutil.copyfile(origin,destination)
    (source/'requirements.txt').write_text('fonttools==4.67.0\nbrotli==1.2.1\nshapely==2.2.0\npillow==13.0.0\nuharfbuzz==0.57.0\n')
    files=sorted(p for p in package.rglob('*') if p.is_file() and p.name!='SHA256SUMS.txt'
                 and '__pycache__' not in p.parts and not any(s.startswith('.') for s in p.relative_to(package).parts))
    (package/'SHA256SUMS.txt').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(package).as_posix()}\n' for p in files))
    files.append(package/'SHA256SUMS.txt')
    archive=package.parent/'Human-Resources-Invasion-Fonts-v2.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for path in files:
            z.write(path,Path('Human-Resources-Invasion-Fonts-v2')/path.relative_to(package))
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert sum(n.endswith('.ttf') for n in z.namelist())==6
        assert sum(n.endswith('.woff2') for n in z.namelist())==6
    print(f'{archive}\n{len(files)} files, {archive.stat().st_size:,} bytes. All CRC checks passed.')


if __name__=='__main__':
    main()
