#!/usr/bin/env python3
"""Render third-edition font specimens and neutral comparisons to edition two."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

DEFAULT=Path(__file__).resolve().parents[1]/'assets/fonts/human-resources-v3'
NAVY='#072A46';CREAM='#FFF1D1';CORAL='#EF7157';MINT='#65E8C6';PAPER='#F4F1E8';MUTED='#60747C'


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--package',type=Path,default=DEFAULT)
    parser.add_argument('--previous',type=Path,default=DEFAULT.parent/'human-resources-v2')
    args=parser.parse_args();p=args.package.resolve()
    logo='HR-Abduction-Logo-Black-Italic';compact='HR-Abduction-Compact-Black-Italic'
    ui='HR-Beam-UI-Medium';semi='HR-Beam-UI-SemiBold';bold='HR-Beam-UI-Bold';field='HR-Field-Lettering-Regular'
    def font(stem,size):
        return ImageFont.truetype(str(p/'ttf'/f'{stem}.ttf'),size)
    def drawtext(draw,x,y,s,stem,size,color,features=None):
        draw.text((x,y),s,font=font(stem,size),fill=color,anchor='lt',**({'features':features} if features else {}))
    im=Image.new('RGB',(1800,2220),PAPER);d=ImageDraw.Draw(im)
    d.rectangle((0,0,1800,640),fill=NAVY)
    drawtext(d,90,44,'Drawn from your logo / Third edition',semi,32,MINT)
    for x,y,text,size in [(80,135,'HUMAN',237),(80,365,'RESOURCES',154)]:
        drawtext(d,x+10,y+13,text,logo,size,CORAL)
        drawtext(d,x,y,text,logo,size,CREAM)
    drawtext(d,95,559,'Actual font files. The cream face is the font; colour and shadow are styling.',ui,25,CREAM)
    drawtext(d,90,688,'Abduction Logo / Black Italic',semi,31,MUTED)
    drawtext(d,85,759,'HUMANITY, ACQUIRED.',logo,83,NAVY)
    drawtext(d,90,864,'Recovered logo faces, completed alphabet, real italic outlines.',ui,28,NAVY)
    d.line((90,940,1710,940),fill='#C9CEC6',width=2)
    drawtext(d,90,984,'Abduction Compact / Black Italic',semi,31,MUTED)
    drawtext(d,85,1052,'ABDUCTION BONUS!',compact,91,NAVY)
    drawtext(d,90,1171,'A narrower companion for results, buttons and short announcements.',ui,28,NAVY)
    d.line((90,1246,1710,1246),fill='#C9CEC6',width=2)
    drawtext(d,90,1288,'Beam UI / Medium, SemiBold, Bold',semi,31,MUTED)
    drawtext(d,90,1358,'Choose an upgrade',bold,60,NAVY)
    drawtext(d,90,1446,'Increase your capture radius. Collect more specimens.',ui,32,NAVY)
    d.rounded_rectangle((90,1520,1710,1660),radius=25,fill=NAVY)
    drawtext(d,130,1564,'12,480',bold,48,CREAM)
    drawtext(d,650,1570,'LV 07',semi,36,CREAM)
    d.rounded_rectangle((890,1567,1400,1608),radius=19,fill='#375A6F')
    d.rounded_rectangle((890,1567,1240,1608),radius=19,fill=MINT)
    drawtext(d,1480,1570,'+250',bold,36,MINT)
    drawtext(d,90,1706,'0O 1Il   £ € $ ¥   Été · Grüße · Łódź · Český · Mañana',ui,31,NAVY)
    d.line((90,1790,1710,1790),fill='#C9CEC6',width=2)
    drawtext(d,90,1835,'Field Lettering / Regular',semi,31,MUTED)
    drawtext(d,90,1905,'Fresh milk. Strange lights.',field,68,NAVY)
    drawtext(d,90,2023,'An uneven broad pen for farm notices and roadside signs.',ui,28,NAVY)
    drawtext(d,90,2120,'Six TTF + WOFF2 styles / 374 characters each / Two optional logo glyphs',ui,25,MUTED)
    im.save(p/'preview/logo-edition-specimen.png')

    proof=Image.new('RGB',(1800,2200),PAPER);pd=ImageDraw.Draw(proof);y=40
    for stem in (logo,compact,ui,semi,bold,field):
        drawtext(pd,60,y,stem,semi,25,MUTED);y+=55
        for s in ('ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz','0123456789 £ € $ ¥ ! ? & %','ÀÁÂÃÄÅ Æ Ç ÈÉÊË ÌÍÎÏ Ñ ÒÓÔÕÖ Ø ÙÚÛÜ Ý ß'):
            drawtext(pd,60,y,s,stem,39,NAVY);y+=66
        y+=28
    proof.save(p/'preview/character-proof.png')

    alternate=Image.new('RGB',(1500,700),NAVY);ad=ImageDraw.Draw(alternate)
    drawtext(ad,55,40,'Plain A / Saucer A / Original initial R',semi,28,MINT)
    drawtext(ad,60,130,'A',logo,275,CREAM)
    drawtext(ad,420,130,'A',logo,275,CREAM,['ss01'])
    drawtext(ad,805,130,'R',logo,275,CREAM,['ss02'])
    drawtext(ad,55,450,'Alternate glyphs are optional; normal typing uses the plain forms.',ui,27,CREAM)
    drawtext(ad,55,522,'ss01: saucer and beam A     ss02: enlarged initial R',ui,27,CREAM)
    alternate.save(p/'preview/logo-alternates.png')

    brand=Image.new('RGB',(1600,850),NAVY);bd=ImageDraw.Draw(brand)
    drawtext(bd,65,40,'Logo-derived lettering / Wide + compact',semi,27,MINT)
    for x,y,s,size in [(55,125,'HUMAN',217),(55,330,'RESOURCES',141)]:
        drawtext(bd,x+8,y+12,s,logo,size,CORAL,['ss01'])
        drawtext(bd,x,y,s,logo,size,CREAM,['ss01'])
    drawtext(bd,65,560,'ABDUCTION BONUS!',compact,84,CREAM)
    drawtext(bd,65,715,'Original letter faces expanded into a usable font alphabet.',ui,28,MINT)
    brand.save(p/'preview/brand-lettering.png')

    small=Image.new('RGB',(1150,820),PAPER);sd=ImageDraw.Draw(small);y=35
    for size in (14,16,18,22,28):
        for stem in (ui,semi,bold):
            drawtext(sd,30,y,f'{size}px {stem.split("-")[-1]}  Choose an upgrade · 12,480 · LV 07 · 0O 1Il',stem,size,NAVY);y+=44
        y+=20
    small.save(p/'preview/ui-size-check.png')

    previous=args.previous.resolve()
    if previous.exists():
        compare=Image.new('RGB',(1800,1230),PAPER);cd=ImageDraw.Draw(compare)
        drawtext(cd,65,40,'Previous edition above. Logo edition below.',semi,34,NAVY)
        samples=[('Title','HR-Hostile-Takeover-Black',logo,'HUMAN RESOURCES'),
                 ('Announcements','HR-Panic-Broadcast-Bold',compact,'ABDUCTION BONUS!'),
                 ('Interface','HR-Bureau-Bold',bold,'Choose an upgrade'),
                 ('Earth signs','HR-Earthling-Signs-Regular',field,'Fresh milk. Strange lights.')]
        for row,(label,old,new,s) in enumerate(samples):
            y=135+row*260
            drawtext(cd,65,y,label,semi,25,MUTED)
            cd.text((65,y+55),s,font=ImageFont.truetype(str(previous/'ttf'/f'{old}.ttf'),65),fill=NAVY,anchor='lt')
            drawtext(cd,65,y+145,s,new,65,NAVY)
        compare.save(p/'preview/v2-v3-comparison.png')
    print('Rendered logo specimen, complete character proof, alternate glyphs, small sizes and comparison.')


if __name__=='__main__':
    main()
