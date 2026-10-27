# Human Resources font collection

Version 1.000 — 24 October 2026

Six original static OpenType fonts, delivered as TrueType (.ttf) for game engines
and desktop software, and WOFF2 (.woff2) for websites. These contain real vector
outlines, Unicode character maps, naming and weight metadata, OpenType layout
tables and embedding permissions. No external font installation is needed to
use them in a project. No third-party font outlines are included or modified.

## Choose a font

| Font | Weight | Intended role | Suggested starting size |
| --- | --- | --- | --- |
| HR Field UI Medium | 500 | Descriptions, tutorial text, settings | 18–24 screen pixels |
| HR Field UI SemiBold | 600 | Buttons, labels, level counters | 18–28 screen pixels |
| HR Field UI Bold | 700 | Score, key actions, compact headings | 22–36 screen pixels |
| HR Orbit Display Bold | 700 | Level-up, results, menu headings | 32–72 screen pixels |
| HR Personnel Title Black | 900 | Game title and promotional headlines | 48 screen pixels and above |
| HR Roadside Regular | 400 | Environmental signs, short painted captions | 28 screen pixels and above |

Sizes are starting points at 1x display scale. Test final physical size, device
scale, background and movement. Prefer SemiBold or Bold for compact essential
text. Keep long passages in Field UI. Personnel Title is intentionally heavy;
small counters and accents are designed for large display use.

Orbit is broad, rounded and upright. Field shares its basic letter shapes with
lighter strokes and open spacing. Personnel pushes the weight and forward lean
for the title. Roadside uses a mild, deterministic variation in lean and angle
for painted signage. The family deliberately keeps a shared visual vocabulary.

## Files

- `ttf/`: six installable, engine-importable TrueType fonts.
- `woff2/`: matching compressed web fonts.
- `fonts.css`: ready-to-use web font declarations with correct weight mapping.
- `specimen.html`: editable preview, size control, and number-spacing control.
- `preview/type-specimen.png`: rendered overview using the shipped TTF files.
- `preview/ui-size-check.png`: UI weights rendered at actual 14–28 px sizes.
- `wordmarks/`: outlined, transparent SVG title lockups in navy and ivory.
- `source/`: original glyph generator, rendering, packaging and validation scripts.
- `manifest.json`: font metadata and exact supported Unicode codepoints.
- `validation.json`: automated checks and their scope.
- `SHA256SUMS.txt`: hashes for every other package file.

## Use in a game

Copy the required files from `ttf/` into the project's font assets and import
them through the engine's font importer. Select the actual weight file for
each role instead of applying synthetic bold. Keep the TTF files as the source
when generating a bitmap, SDF or MSDF atlas. Add the project's real localized
strings to an atlas, or use dynamic glyph generation if the engine supports it.

Default digits are tabular in every font: changing a score does not change its
width when the digit count stays the same. Field UI uses a slashed zero and a
barred capital I, with a hooked lowercase l. Display, title and sign faces use
an unslashed zero. Kerning is supplied through GPOS; a renderer that ignores
OpenType layout will still show text and tabular digits, but will lose kerning,
alternate figures and combining-mark positioning. The precomposed accented
letters work independently of shaping. Use NFC-normalized text when a game
renderer does not provide shaping support.

The outlines are clean, without baked paint texture. Apply optional paint or
surface treatment to large title/sign text in the renderer. Keep essential UI
text clean. A sign font does not replace an actual sign mesh or texture.

## Use on the web

Keep `fonts.css` alongside the `woff2` directory and load it:

```html
<link rel="stylesheet" href="fonts.css">
```

```css
body { font-family: 'HR Field UI', sans-serif; font-weight: 500; }
button { font-family: 'HR Field UI', sans-serif; font-weight: 600; }
.score { font-family: 'HR Field UI', sans-serif; font-weight: 700;
         font-variant-numeric: tabular-nums; }
h2 { font-family: 'HR Orbit Display', sans-serif; font-weight: 700; }
.title { font-family: 'HR Personnel Title', sans-serif; font-weight: 900; }
.sign { font-family: 'HR Roadside', sans-serif; font-weight: 400; }
```

Use `font-variant-numeric: proportional-nums` for proportional digits in prose.
Open `specimen.html` directly, or serve this directory with a local HTTP server.
The specimen reports font loading and provides a character inventory.

## Coverage and layout

Each style maps 374 Unicode characters to 385 glyphs including `.notdef` and
alternate digits. Coverage includes printable ASCII, Latin-1 Supplement,
Latin Extended-A, selected combining accents, smart punctuation, common currency
symbols, arithmetic signs, arrows and a check mark. The complete set is in
`manifest.json`. Zero-width space, word joiner and soft hyphen are deliberately
blank. Discretionary hyphen insertion is the layout application's responsibility.

Features: `kern`, `pnum`, `tnum`, `ccmp`, and `mark`. Default digits are tabular.
All styles include uppercase and genuine lowercase drawings. Supported combining
marks have anchors on the unaccented Latin alphabet; arbitrary stacked combining
marks are not supported. Vietnamese Extended Additional, Greek, Cyrillic,
Arabic, Hebrew, Indic and East Asian scripts need fallback fonts or new designs.

Units per em: 1000. Cap height: 700. Shared ascender/descender: 1100/-320, with
zero line gap. The generous vertical metrics accommodate the accents. A starting
line-height of 1.45 is appropriate for body copy; shorten display line spacing
only after checking accents and descenders. There are no manual TrueType hinting
instructions. Small-size rasterization depends on the host's rasterizer.

## Validation

The supplied report checks all six TTF/WOFF2 pairs for table integrity, complete
declared Latin coverage, naming/weight/embedding metadata, bounds, bearings,
equal-width digits, proportional alternates, kerning, HarfBuzz shaping,
NFC/NFD accent equivalence, FreeType rasterization and WOFF2 decoding parity.
The PNG specimens were visually inspected. Final game-engine atlas generation,
device rendering and localization layouts still need review in the game.

## Rebuild

Python 3.11+ is recommended. From this package directory:

```sh
python3 -m venv .font-build
.font-build/bin/python -m pip install -r source/requirements.txt
.font-build/bin/python source/build_fonts.py --output .
.font-build/bin/python source/preview_fonts.py --package .
.font-build/bin/python source/validate_fonts.py --package .
.font-build/bin/python source/package_fonts.py --package .
```

The last command rebuilds the SVG wordmarks, hashes, and ZIP beside this directory.
The generator contains the original vector drawings and deterministic style
parameters. It never reads an installed typeface. These are static fonts; there
are no variable weight axes or italic files.

## License

MIT. Commercial use, game and app embedding, modification, redistribution and
marketing use are permitted. Include `LICENSE.txt` with redistributions. No
attribution is required in the visible game interface. See the actual license
for its terms.
