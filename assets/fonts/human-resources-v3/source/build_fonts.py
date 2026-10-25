#!/usr/bin/env python3
"""Build the original Human Resources type collection from vector skeletons.

No installed or third-party typeface is read by this generator.
Install source/requirements.txt, then run from any working directory.
"""
from __future__ import annotations

import json
import math
import unicodedata
import argparse
from dataclasses import dataclass
from pathlib import Path

from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.basePen import BasePen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.svgLib.path import parse_path
from fontTools.ttLib import TTFont
from shapely import affinity
from shapely.geometry import LineString, Point, Polygon, box
from shapely.geometry.polygon import orient
from shapely.ops import unary_union

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/fonts/human-resources'
EPOCH = 3875644800  # Fixed OpenType timestamp: 2026-10-24.

# Original centerline drawings. Coordinates use a 700-unit cap height.
DRAWINGS: dict[str, tuple[int, str]] = {
 'A':(520,'M0 0 L240 680 Q260 735 280 680 L520 0 M90 235 L430 235'),
 'B':(480,'M0 0 L0 700 L265 700 C540 700 550 370 275 370 L0 370 M275 370 C575 370 565 0 280 0 L0 0'),
 'C':(500,'M500 610 C425 735 170 750 60 600 C-20 490 -20 210 60 100 C170 -50 425 -35 500 90'),
 'D':(500,'M0 0 L0 700 L180 700 C430 700 500 560 500 350 C500 140 430 0 180 0 Z'),
 'E':(440,'M440 700 L0 700 L0 0 L440 0 M0 360 L370 360'),
 'F':(440,'M440 700 L0 700 L0 0 M0 360 L370 360'),
 'G':(520,'M505 610 C430 735 170 750 60 600 C-20 490 -20 210 60 100 C165 -45 420 -35 510 85 L510 330 L300 330'),
 'H':(500,'M0 0 L0 700 M500 0 L500 700 M0 355 L500 355'),
 'I':(260,'M0 700 L260 700 M130 700 L130 0 M0 0 L260 0'),
 'J':(400,'M400 700 L400 170 C400 -60 0 -65 0 160'),
 'K':(480,'M0 0 L0 700 M470 700 L0 230 M180 410 L480 0'),
 'L':(420,'M0 700 L0 0 L420 0'),
 'M':(620,'M0 0 L0 700 L310 230 L620 700 L620 0'),
 'N':(520,'M0 0 L0 700 L520 0 L520 700'),
 'O':(520,'M260 700 C65 700 0 580 0 350 C0 120 65 0 260 0 C455 0 520 120 520 350 C520 580 455 700 260 700 Z'),
 'P':(480,'M0 0 L0 700 L265 700 C560 700 560 330 265 330 L0 330'),
 'Q':(520,'M260 700 C65 700 0 580 0 350 C0 120 65 0 260 0 C455 0 520 120 520 350 C520 580 455 700 260 700 Z M320 170 L550 -85'),
 'R':(490,'M0 0 L0 700 L260 700 C555 700 555 350 260 350 L0 350 M240 350 L490 0'),
 'S':(480,'M470 610 C420 745 90 760 25 570 C-45 355 485 385 480 175 C475 -50 80 -65 0 95'),
 'T':(500,'M0 700 L500 700 M250 700 L250 0'),
 'U':(500,'M0 700 L0 220 C0 -75 500 -75 500 220 L500 700'),
 'V':(520,'M0 700 L260 0 L520 700'),
 'W':(740,'M0 700 L160 0 L370 500 L580 0 L740 700'),
 'X':(500,'M0 700 L500 0 M500 700 L0 0'),
 'Y':(520,'M0 700 L260 365 L520 700 M260 365 L260 0'),
 'Z':(480,'M0 700 L480 700 L0 0 L480 0'),
 'a':(400,'M400 0 L400 500 M400 385 C330 565 0 565 0 250 C0 -65 330 -65 400 115'),
 'b':(400,'M0 0 L0 720 M0 385 C70 565 400 565 400 250 C400 -65 70 -65 0 115'),
 'c':(380,'M380 435 C230 585 0 510 0 250 C0 -10 230 -85 380 65'),
 'd':(400,'M400 0 L400 720 M400 385 C330 565 0 565 0 250 C0 -65 330 -65 400 115'),
 'e':(400,'M0 255 L400 255 C415 590 0 590 0 255 C0 -10 250 -65 380 60'),
 'f':(250,'M80 0 L80 565 Q80 760 250 700 M0 480 L250 480'),
 'g':(400,'M400 500 L400 -40 C400 -240 110 -250 30 -155 M400 385 C330 565 0 565 0 250 C0 -65 330 -65 400 115'),
 'h':(400,'M0 0 L0 720 M0 345 C0 535 400 585 400 335 L400 0'),
 'i':(80,'M40 0 L40 500 M40 695 L40 700'),
 'j':(170,'M170 500 L170 -70 Q170 -240 0 -180 M170 695 L170 700'),
 'k':(380,'M0 0 L0 720 M370 500 L0 165 M145 295 L380 0'),
 'l':(150,'M0 720 L0 100 Q0 0 150 0'),
 'm':(650,'M0 0 L0 500 M0 345 C0 545 310 565 310 345 L310 0 M310 345 C310 545 650 565 650 345 L650 0'),
 'n':(400,'M0 0 L0 500 M0 345 C0 535 400 585 400 335 L400 0'),
 'o':(400,'M200 500 C50 500 0 410 0 250 C0 90 50 0 200 0 C350 0 400 90 400 250 C400 410 350 500 200 500 Z'),
 'p':(400,'M0 -210 L0 500 M0 385 C70 565 400 565 400 250 C400 -65 70 -65 0 115'),
 'q':(400,'M400 -210 L400 500 M400 385 C330 565 0 565 0 250 C0 -65 330 -65 400 115'),
 'r':(265,'M0 0 L0 500 M0 315 Q35 535 265 475'),
 's':(350,'M345 430 C270 545 0 525 0 370 C0 205 350 285 350 125 C350 -35 85 -40 0 60'),
 't':(260,'M90 660 L90 115 Q90 -35 260 30 M0 475 L260 475'),
 'u':(400,'M0 500 L0 165 C0 -85 400 -35 400 155 M400 500 L400 0'),
 'v':(400,'M0 500 L200 0 L400 500'),
 'w':(620,'M0 500 L130 0 L310 360 L490 0 L620 500'),
 'x':(380,'M0 500 L380 0 M380 500 L0 0'),
 'y':(400,'M0 500 L205 0 M400 500 L170 -100 Q115 -235 0 -180'),
 'z':(360,'M0 500 L360 500 L0 0 L360 0'),
 '0':(450,'M225 700 C50 700 0 580 0 350 C0 120 50 0 225 0 C400 0 450 120 450 350 C450 580 400 700 225 700 Z M75 90 L375 610'),
 '1':(330,'M0 550 L165 700 L165 0 M0 0 L330 0'),
 '2':(450,'M0 545 C0 780 450 780 450 550 C450 385 15 200 0 0 L450 0'),
 '3':(450,'M0 625 C95 750 440 745 440 530 Q440 350 200 350 M200 350 C540 370 540 -15 240 0 Q70 -10 0 95'),
 '4':(470,'M365 0 L365 700 L0 220 L470 220'),
 '5':(430,'M420 700 L35 700 L10 380 C100 445 430 475 430 210 C430 -40 95 -55 0 70'),
 '6':(450,'M420 650 C210 805 0 620 0 270 C0 -85 450 -85 450 200 C450 455 90 465 0 240'),
 '7':(440,'M0 700 L440 700 L100 0'),
 '8':(450,'M225 360 C-70 360 -65 700 225 700 C515 700 520 360 225 360 C-80 360 -80 0 225 0 C530 0 530 360 225 360 Z'),
 '9':(450,'M30 50 C240 -105 450 80 450 430 C450 785 0 785 0 500 C0 245 360 235 450 460'),
 '.':(80,'M40 5 L40 10'), ',':(100,'M75 20 Q75 -75 0 -115'),
 ':':(80,'M40 380 L40 385 M40 5 L40 10'), ';':(100,'M75 380 L75 385 M75 20 Q75 -75 0 -115'),
 '!':(80,'M40 700 L40 220 M40 5 L40 10'),
 '?':(380,'M0 560 C0 770 380 770 380 555 C380 390 190 410 190 220 M190 5 L190 10'),
 '-':(260,'M0 275 L260 275'), '_':(450,'M0 -145 L450 -145'),
 '+':(420,'M0 310 L420 310 M210 100 L210 520'),
 '=':(420,'M0 205 L420 205 M0 405 L420 405'),
 '/':(320,'M0 -80 L320 750'), '\\':(320,'M0 750 L320 -80'),
 '|':(0,'M0 -160 L0 760'),
 '(':(210,'M210 760 C-70 535 -70 165 210 -60'),
 ')':(210,'M0 760 C280 535 280 165 0 -60'),
 '[':(220,'M220 750 L0 750 L0 -60 L220 -60'),
 ']':(220,'M0 750 L220 750 L220 -60 L0 -60'),
 '{':(270,'M270 750 Q70 760 90 550 Q110 350 0 350 Q110 350 90 150 Q70 -60 270 -60'),
 '}':(270,'M0 750 Q200 760 180 550 Q160 350 270 350 Q160 350 180 150 Q200 -60 0 -60'),
 '<':(380,'M380 550 L0 300 L380 50'),
 '>':(380,'M0 550 L380 300 L0 50'),
 '"':(190,'M0 700 L0 530 M190 700 L190 530'),
 "'":(0,'M0 700 L0 530'),
 '`':(160,'M0 750 L160 620'), '^':(300,'M0 550 L150 710 L300 550'),
 '~':(440,'M0 300 C150 480 280 120 440 300'),
 '*':(350,'M175 700 L175 300 M0 600 L350 400 M0 400 L350 600'),
 '#':(460,'M120 0 L230 700 M330 0 L440 700 M0 230 L460 230 M30 470 L490 470'),
 '$':(480,'M470 610 C420 745 90 760 25 570 C-45 355 485 385 480 175 C475 -50 80 -65 0 95 M240 -95 L240 790'),
 '%':(620,'M0 0 L620 700 M110 700 C-35 700 -35 430 110 430 C255 430 255 700 110 700 Z M510 270 C365 270 365 0 510 0 C655 0 655 270 510 270 Z'),
 '&':(550,'M540 0 C420 110 -30 495 100 650 C230 805 450 665 320 510 C255 430 -60 290 20 100 C100 -90 480 -30 540 315'),
 '@':(700,'M470 195 L470 480 M470 390 C420 530 200 540 200 300 C200 70 425 105 470 255 M470 255 C470 30 700 150 700 365 C700 845 0 855 0 350 C0 -25 400 -95 610 40'),
 '£':(440,'M410 610 C320 800 60 695 90 515 L135 180 Q135 25 0 0 L440 0 M0 340 L320 340'),
 '€':(500,'M500 605 C340 795 30 710 30 350 C30 -10 340 -95 500 95 M0 440 L380 440 M0 260 L340 260'),
 '¥':(500,'M0 700 L250 360 L500 700 M250 360 L250 0 M40 310 L460 310 M40 160 L460 160'),
 '¢':(380,'M380 485 C230 635 0 560 0 300 C0 40 230 -35 380 115 M210 0 L210 620'),
 '¤':(420,'M0 0 L95 95 M325 425 L420 520 M0 520 L95 425 M325 95 L420 0 M210 440 C-20 440 -20 80 210 80 C440 80 440 440 210 440 Z'),
 'ß':(460,'M0 0 L0 520 C0 780 410 785 375 545 Q365 435 210 370 C565 395 555 -75 195 20'),
 'Æ':(740,'M0 0 L300 700 L740 700 M300 700 L300 0 L740 0 M300 355 L670 355 M100 235 L300 235'),
 'æ':(720,'M340 0 L340 500 M340 385 C270 565 0 565 0 250 C0 -65 270 -65 340 115 M340 255 L720 255 C735 590 340 590 340 255 C340 -10 580 -65 700 60'),
 'Œ':(800,'M400 700 L250 700 C60 700 0 580 0 350 C0 120 60 0 250 0 L400 0 L400 700 L800 700 M400 355 L740 355 M400 0 L800 0'),
 'œ':(740,'M185 500 C45 500 0 410 0 250 C0 90 45 0 185 0 C325 0 370 90 370 250 C370 410 325 500 185 500 Z M370 255 L740 255 C755 590 370 590 370 255 C370 -10 600 -65 720 60'),
 'Ø':(520,'M260 700 C65 700 0 580 0 350 C0 120 65 0 260 0 C455 0 520 120 520 350 C520 580 455 700 260 700 Z M-20 -20 L540 720'),
 'ø':(400,'M200 500 C50 500 0 410 0 250 C0 90 50 0 200 0 C350 0 400 90 400 250 C400 410 350 500 200 500 Z M-20 -20 L420 520'),
 'Đ':(510,'M40 0 L40 700 L200 700 C450 700 510 560 510 350 C510 140 450 0 200 0 Z M0 350 L290 350'),
 'đ':(440,'M400 0 L400 720 M400 385 C330 565 0 565 0 250 C0 -65 330 -65 400 115 M210 620 L480 620'),
 'Ð':(510,'M40 0 L40 700 L200 700 C450 700 510 560 510 350 C510 140 450 0 200 0 Z M0 350 L290 350'),
 'ð':(400,'M330 700 C600 285 430 0 200 0 C-60 0 -60 460 200 460 Q390 460 420 280 M180 605 L410 735'),
 'Þ':(480,'M0 0 L0 700 M0 570 L260 570 C555 570 555 180 260 180 L0 180'),
 'þ':(400,'M0 -210 L0 720 M0 385 C70 565 400 565 400 250 C400 -65 70 -65 0 115'),
 'Ł':(440,'M40 700 L40 0 L440 0 M0 250 L270 490'),
 'ł':(200,'M50 720 L50 100 Q50 0 200 0 M0 260 L200 440'),
 'ı':(80,'M40 0 L40 500'),
 'ſ':(250,'M80 0 L80 565 Q80 760 250 700'),
 'ĸ':(380,'M0 0 L0 500 M370 500 L0 165 M145 295 L380 0'),
 'Ŋ':(520,'M0 0 L0 700 L520 0 L520 700 M520 0 Q520 -235 320 -180'),
 'ŋ':(400,'M0 0 L0 500 M0 345 C0 535 400 585 400 335 L400 -70 Q400 -230 240 -180'),
 'ƒ':(300,'M0 -150 Q190 -225 190 0 L190 540 Q190 755 300 700 M70 430 L300 430'),
 'ẞ':(530,'M0 0 L0 700 L395 700 Q475 700 435 625 L260 360 C590 395 620 -40 210 20'),
 '–':(450,'M0 275 L450 275'), '—':(750,'M0 275 L750 275'),
 '‘':(100,'M0 530 Q0 625 90 700'), '’':(100,'M90 700 Q90 605 0 530'),
 '“':(260,'M0 530 Q0 625 90 700 M170 530 Q170 625 260 700'),
 '”':(260,'M90 700 Q90 605 0 530 M260 700 Q260 605 170 530'),
 '‚':(100,'M90 20 Q90 -75 0 -150'),
 '„':(260,'M90 20 Q90 -75 0 -150 M260 20 Q260 -75 170 -150'),
 '‹':(180,'M180 500 L0 300 L180 100'), '›':(180,'M0 500 L180 300 L0 100'),
 '«':(390,'M180 500 L0 300 L180 100 M390 500 L210 300 L390 100'),
 '»':(390,'M0 500 L180 300 L0 100 M210 500 L390 300 L210 100'),
 '•':(100,'M50 305 L50 310'), '·':(60,'M30 305 L30 310'),
 '…':(480,'M0 5 L0 10 M240 5 L240 10 M480 5 L480 10'),
 '×':(380,'M0 500 L380 120 M380 500 L0 120'),
 '÷':(420,'M0 310 L420 310 M210 100 L210 105 M210 515 L210 520'),
 '−':(420,'M0 310 L420 310'),
 '±':(420,'M0 380 L420 380 M210 170 L210 590 M0 0 L420 0'),
 '≈':(440,'M0 410 C150 590 280 230 440 410 M0 210 C150 390 280 30 440 210'),
 '≠':(420,'M0 205 L420 205 M0 405 L420 405 M80 50 L340 560'),
 '≤':(380,'M380 600 L0 350 L380 100 M0 -60 L380 -60'),
 '≥':(380,'M0 600 L380 350 L0 100 M0 -60 L380 -60'),
 '°':(200,'M100 700 C-35 700 -35 500 100 500 C235 500 235 700 100 700 Z'),
 '†':(350,'M175 -100 L175 720 M0 480 L350 480'),
 '‡':(350,'M175 -100 L175 720 M0 480 L350 480 M0 120 L350 120'),
 '←':(600,'M0 320 L600 320 M230 560 L0 320 L230 80'),
 '→':(600,'M0 320 L600 320 M370 560 L600 320 L370 80'),
 '↑':(480,'M240 0 L240 700 M0 460 L240 700 L480 460'),
 '↓':(480,'M240 0 L240 700 M0 240 L240 0 L480 240'),
 '✓':(480,'M0 280 L155 80 L480 600'),
 '∞':(700,'M350 300 C70 690 -150 50 130 100 C240 120 300 230 350 300 C630 690 850 50 570 100 C460 120 400 230 350 300 Z'),
 '¡':(80,'M40 0 L40 480 M40 695 L40 700'),
 '¿':(380,'M0 140 C0 -70 380 -70 380 145 C380 310 190 290 190 480 M190 695 L190 700'),
 '¬':(420,'M0 400 L420 400 L420 180'),
 '¦':(0,'M0 -100 L0 230 M0 430 L0 760'),
 '¶':(450,'M450 -120 L450 700 L200 700 C-65 700 -65 340 200 340 L240 340 M240 700 L240 -120'),
 '§':(380,'M370 640 C300 770 0 720 0 570 C0 420 380 410 380 250 C380 90 0 90 0 280 C0 440 380 420 380 80 C380 -70 80 -80 0 30'),
}

