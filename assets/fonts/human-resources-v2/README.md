# Human Resources — Invasion edition

Version 2.000. Four original type families, six static styles, supplied as real
TrueType OpenType fonts (`ttf/`) and matching web fonts (`woff2/`). The original
version 1 pack remains separate and unchanged.

Start with `preview/invasion-specimen.png` and `preview/v1-v2-comparison.png`.
Open `specimen.html` for an editable preview. Read `RESEARCH.md` for the alien
invasion media references and their specific influence on the new drawings.

## Fonts and intended use

| Family | Style / weight | Role | Starting screen size |
| --- | --- | --- | --- |
| HR Hostile Takeover | Black / 900 | Title and major promotional headings | 48 px and above |
| HR Panic Broadcast | Bold / 700 | Short announcements, capture bonuses | 30 px and above |
| HR Bureau | Medium / 500 | Descriptions and settings | 18–24 px |
| HR Bureau | SemiBold / 600 | Buttons and labels | 18–28 px |
| HR Bureau | Bold / 700 | Score and compact headings | 22–36 px |
| HR Earthling Signs | Regular / 400 | Environmental signs | 28 px and above |

Hostile Takeover is deliberately **unicase**: ASCII lowercase input uses the
same poster capitals as uppercase input. This is a display alphabet, not a
body-text face. The other families have distinct uppercase and lowercase.
Use the supplied weight files instead of synthetic bold. Pixel sizes are
starting points; review at the game's final physical size and display scale.

## Contents

- `ttf/` and `woff2/`: six styles each.
- `fonts.css`: correctly named web font declarations.
- `specimen.html`: editable samples, size control and references.
- `preview/invasion-specimen.png`: type in a title and HUD setting.
- `preview/v1-v2-comparison.png`: unchanged colours and words for comparison.
- `preview/character-proof.png`: all alphabets, digits and representative accents.
- `preview/ui-size-check.png`: Bureau rendered at actual 14–28 px sizes.
- `wordmarks/`: transparent outlined SVG title assets, with solid and poster variants.
- `source/`: original vector drawings, compiler, rendering, validation and packaging scripts.
- `manifest.json`: exact Unicode inventory and font metadata.
- `validation.json`: results of font integrity and rendering checks.
- `RESEARCH.md`: linked reference analysis.
- `LICENSE.txt`: MIT licence for the original fonts and package assets.
- `SHA256SUMS.txt`: integrity hashes.

## Game import

Import the required `.ttf` files through the engine's font importer. Generate
bitmap/SDF/MSDF atlases with the actual localized strings, or use dynamic
glyph generation if available. Clean vector outlines work as source assets for
these pipelines; paint texture and colour effects belong in the material or
layout. The fonts themselves are single-colour and do not contain the optional
coral shadow shown in the poster specimen.

Numerals are tabular by default, so a changing score holds its width when its
digit count is unchanged. OpenType features are `kern`, `pnum`, `tnum`, `ccmp`
and `mark`. A renderer that does not support shaping still gets precomposed
accented glyphs and tabular digits, but loses kerning, alternate figures and
combining-mark positioning. Use NFC-normalized text for such a renderer.

## Web use

Keep `fonts.css` beside `woff2/`, then load it:

```html
<link rel="stylesheet" href="fonts.css">
```

```css
body { font-family: 'HR Bureau', sans-serif; font-weight: 500; }
button { font-family: 'HR Bureau', sans-serif; font-weight: 600; }
.score { font-family: 'HR Bureau', sans-serif; font-weight: 700;
         font-variant-numeric: tabular-nums; }
.announcement { font-family: 'HR Panic Broadcast', sans-serif; font-weight: 700; }
.title { font-family: 'HR Hostile Takeover', sans-serif; font-weight: 900; }
.earth-sign { font-family: 'HR Earthling Signs', serif; font-weight: 400; }
```

## Coverage and limits

Each style has 374 mapped Unicode characters and 385 glyphs. Printable ASCII,
Latin-1 Supplement and Latin Extended-A are complete. Selected combining accents,
smart punctuation, common currencies, arithmetic signs, arrows and a check mark
are included. Other writing systems require fallback fonts. Combining anchors
support the unaccented Latin alphabet; arbitrary stacked accents are not covered.
Blank soft hyphens leave discretionary insertion to the layout application.

There is no manual TrueType hinting. Small-size rendering depends on the
rasterizer. Use Bureau SemiBold or Bold for small essential text. The title's
narrow, oblique counters are intentional at large sizes.

UPM: 1000. Nominal cap height: 700 with display overshoots. Shared line metrics:
1100/-320 with zero line gap. Start body text at line-height 1.45; adjust display
line spacing only after checking accents and descenders.

The report checks table integrity, coverage, naming/embedding metadata,
vertical bounds, bearings, kerning, tabular/proportional figures, HarfBuzz shaping,
NFC/NFD equivalence, FreeType rendering and WOFF2 parity. The PNG alphabets and
specimens were inspected. Final engine/device rendering and atlas settings
still need in-game review.

## Rebuild

From this package directory, with Python 3.11+:

```sh
python3 -m venv .font-build
.font-build/bin/python -m pip install -r source/requirements.txt
.font-build/bin/python source/build_invasion_fonts.py --output .
.font-build/bin/python source/preview_invasion_fonts.py --package .
.font-build/bin/python source/validate_fonts.py --package .
.font-build/bin/python source/package_invasion_fonts.py --package .
```

`source/build_fonts.py` is the shared first-edition Unicode/OpenType compiler
required by the new builder; it does not read existing font files. An optional
`--previous /path/to/v1` on the preview script regenerates the comparison image.
The packaging command creates a ZIP beside this directory.

## Licence

MIT. Commercial use, embedding, modification and redistribution are permitted.
Include `LICENSE.txt` in redistributions. No visible in-game attribution is
required. Reference artwork is linked only and is not part of the licence.
