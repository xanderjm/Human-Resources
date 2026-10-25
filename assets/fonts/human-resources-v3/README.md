# Human Resources — logo edition

Version 3.000. Six real font styles as TTF and WOFF2, including two fonts based
on the logo supplied with this request. Earlier editions remain separate.

Start with `preview/logo-edition-specimen.png`, then open `specimen.html` for
editable text and the original logo reference. `DESIGN-NOTES.md` explains which
letters came from the logo and how the alphabet was completed.

## Included fonts

| Font | Weight and style | Intended use |
| --- | --- | --- |
| HR Abduction Logo | Black Italic / 900 | Title, marketing, large brand headings |
| HR Abduction Compact | Black Italic / 900 | Results and short announcements |
| HR Beam UI Medium | Upright / 500 | Descriptions, settings, tutorials |
| HR Beam UI SemiBold | Upright / 600 | Buttons and labels |
| HR Beam UI Bold | Upright / 700 | Score and compact headings |
| HR Field Lettering | Regular / 400 | Farm notices and roadside signs |

The two logo fonts are **unicase**: ordinary ASCII lowercase input displays as
capitals. Beam UI and Field Lettering have distinct uppercase and lowercase.
The fonts are static; there are no variable axes. Abduction Compact is supplied
as its own family to make selection straightforward in game engines.

Start logo text at 48 px or above, Compact at 32 px or above, Beam UI at 18–24 px
and Field Lettering at 28 px or above. Prefer SemiBold or Bold for small
essential UI text. Sizes refer to the final screen scale and require in-game
review. Select the actual weight file instead of applying synthetic bold.

## Files

- `ttf/`: six installable TrueType OpenType font files for game and desktop use.
- `woff2/` and `fonts.css`: equivalent compressed web fonts and declarations.
- `specimen.html`: editable samples, sizing, alternate A/R controls and reference.
- `preview/logo-edition-specimen.png`: the complete font system in context.
- `preview/character-proof.png`: alphabets, figures, punctuation and accents.
- `preview/logo-alternates.png`: plain and optional A/R glyphs.
- `preview/ui-size-check.png`: actual 14–28 px UI samples.
- `preview/v2-v3-comparison.png`: neutral comparison with the previous edition.
- `wordmarks/`: outlined SVG titles in navy, ivory and a coral/mint brand setting.
- `reference/logo-reference.png`: the supplied artwork, unchanged.
- `source/`: original font sources, recovered glyph vectors, build and checks.
- `manifest.json`, `validation.json`, `SHA256SUMS.txt`: coverage, results and hashes.

## Game use

Import the required `.ttf` files through your engine's font importer. Use these
as source fonts when generating bitmap/SDF/MSDF atlases. Include the actual
localized strings in a static atlas, or use dynamic glyph generation where
available. Preview final rendering, material settings, spacing and clipping in
the actual engine and on the target device.

The Abduction fonts already lean forward: do not apply a second synthetic
italic. Their ink can extend beyond a glyph's advance width, as is typical for
italic lettering. Give display text sufficient right padding.

Glyphs are single-colour outlines. Cream faces, coral extrusion and mint
underlines are separate styling, demonstrated by the SVG wordmarks. The font
does not automatically curve a line or resize individual letters to match the
original image's composition.

All digits are tabular by default. Scores hold their width when the digit
count stays constant. `pnum` provides proportional figures, while `tnum`
restores tabular figures. `kern`, `ccmp` and `mark` provide kerning and accent
composition/positioning. NFC-normalize strings if the game renderer does not
support text shaping; precomposed accented letters remain usable.

## Logo alternates

Both Abduction fonts contain:

- `ss01`: saucer-and-beam A, with a star-shaped cutout.
- `ss02`: the original enlarged initial R. Apply to the intended initial only;
  enabling it for an entire paragraph replaces every ASCII R/r in that span.

These are OpenType stylistic sets, not private-use Unicode characters. An
engine must support the feature or direct glyph access to use them. Ordinary
typing uses the standard A and R. The SVG wordmarks contain their own outlines
and need no font installed.

## Web use

```html
<link rel="stylesheet" href="fonts.css">
```

```css
.title { font-family: 'HR Abduction Logo', sans-serif;
         font-weight: 900; font-style: italic; }
.announcement { font-family: 'HR Abduction Compact', sans-serif;
                font-weight: 900; font-style: italic; }
.saucer-a { font-feature-settings: 'ss01' 1; }
.initial-r { font-feature-settings: 'ss02' 1; }
body { font-family: 'HR Beam UI', sans-serif; font-weight: 500; }
button { font-family: 'HR Beam UI', sans-serif; font-weight: 600; }
.score { font-family: 'HR Beam UI', sans-serif; font-weight: 700;
         font-variant-numeric: tabular-nums; }
.sign { font-family: 'HR Field Lettering', sans-serif; font-weight: 400; }
```

## Coverage, metrics and validation

Each style maps 374 Unicode characters. The logo fonts have 387 glyphs; the
others have 385. Printable ASCII, Latin-1 Supplement and Latin Extended-A are
complete, with selected combining marks, punctuation, currencies, arrows and
arithmetic symbols. The exact inventory is in `manifest.json`. Other writing
systems require fallback fonts. Arbitrary stacked accents are not supported.

UPM: 1000. Cap height: nominally 700, with optical/hand-lettered overshoots.
Shared ascender/descender: 1100/-320, no additional line gap. Start body text at
line-height 1.45, and check accents before reducing display line spacing.

There are no manual TrueType hinting instructions. Rendering at small sizes
depends on the rasterizer. The logo reference is raster artwork; contour
reconstruction retains some of its drawn irregularity while normalizing it
into reusable type. This is not a claim of exact perspective or pixel matching.

Validation covers table checksums, declared coverage, naming, weight/embedding
metadata, glyph bounds and bearings, default/alternate figures, kerning,
HarfBuzz shaping, NFC/NFD equivalence, FreeType rendering and WOFF2 round-trip.
Additional checks verify true italic flags, the slant angle and both stylistic
sets. The PNG proofs were visually inspected. Engine/device QA remains separate.

## Rebuild

From this package directory, with Python 3.11+:

```sh
python3 -m venv .font-build
.font-build/bin/python -m pip install -r source/requirements.txt
.font-build/bin/python source/build_logo_fonts.py --output .
.font-build/bin/python source/preview_logo_fonts.py --package .
.font-build/bin/python source/validate_logo_fonts.py --package .
.font-build/bin/python source/package_logo_fonts.py --package .
```

The build reads the bundled `source/logo-glyphs.json`. Recreate that source only
if desired, using the included reference-specific extractor:

```sh
.font-build/bin/python source/extract_logo_glyphs.py \
  --image reference/logo-reference.png --output source/logo-glyphs.json
```

The extractor is specific to the supplied 1024×1536 image. It is not a general
OCR or font-identification tool. The optional `--previous /path/to/v2` argument
on the preview script rebuilds the comparison image. `source/build_fonts.py`
is the shared Unicode/OpenType compiler used by the new builder.

## Licence

The font software, source scripts and new type specimens are supplied under
MIT, permitting commercial use, embedding, modification and redistribution.
Include `LICENSE.txt` when redistributing. The supplied reference artwork
retains its existing rights and is not relicensed by this package.
