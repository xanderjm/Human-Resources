#!/usr/bin/env python3
"""Check shipped font metadata, coverage, metrics, WOFF2 decoding and shaping."""
import argparse
import io
import json
import unicodedata
from pathlib import Path

from fontTools.ttLib import TTFont
import uharfbuzz as hb
from PIL import ImageFont

DEFAULT=Path(__file__).resolve().parents[1]/'assets/fonts/human-resources'


def shape(data,text,features=None):
    face=hb.Face(data)
    font=hb.Font(face)
    font.scale=(face.upem,face.upem)
    buffer=hb.Buffer()
    buffer.add_str(text)
    buffer.guess_segment_properties()
    hb.shape(font,buffer,features or {})
    return [(i.codepoint,p.x_advance,p.x_offset,p.y_offset)
            for i,p in zip(buffer.glyph_infos,buffer.glyph_positions)]


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--package',type=Path,default=DEFAULT)
    package=parser.parse_args().package.resolve()
    manifest=json.loads((package/'manifest.json').read_text())
    report=[]
    samples=['Human Resources','Research complete!','Choose an upgrade','BEAM LASER SHIELD',
             '12,480 LV 07 +250 0O 1Il','Été à la ferme','Grüße Łódź Český Mañana Smørrebrød',
             '£ € $ ¥ 25% × ÷ − → ← ↑ ↓ ✓','ÁÉÍÓÚ àèìòù äëïöü Ğğ İı Şş Œœ']
    for record in manifest['fonts']:
        path=package/'ttf'/f'{record["file"]}.ttf'
        data=path.read_bytes()
        font=TTFont(path,checkChecksums=2)
        for tag in font.keys():
            if tag!='GlyphOrder':
                font[tag]
        cmap=font.getBestCmap()
        assert len(cmap)==record['characters']
        assert len(set(font.getGlyphOrder()))==record['glyphs']
        assert all(cp in cmap for cp in range(32,127))
        assert all(cp in cmap for cp in range(0xA0,0x180))
        assert font['OS/2'].fsType==0
        assert font['OS/2'].usWeightClass==record['weight']
        for name_id in (1,2,4,6,16,17,13):
            assert font['name'].getDebugName(name_id), (path,name_id)
        ascent=font['hhea'].ascent
        descent=font['hhea'].descent
        glyph_count=0
        for name in font.getGlyphOrder():
            glyph=font['glyf'][name]
            if glyph.numberOfContours:
                glyph_count+=1
                assert glyph.yMax<=ascent,(path,name,'top clipping',glyph.yMax)
                assert glyph.yMin>=descent,(path,name,'bottom clipping',glyph.yMin)
                assert len(glyph.coordinates)>2
                assert font['hmtx'][name][1]==glyph.xMin,(name,'left bearing')
        digits=[font['hmtx'][cmap[ord(c)]][0] for c in '0123456789']
        assert len(set(digits))==1
        for sample in samples:
            shaped=shape(data,sample)
            assert all(item[0]!=0 for item in shaped),(path,sample)
            imagefont=ImageFont.truetype(str(path),18)
            assert imagefont.getmask(sample).getbbox() is not None
        assert sum(i[1] for i in shape(data,'AV',{'kern':True})) < sum(i[1] for i in shape(data,'AV',{'kern':False}))
        assert len({i[1] for i in shape(data,'0123456789',{'tnum':True})})==1
        assert len({i[1] for i in shape(data,'0123456789',{'pnum':True})})>1
        for text in ('ÁÉÍÓÚ','àèìòù','äëïöü','Čč Ğğ Ńń Œœ'):
            assert shape(data,text)==shape(data,unicodedata.normalize('NFD',text)),(path,text,'decomposition')
        web=TTFont(package/'woff2'/f'{record["file"]}.woff2')
        assert web.getBestCmap()==cmap
        assert web['hmtx'].metrics==font['hmtx'].metrics
        assert web.getGlyphOrder()==font.getGlyphOrder()
        for tag in ('glyf','GSUB','GPOS','OS/2','name'):
            assert web.getTableData(tag)==font.getTableData(tag),(path,tag,'WOFF2 mismatch')
        web.flavor=None
        buffer=io.BytesIO()
        web.save(buffer)
        assert shape(buffer.getvalue(),'Choose an upgrade · 12,480')==shape(data,'Choose an upgrade · 12,480')
        report.append({'font':record['file'],'status':'passed','characters':len(cmap),
                       'glyphs':record['glyphs'],'outline_glyphs':glyph_count,
                       'tabular_advance':digits[0], 'ttf_bytes':len(data),
                       'checks':['table checksums','ASCII / Latin-1 / Extended-A coverage','name and embedding metadata',
                                 'vertical bounds and bearings','tabular and proportional numbers','kerning',
                                 'NFC/NFD equivalence','HarfBuzz shaping','FreeType rendering','WOFF2 round-trip']})
    result={'status':'passed','fonts':report,'limitations':[
        'Fonts have no manual TrueType hinting; rendering uses the host rasterizer.',
        'Latin coverage only; other writing systems need separate fallback fonts.',
        'Engine-specific import and distance-field atlas settings require in-engine QA.'
    ]}
    (package/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':
    main()
