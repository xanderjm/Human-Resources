# Atomic Enamel native UI implementation

## Authority and completion

The owner-selected [hero](../art/ui/screen-suite-v1/hero-reference.png),
[screen gallery](../art/ui/screen-suite-v1/index.html) and
[component system](../art/ui/screen-suite-v1/component-system.json) define the
requested visual result. This task remains active until the native game uses a
reusable component library throughout Home and all twelve inventoried screens
and states, and actual native captures establish their layout/style fidelity.
Building the library or compiling a screen does not establish that result.

All values, eligible hardware, unlock milestones and evolution recipes must come
from the current gameplay/progression owners. The mockups' sample values and
older catalogue counts are not a second rules authority. Preserve actual credit
settlement, saving failures, input cancellation and accessibility behaviour.

## Component and asset ownership

- `native/human_resources_product/src/ui_canvas.h`: existing renderer-neutral
  measurement, layout coordinates, legacy glyphs and semantic hit geometry;
  extracted from `interface.cpp` before styling changes.
- `enamel_components.h/.cpp`: shared image/panel drawing, nine-slice frames,
  live text and heading treatment, primary/secondary actions, progress, rank
  pips, toggles and module icon mapping. Labels use an authored upright bitmap
  companion derived from the owned Abduction Compact outlines. Descriptions
  retain Beam UI SemiBold; display signs/actions retain the italic Abduction
  faces. Control identity and text do not live in sprite images.
- `enamel_screens.h/.cpp`: Home/Pause and composition routing;
  `enamel_menus.h/.cpp`: permanent progression, library, settings/privacy and
  recovery compositions; `enamel_flight_screens.h/.cpp`: promotion, grouped build
  and report compositions. All consume the same component library and live
  product projections.
  `enamel_hud.h/.cpp` owns the three tactical stat pods, passive sweep/Overdrive
  state, real input hints, and shared saucer Dash action on ordinary phones.
  `enamel_layout.h` reflows short portraits on the390point width grid rather
  than shrinking the entire tall composition. Tactical HUD framing retains its
  independent physical geometry. A compact heading reuses the riveted frame,
  live display text and aspect-preserved saucer/beam icon instead of flattening
  the large skewed sign into a short rectangle.
  The game is portrait only; the separate wide menu owner is removed.
  The measured reader and tactical strips in `interface.cpp` also consume those
  frames, fonts, icons and actions. Earlier independent standard/wide menu
  implementations are removed; missing space selects a measured shared page.
  This common ownership is implementation evidence, not final visual acceptance.
- `assets/graphics/atomic-ui/v1/`: original transparent generated surface, icon
  and title sheets; exact prompts and the crop/packing recipe. Six blank surface
  forms, the static title logo and36icons produce43reusable sprite regions.
  No live credit, stat, rank or action label is baked into those pieces.
- `tools/cook_ui_components.py`: checked non-overlapping packing, premultiplied
  alpha filtering, selected font glyphs, generated region/metric header and one
  2048×2048 HRM texture. Exact authoring inputs are included in runtime provenance.
  Generated headers and previews stay under `build/`.
- `ScreenQuad`: one64byte CPU/HLSL contract with legacy/enamel texture page,
  normalized affine edge offsets for live tilted headings, and explicit owned
  button-surface metadata. Renderer resource loading remains in the renderer.
  Legacy glyph atlas coordinates are preserved.

The additional cooked texture has16,777,216RGBA bytes plus its bounded HRM mesh
header. This is a known static texture cost, not a phone memory/performance
qualification. Original art, old assets and font sources are preserved; only
the explicit cooked UI output joins the shipping package.

## Screen acceptance matrix

| Screen/state | Current implementation | Remaining proof/work |
| --- | --- | --- |
| Home | Tall and compact portrait compositions; measured enlarged reader; primary action first in focus order | Final reference comparison and visual acceptance |
| Gameplay HUD | Portrait three-pod layout plus shared bounded enlarged strips; real feedback, cooldown, progression and input hints | Final cross-state visual comparison |
| Promotion | Shared tall and compact/dense cards; real three-offer draw and complete mechanics | More natural rarity/evolution draws; final visual comparison |
| Pause | Portrait compositions, measured reader | Final visual comparison |
| Current Build | Three themed inspection pages, full six-slot/seven-research cases, compact and measured variants | Final full-state visual comparison |
| Flight Report | Live receipt/goals, saved/pending/guest and Signal Lost variants; compact and measured layouts | More recovered/loss scenario comparison; final visual comparison |
| Hangar | Four live comparison/purchase cards across portrait phone sizes; shared measured reader | Further rank/cap visual comparison |
| Blueprint Library | Live three-item catalogue, complete requirements and shared portrait/reader layouts | Further unlock-state visual comparison |
| Evolutions | Five recipes; stable two-card tall or single-card compact/narrow spreads; contrasting ingredient labels | Final rank/ingredient-state comparison |
| Settings | Shared toggles/instructions across layouts and measured sizes | Final device-state visual comparison |
| Privacy | Six shared policy sections; real native paging and measured continuations | Final complete-policy visual comparison |
| Saved Run | Shared Pause and earned-choice notice across layouts | Final native comparison |
| Progress Protected | Shared blocking dialog/reader and exclusive recovery actions across layouts | More long actual-reason comparison; final visual comparison |

No screen is accepted as1:1 solely from this matrix. Record measured captures,
comparison findings, corrections and actual verification in the development log
and `build/evidence/atomic-ui-implementation-v1/`.

## Portrait-only scope

