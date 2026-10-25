# Human Resources typography

The owner approved the **logo edition, version 3.000**, as the starting point on
24 October 2026. Use this set for new game screens and artwork. Earlier editions
remain comparison material.

The source package is [`assets/fonts/human-resources-v3/`](../assets/fonts/human-resources-v3/README.md).
Its [specimen](../assets/fonts/human-resources-v3/preview/logo-edition-specimen.png)
and [design notes](../assets/fonts/human-resources-v3/DESIGN-NOTES.md) describe
the relationship to the supplied logo.

## Starting roles

| Role | Typeface | Font file |
| --- | --- | --- |
| Game title and large brand headings | HR Abduction Logo Black Italic | `HR-Abduction-Logo-Black-Italic.ttf` |
| Results and short announcements | HR Abduction Compact Black Italic | `HR-Abduction-Compact-Black-Italic.ttf` |
| Descriptions, tutorials and settings | HR Beam UI Medium | `HR-Beam-UI-Medium.ttf` |
| Buttons, labels and level counters | HR Beam UI SemiBold | `HR-Beam-UI-SemiBold.ttf` |
| Score and compact emphasis | HR Beam UI Bold | `HR-Beam-UI-Bold.ttf` |
| Farm notices and roadside signs | HR Field Lettering Regular | `HR-Field-Lettering-Regular.ttf` |

All files above are under `assets/fonts/human-resources-v3/ttf/`. Matching WOFF2
files and CSS are supplied for web previews. Choose the real weight and italic
file rather than applying synthetic bold or a second italic lean.

## Brand treatment

Use the logo's navy, cream, coral and mint as the starting palette. Large brand
lettering can use a cream face with a coral offset and a mint underline. These
are layout treatments; the fonts themselves are single-colour outlines.
Outlined SVG wordmarks are supplied in the package's `wordmarks/` directory.

Both Abduction faces intentionally use capitals for ASCII lowercase input.
Keep longer sentences and essential small text in Beam UI. Start title text at
48 screen pixels or above, Compact at 32 or above, Beam UI at 18–24, and Field
Lettering at 28 or above. Adjust after checking the actual device, layout and
viewing scale.

Use the optional `ss01` saucer A in the brand title where supported. Ordinary
headings use the plain A by default. The `ss02` original initial R is an optional
wordmark detail; apply it only to the intended character. Use the SVG wordmark
when the text renderer cannot select these alternates.

## HUD and implementation

Keep HUD numbers in Beam UI with its default tabular figures. Scores retain
their width when the digit count stays constant. Keep essential UI lettering
clean; apply painted surface treatment only to large signs or artwork.

This records the approved visual starting point. The native foundation does not
yet render these fonts. Select and implement measurement, shaping/rasterisation
and atlas handling through the existing UI/renderer boundaries during HUD work.
Then review small-size rendering, italic overhangs, accents, safe areas, text
expansion and missing-glyph handling on the target devices.

Promote only fonts actually consumed by that implementation into the runtime
allowlist, retaining the font licence. Font source scripts, previews, archives
and the reference artwork remain authoring assets. The current package covers
Latin text; additional writing systems require a coverage and fallback review.
