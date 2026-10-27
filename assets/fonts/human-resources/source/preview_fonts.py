#!/usr/bin/env python3
"""Render specimens with the actual shipped TTF files, using FreeType via Pillow."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

DEFAULT = Path(__file__).resolve().parents[1] / 'assets/fonts/human-resources'


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--package',type=Path,default=DEFAULT)
    package=parser.parse_args().package.resolve()
    navy='#152F43'; ivory='#F6F1E7'; mint='#8DEBCC'; coral='#EA7B65'; muted='#5F727C'
    image=Image.new('RGB',(1800,2130),ivory)
    draw=ImageDraw.Draw(image)

    def font(stem,size):
        return ImageFont.truetype(str(package/'ttf'/f'{stem}.ttf'),size)

    def text(x,y,s,stem='HR-Field-UI-Medium',size=26,fill=navy):
        draw.text((x,y),s,font=font(stem,size),fill=fill,anchor='lt')

    title='HR-Personnel-Title-Black'
    display='HR-Orbit-Display-Bold'
    medium='HR-Field-UI-Medium'
    semibold='HR-Field-UI-SemiBold'
    bold='HR-Field-UI-Bold'
    sign='HR-Roadside-Regular'
    draw.rectangle((0,0,1800,430),fill=navy)
    text(90,52,'Original type collection / Human Resources',size=25,fill=mint)
    text(80,126,'HUMAN',title,145,ivory)
    text(80,267,'RESOURCES',title,145,ivory)
    text(1130,160,'Personnel Title',semibold,32,mint)
    text(1130,220,'Black / 900',medium,25,ivory)
    text(1130,276,'For the title screen',medium,25,ivory)
    text(1130,322,'and promotional artwork.',medium,25,ivory)

    text(90,470,'Orbit Display / Bold',semibold,28,muted)
    text(85,532,'Research complete!',display,87)
    text(90,650,'Aa Bb Cc Dd Ee Ff Gg Hh Ii Jj Kk Ll Mm',display,42)
    text(90,715,'Nn Oo Pp Qq Rr Ss Tt Uu Vv Ww Xx Yy Zz',display,42)
    draw.line((90,800,1710,800),fill='#C7D0CD',width=2)

    text(90,844,'Field UI / Medium, SemiBold, Bold',semibold,28,muted)
    text(90,909,'Choose an upgrade',bold,58)
    text(90,995,'Increase your capture radius and collect more specimens.',medium,32)
    for x,label in [(90,'BEAM'),(620,'LASER'),(1150,'SHIELD')]:
        draw.rounded_rectangle((x,1070,x+475,1192),radius=24,fill=navy)
        text(x+42,1108,label,bold,42,ivory)
    draw.rounded_rectangle((90,1230,1710,1375),radius=35,fill=navy)
    draw.ellipse((129,1271,183,1325),fill='#F2BB63')
    text(210,1274,'12,480',bold,48,ivory)
    text(760,1281,'LV 07',semibold,35,ivory)
    draw.rounded_rectangle((1010,1280,1450,1320),radius=20,fill='#3B566A')
    draw.rounded_rectangle((1010,1280,1320,1320),radius=20,fill=mint)
    text(1510,1278,'+250',bold,37,mint)
    text(90,1418,'0123456789   0O  1Il   £ € $ ¥   + − × ÷ %',medium,38)
    text(90,1482,'Été à la ferme · Grüße · Mañana · Łódź · Český · Smørrebrød',medium,31)
    draw.line((90,1560,1710,1560),fill='#C7D0CD',width=2)

    text(90,1604,'Roadside / Regular',semibold,28,muted)
    text(90,1670,'Fresh milk. Strange lights.',sign,64)
    text(90,1770,'Roadside services · Open all night',sign,43)
    text(90,1875,'374 characters per style / Latin-1 + Latin Extended-A',medium,26,muted)
    text(90,1932,'TrueType for games and desktop / WOFF2 for web / MIT licence',medium,26,muted)
    text(90,2014,'Version 1.000 · Clean outlines; painted texture can be added in-game.',medium,24,muted)
    image.save(package/'preview/type-specimen.png')

    # Render essential UI text at real 1x pixel sizes, with no enlargement.
    small=Image.new('RGB',(1080,780),ivory)
    d=ImageDraw.Draw(small)
    y=30
    for size in (14,16,18,22,28):
        for stem in (medium,semibold,bold):
            f=font(stem,size)
            d.text((30,y),f'{size}px  {stem.split("-")[-1]}  Choose an upgrade · 12,480 · LV 07 · 0O 1Il',font=f,fill=navy,anchor='lt')
            y+=42
        y+=18
    small.save(package/'preview/ui-size-check.png')
    print(package/'preview/type-specimen.png')


if __name__=='__main__':
    main()