# Separate marks, also used to compose Latin-1 and Latin Extended-A letters.
MARKS = {
 '\u0300':(240,'M40 160 L200 40'),
 '\u0301':(240,'M40 40 L200 160'),
 '\u0302':(300,'M0 35 L150 170 L300 35'),
 '\u0303':(320,'M0 70 C110 220 210 -35 320 110'),
 '\u0304':(300,'M0 90 L300 90'),
 '\u0306':(300,'M0 150 Q150 -50 300 150'),
 '\u0307':(90,'M45 90 L45 95'),
 '\u0308':(270,'M25 90 L25 95 M245 90 L245 95'),
 '\u030A':(200,'M100 175 C-20 175 -20 0 100 0 C220 0 220 175 100 175 Z'),
 '\u030B':(330,'M0 35 L130 170 M200 35 L330 170'),
 '\u030C':(300,'M0 170 L150 35 L300 170'),
 '\u0327':(180,'M100 0 L40 -95 Q240 -75 160 -190 Q100 -250 0 -205'),
 '\u0328':(180,'M160 0 C-100 -160 70 -260 180 -175'),
}


class FlattenPen(BasePen):
    def __init__(self):
        super().__init__(None)
        self.paths: list[list[tuple[float, float]]] = []
        self.current: list[tuple[float, float]] = []

    def _moveTo(self, p):
        if self.current:
            self.paths.append(self.current)
        self.current = [p]

    def _lineTo(self, p):
        self.current.append(p)

    def _curveToOne(self, a, b, c):
        start = self.current[-1]
        length = sum(math.dist(p, q) for p, q in zip((start, a, b), (a, b, c)))
        steps = max(8, math.ceil(length / 12))
        for i in range(1, steps + 1):
            t = i / steps
            u = 1-t
            self.current.append(tuple(u**3*start[j]+3*u*u*t*a[j]+3*u*t*t*b[j]+t**3*c[j] for j in (0, 1)))

    def _qCurveToOne(self, control, end):
        start = self.current[-1]
        self._curveToOne(tuple(start[j]+2/3*(control[j]-start[j]) for j in (0,1)),
                         tuple(end[j]+2/3*(control[j]-end[j]) for j in (0,1)), end)

    def _closePath(self):
        self.current.append(self.current[0])
        self._endPath()

    def _endPath(self):
        if self.current:
            self.paths.append(self.current)
            self.current = []


