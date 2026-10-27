#!/usr/bin/env python3
"""Render the invasion edition and an honest same-colour comparison to v1."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

DEFAULT=Path(__file__).resolve().parents[1]/'assets/fonts/human-resources-v2'
NAVY='#152F43'; CREAM='#F8EED8'; MINT='#8DEBCC'; CORAL='#EB785E'; MUTED='#64706E'


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--package',type=Path,default=DEFAULT)
    parser.add_argument('--previous',type=Path,default=DEFAULT.parent/'human-resources')
    args=parser.parse_args();p=args.package.resolve()

    def f(stem,size):
        return ImageFont.truetype(str(p/'ttf'/f'{stem}.ttf'),size)

    ui='HR-Bureau-Medium';semi='HR-Bureau-SemiBold';bold='HR-Bureau-Bold'
    title='HR-Hostile-Takeover-Black';panic='HR-Panic-Broadcast-Bold';earth='HR-Earthling-Signs-Regular'
    im=Image.new('RGB',(1800,2200),CREAM);d=ImageDraw.Draw(im)

    def text(x,y,s,stem=ui,size=28,color=NAVY):
        d.text((x,y),s,font=f(stem,size),fill=color,anchor='lt')

    d.rectangle((0,0,1800,700),fill=NAVY)
    text(92,48,'Earth is now under new management.',semi,32,MINT)
    # A two-colour film-poster setting. The font outlines are themselves one-colour.
    for x,y,s,size in [(90,150,'HUMAN',215),(90,366,'RESOURCES',188)]:
        text(x+13,y+16,s,title,size,CORAL)
        text(x,y,s,title,size,CREAM)
    text(95,602,'Original invasion edition / 2.000 / Six usable font styles',ui,27,CREAM)
    d.ellipse((1430,80,1660,136),outline=MINT,width=6)
    d.arc((1490,36,1600,132),180,360,fill=MINT,width=6)
    d.line((1480,170,1430,270),fill=CORAL,width=6)
    d.line((1570,170,1550,280),fill=CORAL,width=6)
    d.line((1660,160,1705,250),fill=CORAL,width=6)

    text(90,748,'Hostile Takeover / Black',semi,30,MUTED)
    text(90,817,'HUMANITY, ACQUIRED.',title,91)
    text(90,931,'Poster wedges, uneven shoulders, slanted counters. Title is unicase.',ui,27)
    d.line((90,1005,1710,1005),fill='#BEC5B5',width=2)
    text(90,1048,'Panic Broadcast / Bold',semi,30,MUTED)
    text(90,1118,'ABDUCTION BONUS!',panic,91)
    text(90,1230,'Crooked comic capitals and matching angular lowercase.',ui,27)
    d.line((90,1305,1710,1305),fill='#BEC5B5',width=2)
    text(90,1348,'Bureau / Medium, SemiBold, Bold',semi,30,MUTED)
    text(90,1416,'Choose an upgrade',bold,61)
    text(90,1505,'Increase your capture radius. Personnel count: 12,480.',ui,32)
    d.rounded_rectangle((90,1580,1710,1715),radius=22,fill=NAVY)
    text(128,1622,'12,480',bold,49,CREAM)
    text(645,1629,'LV 07',semi,36,CREAM)
    d.rounded_rectangle((900,1626,1415,1667),radius=18,fill='#3C5667')
    d.rounded_rectangle((900,1626,1225,1667),radius=18,fill=MINT)
    text(1490,1629,'+250',bold,36,MINT)
    text(90,1760,'0O  1Il   £ € $ ¥   Été · Grüße · Łódź · Český · Mañana',ui,32)
    d.line((90,1840,1710,1840),fill='#BEC5B5',width=2)
    text(90,1881,'Earthling Signs / Regular',semi,30,MUTED)
    text(90,1950,'Fresh milk. Strange lights.',earth,66)
    text(90,2070,'TTF + WOFF2 / Original outlines / MIT licence / 374 characters per style',ui,25,MUTED)
    im.save(p/'preview/invasion-specimen.png')

    small=Image.new('RGB',(1150,820),CREAM);sd=ImageDraw.Draw(small);y=35
    for size in (14,16,18,22,28):
        for stem in (ui,semi,bold):
            sd.text((30,y),f'{size}px  {stem.split("-")[-1]}  Choose an upgrade · 12,480 · LV 07 · 0O 1Il',font=f(stem,size),fill=NAVY,anchor='lt');y+=44
        y+=20
    small.save(p/'preview/ui-size-check.png')

    # A full inventory reveals weak letters that a flattering headline can hide.
    inventory=Image.new('RGB',(1800,2150),CREAM);iv=ImageDraw.Draw(inventory);y=35
    for stem in (title,panic,ui,semi,bold,earth):
        iv.text((60,y),stem,font=f(semi,24),fill=MUTED,anchor='lt');y+=55
        for sample in ('ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz','0123456789   £ € $ ¥   ! ? & %','ÀÁÂÃÄÅ Æ Ç ÈÉÊË ÌÍÎÏ Ñ ÒÓÔÕÖ Ø ÙÚÛÜ Ý ß'):
            iv.text((60,y),sample,font=f(stem,43 if stem==title else 42),fill=NAVY,anchor='lt');y+=63
        y+=35
    inventory.save(p/'preview/character-proof.png')

    previous=args.previous.resolve()
    if previous.exists():
        compare=Image.new('RGB',(1800,1190),CREAM);cd=ImageDraw.Draw(compare)
        cd.text((70,45),'Same words. Same colours. New letterforms.',font=f(semi,34),fill=NAVY,anchor='lt')
        pairs=[('Title','HR-Personnel-Title-Black',title,'HUMAN RESOURCES'),
               ('Announcement','HR-Orbit-Display-Bold',panic,'ABDUCTION BONUS!'),
               ('Interface','HR-Field-UI-Bold',bold,'Choose an upgrade'),
               ('Earth signage','HR-Roadside-Regular',earth,'Fresh milk. Strange lights.')]
        for row,(label,old,new,sample) in enumerate(pairs):
            y=135+row*250
            cd.text((70,y),label+' / First edition above, invasion edition below',font=f(semi,24),fill=MUTED,anchor='lt')
            oldfont=ImageFont.truetype(str(previous/'ttf'/f'{old}.ttf'),65)
            cd.text((70,y+55),sample,font=oldfont,fill=NAVY,anchor='lt')
            cd.text((70,y+140),sample,font=f(new,65),fill=NAVY,anchor='lt')
        compare.save(p/'preview/v1-v2-comparison.png')
    print('Rendered invasion specimen, small-size check, full alphabets and comparison.')


if __name__=='__main__':
    main()
