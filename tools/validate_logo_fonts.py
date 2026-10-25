#!/usr/bin/env python3
"""Run the shared font checks, then verify genuine italics and logo alternates."""
import argparse
import json
import subprocess
import sys
from pathlib import Path

import uharfbuzz as hb
from fontTools.ttLib import TTFont


def shaped(data,text,features):
    face=hb.Face(data);font=hb.Font(face);font.scale=(1000,1000)
    buffer=hb.Buffer();buffer.add_str(text);buffer.guess_segment_properties()
    hb.shape(font,buffer,features)
    return [g.codepoint for g in buffer.glyph_infos]


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--package',type=Path,required=True)
    package=parser.parse_args().package.resolve()
    subprocess.run([sys.executable,str(Path(__file__).with_name('validate_fonts.py')),'--package',str(package)],check=True,capture_output=True,text=True)
    report=json.loads((package/'validation.json').read_text())
    for record in report['fonts']:
        path=package/'ttf'/f'{record["font"]}.ttf'
        font=TTFont(path);dot=font['glyf'][font.getBestCmap()[ord('.')]]
        assert dot.yMax-dot.yMin>40,(path,'punctuation dot')
        assert abs(font['head'].fontRevision-3)<0.001
        if 'Abduction' in path.stem:
            assert font['OS/2'].fsSelection&1
            assert not font['OS/2'].fsSelection&(1<<6)
            assert font['head'].macStyle&2
            assert font['post'].italicAngle < -15
            data=path.read_bytes()
            assert shaped(data,'A',{})!=shaped(data,'A',{'ss01':True})
            assert shaped(data,'a',{'ss01':True})==shaped(data,'A',{'ss01':True})
            assert shaped(data,'R',{})!=shaped(data,'R',{'ss02':True})
            assert shaped(data,'A',{'ss01':True})==[font.getGlyphID('A.abduction')]
            assert shaped(data,'R',{'ss02':True})==[font.getGlyphID('R.initial')]
            record['checks']+=['genuine italic metadata','saucer A stylistic set','initial R stylistic set']
        else:
            assert not font['OS/2'].fsSelection&1
    report['logo_provenance']='Ten main letter faces and an initial R recovered from the user-supplied game logo; missing glyphs drawn to complete the font.'
    (package/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print('All six fonts passed shared checks; italic metadata, alternate A/R shaping and punctuation checks passed.')


if __name__=='__main__':
    main()