@dataclass(frozen=True)
class Style:
    family: str
    style: str
    weight: int
    stroke: int
    width: float = 1.0
    side: int = 48
    character: str = 'ui'

STYLES = [
    Style('HR Field UI', 'Medium', 500, 64),
    Style('HR Field UI', 'SemiBold', 600, 78),
    Style('HR Field UI', 'Bold', 700, 92),
    Style('HR Orbit Display', 'Bold', 700, 128, 1.06, 44, 'display'),
    Style('HR Personnel Title', 'Black', 900, 175, 1.04, 38, 'title'),
    Style('HR Roadside', 'Regular', 400, 91, .94, 55, 'sign'),
]


def glyph_name(ch):
    return f'uni{ord(ch):04X}'


def shape_path(path, stroke):
    pen = FlattenPen()
    parse_path(path, pen)
    pen._endPath()
    return unary_union([LineString(p).buffer(stroke/2, quad_segs=5) if len(set(p)) > 1
                        else Point(p[0]).buffer(stroke/2, quad_segs=8) for p in pen.paths])


def transform_outline(shape, width, style, ch):
    shape = affinity.scale(shape, xfact=style.width, yfact=1-style.stroke/700, origin=(0, 0))
    shape = affinity.translate(shape, xoff=style.side+style.stroke/2, yoff=style.stroke/2)
    if style.character == 'title':
        # Chunky, forward-reaching terminals without distress or decorative cuts.
        shape = affinity.skew(shape, xs=3.2, origin=(0,0))
    elif style.character == 'sign':
        # Deterministic hand-lettered lean, preserving the counters.
        phase = (ord(ch)*17 % 13 - 6) / 6
        shape = affinity.skew(shape, xs=5.0+phase*1.1, origin=(0,0))
        shape = affinity.rotate(shape, phase*.7, origin=(width/2, 300))
    return shape


