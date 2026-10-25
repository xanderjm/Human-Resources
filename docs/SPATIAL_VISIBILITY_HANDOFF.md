# Playfield and projected visibility

The installed portrait client uses a 17 metre orthographic width. Player roots
are limited to ±5.65 metres, leaving the existing 0.85 metre core inside the
intended 13 metre central band. Forward/back movement remains ±6 metres. Enemy
ingress is 13 metres ahead of the runner and retirement is 11 metres behind it.
The source models, movement speeds, damage, capture and reward curves retain
their existing scale and rules. The landscape presentation keeps its 31 metre
camera. `runtime-spatial.json` owns runtime boundaries; product `scene.cpp` owns
the camera. Military integration owns the configuration and its save compatibility.

This corrects a structural mismatch: the previous ±8.25 metre player root bounds
extended beyond the scenery's ±6.5 metre clear band. Moving the camera alone would
have clipped installed orbit drones and left legitimate actors behind landmarks.
The previous 18 metre ingress also concealed a large tank behind a small phone's
HUD at the closer camera. The accepted width includes useful phone pixel margins
for actual installed equipment rather than checking the root point alone.

## Shared visibility rule

The product cooker emits `product_mesh_bounds.generated.h` from the final HRM
vertices for all cooked models. The generated header is a declared cooker
byproduct; it is never hand edited. Product visibility performs no runtime file
reads and imports no renderer or platform API.

`preserve_play_visibility` receives the exact scenery prefix, then protects actual
dynamic body, projectile and installed gear bounds from the same frame. Charged
shield geometry contributes one silhouette. Effect flourishes and renderer-owned
skeletal attachments do not invent extra critical bodies. Actual beam targets
protect their ground capture silhouette. Current committed warnings protect the
whole source-to-aim ground path. Typed infantry warnings additionally use the
authoritative source muzzle, pellet count and spread to protect every committed
ray, including both shotgun fan edges. Invalid near-body muzzle geometry creates
no fan. Attract screens omit stale run-specific warning and capture data.

Every tall opaque scenery type follows the same projected geometry rule, including
trees, rotors, barns, houses, cliffs, cacti and service canopies. Props whose highest
transformed point is below 0.6 metres remain ordinary ground dressing. Foreground
overlap smoothly lowers prop instance coverage to a bounded minimum of 0.12,
with a screen-space silhouette allowance and feather. Imported material and
critical object opacity are unchanged. This rule is spatial and stateless; pause,
resume and active gameplay outcomes are not inferred from cosmetic time.

Critical regions use a fixed array of 256 entries. An over-capacity public fixture
conservatively unions remaining regions instead of allocating or dropping safety
coverage. Camera matrices and source bounds supply projection/depth comparisons.

The existing shadow policy is retained: the renderer selects `casts_shadow`
independently of instance coverage. This avoids a moving threshold that abruptly
switches shadows. Native acceptance must judge protected ship, target, threat and
warning visibility with those shadows present; projected boxes or lower alpha
alone are insufficient evidence. This milestone adds no render pass or dither.

## Focused verification

`spatial_visibility_contracts` reads real cooked HRM vertices through the production
decoder. It verifies:

- legal six-slot rank III equipment, four aimed hull emitters, three aimed drones,
  two evolutions, 32 orbit/ring phases, both banks and all player extremes in all
  three themes at 320×568 and 390×844: at least six actual pixels at each side;
- complete boss hull/turret and missile geometry at ingress and retirement,
  16 headings and both dodge extremes, against the real UI with 20/0 and 59/34
  top/bottom safe areas: twelve pixels below the HUD and ten above Phase Drive;
- generated bounds for every tall scenery family, actual foreground barn/cliff
  protection for a projectile remote from the ship, and a committed warning path;
- continuous bounded coverage under small movement, and no invented protection
  in an empty scene;
- a cactus behind the outer committed shotgun lane which centre-path protection
  alone does not cover. The centre-only fixture stays above 0.8 opacity while
  the real typed fan reduces it below 0.3.

The optimized focused build and three projection/visibility suites pass. Logs are
`build/evidence/spatial-visibility-build.log` and
`build/evidence/spatial-visibility-focused.log`. These are portable geometry and
contract checks, not device, visual, fun or frame-time qualification. Independent
native QA owns the combined infantry/scenery/spatial film and themed extremity
proof after source freeze. Platform qualification follows that combined milestone.

## Integration seams

Military presentation appends its dynamic bodies, attached weapons and real
projectiles before the final visibility call, preserving the original scenery
prefix count. Its visible warning fan must use the same typed attack/muzzle
contract. New source assets automatically enter generated bounds through `MODELS`.
Scenery grouping keeps camera/boundaries fixed and preserves outward setbacks;
local silhouette overhangs remain protected by this shared helper.
