# Third edition — the logo is the design source

The reference is the game logo supplied in this chat on 24 October 2026. Its
lettering has a heavy forward lean, broad cream faces, tight counters, soft
bowls, cut terminals and a comic sense of momentum. That is a different
construction from the jagged, upright invasion lettering in edition two.

## Building a usable font from the supplied lettering

The cream faces of H, U, M, A, N, R, E, S, O and C were separated from the coral
extrusion, illustration and background. Repeated letters allowed a regular R
to be distinguished from the initial R. The recovered contours were converted
to vectors, cleaned, normalized to a 700-unit cap height and regularized to a
common forward lean. The original reference is preserved unchanged under
`reference/`; the recovered vector outlines are in `source/logo-glyphs.json`.

B, D, F, G, I, J, K, L, P, Q, T, V, W, X, Y and Z were drawn to extend that
alphabet. Numbers, punctuation and accented characters complete its practical
coverage. The default A uses the logo's outside silhouette with a plain counter
and crossbar. The optional A uses a monochrome saucer/beam/star cutout. It does
not embed a full-colour illustration inside a TrueType glyph.

The font normalizes letter sizes and spacing so arbitrary words can be set.
It therefore does not reproduce the reference's individual letter scaling,
curved baseline or perspective automatically. Separate outlined SVG wordmarks
provide a rising two-line arrangement, coral offset and mint underline.

## The revised family

- **Abduction Logo Black Italic** is the main brand face. It preserves the logo's
  forward momentum and recognizable letter silhouettes. It is deliberately
  unicase: lowercase Latin input is displayed as poster capitals.
- **Abduction Compact Black Italic** is its narrower companion for results and
  short announcements. This width is purposefully related to the source logo,
  rather than introducing a competing visual identity.
- **Beam UI Medium, SemiBold and Bold** use open humanist curves, a double-storey
  a and clear numerals. They pair with the broad title faces without repeating
  the title's very tight counters at small sizes.
- **Field Lettering Regular** uses separately drawn, uneven letter skeletons
  and a tapered broad-pen construction for human signs. It is intended for
  short farm notices and roadside lettering, not menus or long descriptions.

The two Abduction fonts contain actual italic outlines and italic metadata.
Their `ss01` feature selects the saucer A. Their `ss02` feature selects the
initial R; apply it only to the desired character or span.

The source logo's navy, cream, coral and mint are demonstrated in the previews.
Colour, extrusion and underline remain layout treatments. Both logo fonts also
work as plain single-colour fonts, shown in the alphabet proofs.

Earlier alien-invasion media studies informed the context, but this edition's
letter shapes are anchored in the supplied brand artwork.