def make_base(ch, style):
    width, path = DRAWINGS[ch]
    # Broad display faces receive an undivided zero; UI retains a slash.
    if ch == '0' and style.character != 'ui':
        path = path.split(' M75')[0]
    stroke = style.stroke
    shape = shape_path(path, stroke)
    shape = transform_outline(shape, width, style, ch)
    advance = round(width*style.width + stroke + style.side*2)
    if ch.isascii() and ch.isdigit():
        target = round(490*style.width + stroke + style.side*2)
        shape = affinity.translate(shape, xoff=(target-advance)/2)
        advance = target
    return shape, advance


def mark_shape(ch, base, style, advance):
    width, path = MARKS[ch]
    stroke = min(style.stroke*.76, 92)
    shape = shape_path(path, stroke)
    shape = affinity.scale(shape, xfact=style.width, origin=(0,0))
    center = advance/2
    if ch in ('\u0327', '\u0328'):
        y = -5
        if ch == '\u0328':
            center = advance*.74
    else:
        # Dots and marks remain separated from stems, even in the title face.
        y = base.bounds[3]+72
    return affinity.translate(shape, xoff=center-width*style.width/2, yoff=y)


def geometry_to_glyph(shape):
    pen = TTGlyphPen(None)
    shape = shape.simplify(.65, preserve_topology=True)
    if shape.is_empty:
        return pen.glyph()
    polygons = [shape] if isinstance(shape, Polygon) else list(shape.geoms)
    for polygon in polygons:
        polygon = orient(polygon, sign=-1.0)
        for ring in [polygon.exterior, *polygon.interiors]:
            points = []
            for x,y in list(ring.coords)[:-1]:
                p = (round(x), round(y))
                if not points or p != points[-1]:
                    points.append(p)
            if len(set(points)) < 3:
                continue
            pen.moveTo(points[0])
            for p in points[1:]:
                pen.lineTo(p)
            pen.closePath()
    return pen.glyph()