The26Octoberowner decision removes landscape gameplay and separate landscape
UI from this work. The mobile shells already lock portrait. Desktop previews
normalize startup requests into portrait, use a320×568minimum, and request
a3:4maximum aspect through SDL window constraints. Keep small/tall phones,
safe areas, enlarged text and portrait tablet geometry in scope. Historical
wide captures and low-level geometry stress tests remain diagnostic only.
They do not create a landscape support or visual acceptance requirement.

## Resizable frame contract

Rectangular enamel panels use nine slices. The source seams surround complete
rivets, and every fixed corner uses one pixel scale on both axes. A measured
source corner radius determines the requested destination radius; larger
content areas stretch only the middle strips and interior. At least one local
point remains for the center on a constrained panel. Capsules use three real
horizontal slices with end caps scaled from the full source height. They never
emit artificial zero-height source strips. Saucer actions and skewed title
art retain their authored silhouettes.

The source recipe records protected rivet bounds and rejects a slice crossing
them. Frame crops trim alpha below16 only when finding bounds, avoiding faint
remote pixels which previously inflated the content rectangle. Original art
and in-bounds alpha are retained. Capital-height metrics position live glyph
ink consistently without treating transparent bearings as content padding.

The measured reader uses the shared panel/action surfaces, display headings,
upright action labels and reward icons. Its line wrapping uses the same font
metrics as drawing. Explicit measured labels disable shrinking so the requested
system size survives physical-coordinate rendering. Complete text still pages;
transparent font-cell origins are not treated as visible ink bounds. Tests
inspect consumed alpha from both atlas pages at the requested size. Arrow
navigation reuses the existing icons, preserving the physical touch target at
large preferences. Ordinary portrait and measured pages all share the same visual
owners; final fidelity remains a separate review requirement.

Compact recipe spreads expose their actual count through
`Interface.recipe_page_count`; native next/previous commands consume it instead
of wrapping at a hardcoded three. It is disposable presentation metadata and
does not change accounts, saved runs or catalogue rules. Four comparison cards,
all purchase prices, seven research entries, earned promotion conditions and
recovery reasons remain live measured text. Large passive counters may use the
shared bounded suffix formatter; accessible account values remain complete.
Height-fitted narrow portraits choose single-card spreads before drawing any
recipe, so a longer effect cannot silently switch its navigation model.

## Label typography and tactical bounds

The original Beam Bold pass was too light for the reference's compact labels;
using italic Compact on ordinary controls also violated the upright-label
rule. The cooker now authors the label bitmap bank from Compact by reversing
the source builder's0.31outline shear once offline, keeping its black contours,
optical sidebearings and fixed31pixel digit advances at48pixels. It does not
change original font files, thicken strokes, or apply a runtime text transform.
Three actually consumed source font files remain allowlisted. Long mixed-case
descriptions continue using the readable SemiBold bank. Native comparisons of
both earlier treatments and the current result are retained in evidence.

The ordinary HUD uses physical-point top pods and the shared portrait grid for
its lower components. `Interface.hud_bottom` records the actual occupied header
boundary; installed receipts and attached enemy vitality clear it. HP/shield
show current/maximum when measured space permits; the current value and labelled
fraction remain at large values. XP likewise retains its labelled fraction
when a complete numeric pair cannot fit. Accessible values remain authoritative.
Changing input hints, sweep/Overdrive, damage or cooldown never changes the
action geometry or causes a fallback in the supported ordinary phone cases.

The bounded tactical variants preserve the existing44point strips and semantic
contacts. Cooked font metadata records the actual uppercase/digit/suffix ink
envelope; `tactical_label` centres that envelope without shrinking requested
type. Wider columns also show separate level/XP and score/time groups when
measured space permits. All remain inside the same strips. Source tests cover
the actual atlas alpha through3.4xtext, large values, damage/boost/installation,
offset viewports and narrower26/28point side insets, including labelled fraction
and stationary Dash alternatives. Camera, flight bounds and timing are unchanged.

The host-only native reviewer covers37named screen/state fixtures, including
Signal Lost, and accepts text-scale and explicit `phone`/`narrow` safe-area
profiles. Its real promotion comes from public Game seed514; all account
storage stays in memory. Source-bound captures and direct reference comparisons
live under `build/evidence/atomic-ui-implementation-v1/`. These static matrices
do not establish final1:1acceptance, human feel or physical-device qualification.

The isolated semantic-input playthrough now lasts320frames. It requires the
blueprint library in addition to the previous screen mask, traverses all three
Build pages and all six policy pages, and records every actual recipe spread.
Expected masks are495for screens,7for Build,63for privacy, and7or31for tall or
single-card recipes. It retains fresh explicit QA storage and never opens an
owner save. Native fixtures separately cover death/recovery states; the journey
does not claim to force a natural death or storage failure.

## Verification owners

- `tools/tests/test_ui_component_recipe.py`: production recipe validation,
  source existence, atlas bounds and sprite/font overlap rejection.
- `native/tests/enamel_component_tests.cpp`: shared geometry, texture page,
  pressure feedback, semantic identity, touch size and initial screen wiring.
- Existing portrait, accessibility, world-content and repository gates remain
  required. A newly textured button surface is distinguished from unchanged
  glyphs/icons in pressure tests; owned bounds and action invariants remain.
- `human_resources_ui_review`: explicitly built host-only native capture
  executable, excluded from default builds and packages. All fixture accounts
  are in memory. Its captures use the shipping UI, scene and SDL/Metal renderer;
  zero animation delta is explicitly static UI evidence, never motion proof.
- Required final evidence includes the aggregate repository gate, actual player
  interaction smoke, themed responsive/text-size/recovery coverage, GPU smoke
  and exact source/package provenance. Host proof does not qualify untested
  selected devices or storefronts.
