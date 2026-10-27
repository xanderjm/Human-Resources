#!/usr/bin/env python3
"""Create outlined wordmarks, bundle editable sources and archive the collection."""
import argparse
import hashlib
import shutil
import zipfile
from pathlib import Path

import uharfbuzz as hb
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

DEFAULT=Path(__file__).resolve().parents[1]/'assets/fonts/human-resources'


def wordmarks(package):
    path=package/'ttf/HR-Personnel-Title-Black.ttf'
    font=TTFont(path)
    glyphs=font.getGlyphSet()
    face=hb.Face(path.read_bytes())
    hbfont=hb.Font(face)
    hbfont.scale=(1000,1000)
    lines=[]
    for text in ('HUMAN','RESOURCES'):
        buf=hb.Buffer();buf.add_str(text);buf.guess_segment_properties()
        hb.shape(hbfont,buf,{'kern':True})
        lines.append((sum(p.x_advance for p in buf.glyph_positions),buf))
    width=max(w for w,_ in lines)
    commands=[]
    for index,(w,buf) in enumerate(lines):
        x=60
        baseline=760+index*840
        for info,pos in zip(buf.glyph_infos,buf.glyph_positions):
            pen=SVGPathPen(glyphs)
            transform=TransformPen(pen,(1,0,0,-1,x+pos.x_offset,baseline-pos.y_offset))
            glyphs[font.getGlyphName(info.codepoint)].draw(transform)
            commands.append(pen.getCommands())
            x+=pos.x_advance
    (package/'wordmarks').mkdir(exist_ok=True)
    for name,color in [('Navy','#152F43'),('Ivory','#F6F1E7')]:
        paths='\n'.join(f'<path d="{command}"/>' for command in commands)
        svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width+160} 1670" role="img" aria-labelledby="title"><title id="title">Human Resources</title><g fill="{color}">{paths}</g></svg>\n'
        (package/'wordmarks'/f'Human-Resources-{name}.svg').write_text(svg)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--package',type=Path,default=DEFAULT)
    package=parser.parse_args().package.resolve()
    wordmarks(package)
    source=package/'source'
    source.mkdir(exist_ok=True)
    for name in ('build_fonts.py','preview_fonts.py','validate_fonts.py','package_fonts.py'):
        origin=Path(__file__).resolve().parent/name
        destination=source/name
        if origin.resolve()!=destination.resolve():
            shutil.copyfile(origin,destination)
    (source/'requirements.txt').write_text('fonttools==4.67.0\nbrotli==1.2.1\nshapely==2.2.0\npillow==13.0.0\nuharfbuzz==0.57.0\n')
    allowed=lambda p: p.is_file() and '__pycache__' not in p.parts and not any(part.startswith('.') for part in p.relative_to(package).parts)
    files=sorted(p for p in package.rglob('*') if allowed(p) and p.name!='SHA256SUMS.txt')
    hashes='\n'.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(package).as_posix()}' for p in files)+'\n'
    (package/'SHA256SUMS.txt').write_text(hashes)
    archive=package.parent/'Human-Resources-Font-Pack-v1.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in [*files,package/'SHA256SUMS.txt']:
            z.write(p,Path('Human-Resources-Font-Pack-v1')/p.relative_to(package))
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert len(z.namelist())==len(files)+1
    print(f'{archive}\n{len(files)+1} files; {archive.stat().st_size:,} bytes; ZIP CRC check passed')


if __name__=='__main__':
    main()