def build(style):
    bases = {ch: make_base(ch, style) for ch in DRAWINGS}
    geometries = dict(bases)
    # Canonical accents with specially drawn bases for barred/ligated letters.
    for cp in list(range(0xA0, 0x180)) + [0x1E9E]:
        ch = chr(cp)
        if ch in geometries:
            continue
        decomposition = unicodedata.normalize('NFD', ch)
        if len(decomposition) > 1 and decomposition[0] in bases and all(m in MARKS for m in decomposition[1:]):
            base_ch = 'ı' if decomposition[0] == 'i' else decomposition[0]
            shape, advance = bases[base_ch]
            # Czech/Slovak carons on tall lowercase letters use a side mark.
            for mark in decomposition[1:]:
                if ch in 'ďľť' and mark == '\u030C':
                    small = shape_path('M0 110 L-35 -20', min(style.stroke*.65,65))
                    small = affinity.translate(small, xoff=advance-15, yoff=610)
                    shape = unary_union([shape,small])
                    advance += 75
                else:
                    shape = unary_union([shape,mark_shape(mark,shape,style,advance)])
            geometries[ch] = (shape, advance)

    # Additional barred letters and Latin-1 micro sign.
    for ch, source, crossbar in [
        ('Ħ','H','M-20 550 L520 550'),('ħ','h','M-30 610 L240 610'),
        ('Ŧ','T','M80 350 L420 350'),('ŧ','t','M0 270 L250 270'),
        ('Ŀ','L','M310 360 L310 365'),('ŀ','l','M240 360 L240 365')]:
        shape,advance=bases[source]
        bar=transform_outline(shape_path(crossbar,style.stroke),advance,style,ch)
        geometries[ch]=(unary_union([shape,bar]),advance+(125 if ch=='ŀ' else 0))
    micro=transform_outline(shape_path('M0 -200 L0 500 M0 165 C0 -85 400 -35 400 155 M400 500 L400 0',style.stroke),400,style,'µ')
    geometries['µ']=(micro,bases['u'][1])

    # Traditional IJ ligatures and a curved apostrophe-n.
    for ch, a, b in [('Ĳ','I','J'),('ĳ','i','j')]:
        ga, wa = bases[a]
        gb, wb = bases[b]
        geometries[ch] = (unary_union([ga,affinity.translate(gb,xoff=wa-30)]),wa+wb-30)
    gn, wn = bases['n']
    apost, wa = bases['’']
    geometries['ŉ'] = (unary_union([apost,affinity.translate(gn,xoff=wa)]),wa+wn)

    # Spacing accents map to visible spacing glyphs; combining marks have zero advance.
    for spacing, mark in [('´','\u0301'),('¨','\u0308'),('¯','\u0304'),('¸','\u0327'),('˘','\u0306'),('ˇ','\u030C'),('˚','\u030A'),('˛','\u0328'),('˝','\u030B'),('˙','\u0307')]:
        anchor = box(0,0,200,540)
        geometries[spacing] = (mark_shape(mark,anchor,style,350),350)
    for ch in MARKS:
        anchor = box(0,0,200,0)
        # Mark outlines are centered on x=0 for GPOS anchors.
        geometries[ch] = (mark_shape(ch,anchor,style,0),0)

    # Smaller readable legal marks, ordinal indicators, fractions, superscripts.
    for ch, source in [('ª','a'),('º','o'),('¹','1'),('²','2'),('³','3')]:
        g, a = bases[source]
        geometries[ch] = (affinity.translate(affinity.scale(g,xfact=.58,yfact=.58,origin=(0,0)),yoff=370),round(a*.58))
    for ch,a,b in [('¼','1','4'),('½','1','2'),('¾','3','4')]:
        ga,wa=bases[a]; gb,wb=bases[b]
        first=affinity.translate(affinity.scale(ga,xfact=.5,yfact=.5,origin=(0,0)),yoff=350)
        second=affinity.translate(affinity.scale(gb,xfact=.5,yfact=.5,origin=(0,0)),xoff=420)
        slash=transform_outline(shape_path('M230 0 L490 700',style.stroke*.65),700,style,'/')
        geometries[ch]=(unary_union([first,second,slash]),round(440+wb*.5))
    for ch, source in [('©','C'),('®','R')]:
        g,a=bases[source]
        g=affinity.scale(g,xfact=.45,yfact=.45,origin=(0,0))
        g=affinity.translate(g,xoff=320-a*.225,yoff=190)
        ring=Point(320,350).buffer(310,quad_segs=32).difference(Point(320,350).buffer(275,quad_segs=32))
        geometries[ch]=(unary_union([g,ring]),640)
    for ch, text in [('™','TM'),('℠','SM')]:
        a,aw=bases[text[0]]; b,bw=bases[text[1]]
        shape=unary_union([a,affinity.translate(b,xoff=aw)])
        geometries[ch]=(affinity.translate(affinity.scale(shape,xfact=.45,yfact=.45,origin=(0,0)),yoff=400),round((aw+bw)*.45))

    space=round(285*style.width)
    for ch,width in [(' ',space),('\u00a0',space),('\u2002',500),('\u2003',1000),('\u2009',180),('\u202f',180),('\u200b',0),('\u2060',0),('\u00ad',0)]:
        geometries[ch]=(Polygon(),width)

    missing=[f'U+{cp:04X} {chr(cp)}' for cp in range(32,127) if chr(cp) not in geometries]
    missing += [f'U+{cp:04X} {chr(cp)}' for cp in range(0xA0,0x180) if chr(cp) not in geometries and cp != 0xAD]
    if missing:
        raise ValueError(f'Missing required characters: {missing}')

    cmap={ord(ch):glyph_name(ch) for ch in geometries}
    order=['.notdef',*sorted(cmap.values())]
    fb=FontBuilder(1000,isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    glyphs={'.notdef':geometry_to_glyph(box(70,0,510,700).difference(box(130,60,450,640)))}
    metrics={'.notdef':(580,70)}
    for ch,(shape,advance) in geometries.items():
        name=glyph_name(ch)
        glyph=geometry_to_glyph(shape)
        glyph.recalcBounds(None)
        glyphs[name]=glyph
        metrics[name]=(advance,glyph.xMin if glyph.numberOfContours else 0)
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=1100,descent=-320,lineGap=0)
    full=f'{style.family} {style.style}'
    ps=full.replace(' ','').replace('-','')
    legacy_family=style.family if style.style in ('Regular','Bold') else full
    legacy_style=style.style if style.style in ('Regular','Bold') else 'Regular'
    fb.setupNameTable({
        'familyName':legacy_family,'styleName':legacy_style,
        'typographicFamily':style.family,'typographicSubfamily':style.style,
        'uniqueFontIdentifier':f'1.000;HR26;{ps}',
        'fullName':full,'psName':ps,'version':'Version 1.000',
        'copyright':'Copyright (c) 2026 Human Resources contributors',
        'manufacturer':'Human Resources',
        'designer':'Original vector type design created for Human Resources',
        'description':'Original game typeface. Latin-1, Latin Extended-A, tabular numerals, kerning and combining marks.',
        'licenseDescription':'MIT License. Commercial use, embedding, modification and redistribution permitted. See LICENSE.txt.',
    })
    is_bold=style.style=='Bold'
    fb.setupOS2(version=4,sTypoAscender=1100,sTypoDescender=-320,sTypoLineGap=0,
                usWinAscent=1100,usWinDescent=320,usWeightClass=style.weight,
                fsType=0,fsSelection=(1<<7)|(1<<5 if is_bold else 1<<6),
                sxHeight=round(500*(1-style.stroke/700)+style.stroke),sCapHeight=700,
                achVendID='HR26')
    fb.setupPost()
    fb.setupMaxp()
    fb.font['head'].macStyle=1 if is_bold else 0
    fb.font['head'].created=EPOCH
    fb.font['head'].modified=EPOCH
    fb.font.recalcTimestamp=False

    pairs={'AV':-65,'AW':-45,'AY':-65,'AT':-40,'VA':-65,'WA':-45,'YA':-65,
           'TA':-45,'To':-55,'Te':-55,'Ta':-50,'Tu':-40,'Ty':-35,'Yo':-50,
           'Ye':-50,'Ya':-50,'Vo':-45,'Va':-40,'Wo':-25,'Wa':-25,'LT':-35,
           'LY':-45,'FA':-35,'Fo':-25,'Pa':-25,'P.':-65,'T.':-55,'V.':-50,
           'Y.':-60,'r.':-30,'f.':-20}
    fea=['languagesystem DFLT dflt;','languagesystem latn dflt;', 'feature kern {']
    for pair,value in pairs.items():
        fea.append(f'pos {glyph_name(pair[0])} {glyph_name(pair[1])} {round(value*style.width)};')
    fea.append('} kern;')
    # Glyph insertion also extends glyf.glyphOrder; keep our order independent.
    order=list(order)
    # Default figures are tabular, making HUD behavior independent of feature support.
    for digit in '0123456789':
        name=glyph_name(digit)+'.pnum'
        original,tab_advance=bases[digit]
        width=DRAWINGS[digit][0]
        natural=round(width*style.width+style.stroke+style.side*2)
        glyphs[name]=geometry_to_glyph(affinity.translate(original,xoff=(natural-tab_advance)/2))
        g=glyphs[name]; g.recalcBounds(fb.font['glyf'])
        fb.font['glyf'][name]=g
        fb.font['hmtx'][name]=(natural,g.xMin)
        order.append(name)
    fb.font.setGlyphOrder(order)
    fea.append('feature pnum {')
    for digit in '0123456789':
        n=glyph_name(digit)
        fea.append(f'sub {n} by {n}.pnum;')
    fea.append('} pnum;')
    fea.append('feature tnum {')
    for digit in '0123456789':
        n=glyph_name(digit)
        fea.append(f'sub {n}.pnum by {n};')
    fea.append('} tnum;')
    # Mark-to-base positioning enables decomposed accented input as well as NFC.
    for mark in MARKS:
        below=mark in ('\u0327','\u0328')
        fea.append(f'markClass {glyph_name(mark)} <anchor 0 {0 if below else 72}> @MC{"Below" if below else "Above"};')
    fea.append('feature mark {')
    for ch in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz':
        shape,adv=bases[ch]
        fea.append(f'pos base {glyph_name(ch)} <anchor {adv//2} {round(shape.bounds[3])+72}> mark @MCAbove <anchor {adv//2} -5> mark @MCBelow;')
    fea.append('} mark;')
    fea.append('feature ccmp {')
    for ch in geometries:
        d=unicodedata.normalize('NFD',ch)
        if len(d)==2 and d[0] in bases and d[1] in MARKS:
            fea.append(f'sub {glyph_name(d[0])} {glyph_name(d[1])} by {glyph_name(ch)};')
    fea.append('} ccmp;')
    addOpenTypeFeaturesFromString(fb.font,'\n'.join(fea))
    stem=full.replace(' ','-')
    ttf=OUT/'ttf'/f'{stem}.ttf'
    fb.save(ttf)
    web=TTFont(ttf,recalcTimestamp=False)
    web.flavor='woff2'
    web.save(OUT/'woff2'/f'{stem}.woff2')
    return {'family':style.family,'style':style.style,'weight':style.weight,
            'file':stem,'characters':len(cmap),'glyphs':len(order),
            'codepoints':[f'U+{cp:04X}' for cp in sorted(cmap)]}


def main():
    global OUT
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=OUT)
    OUT=parser.parse_args().output.resolve()
    for directory in ['ttf','woff2','source','preview']:
        (OUT/directory).mkdir(parents=True,exist_ok=True)
    records=[build(style) for style in STYLES]
    (OUT/'manifest.json').write_text(json.dumps({'version':'1.000','license':'MIT','fonts':records},indent=2)+'\n')
    css=[]
    for record in records:
        css.append("@font-face {\n  font-family: '%s';\n  src: url('woff2/%s.woff2') format('woff2');\n  font-weight: %s;\n  font-style: normal;\n  font-display: swap;\n}" % (record['family'],record['file'],record['weight']))
    (OUT/'fonts.css').write_text('\n\n'.join(css)+'\n')
    print(json.dumps([{k:v for k,v in r.items() if k!='codepoints'} for r in records],indent=2))


if __name__=='__main__':
    main()
