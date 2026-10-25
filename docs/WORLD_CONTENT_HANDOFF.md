# Three-location playable integration

The title now offers Farmland, Suburbia and Desert through one labelled location
control. Every location is immediately available; activating the control cycles
the backdrop before Start Run. Farmland is the default on a new app session.
The selected location survives retry/menu navigation in that session. It is
presentation state and changes neither gameplay/director tuning nor the schema-1
protected account. Pause and ordinary results identify the location; recovered
results omit it because the existing checkpoint deliberately does not store a
location.

`View.theme` and the optional final `scene` argument carry the existing portable
`SceneryTheme`. The app dispatches `location_next` only while on title. Accessible
semantics name the current and next location. The default scene argument keeps
existing consumers on Farmland.

Suburbia presents the biological human target with the accepted resident body and
nightcap, through the same stable animation identity and animated head attachment
path as the farmer/straw hat. Farmland and Desert retain the farmer. This visual
variation does not alter target rewards or runtime rules.

## Shipping boundary and cost

The explicit `assets/runtime/game-content.json` recipe selects 44 existing models,
six ground materials, six animation pairs and the two approved fonts. The cooker
validates the entire recipe, rejects unknown fields, omissions and extra models,
and retains exact source provenance. Only native cooked data ships; unused wood
terrain, authoring GLBs and the rest of the asset tree remain excluded. Shared
scenery materials preload once. All six skeletal pairs, including the resident,
prepare before rendering gameplay; there is no per-location first-use file read.

The actual optimized dev output on 25 October 2026 is 65 runtime files containing
93,098,029 bytes (88.8 MiB), bound to 73 source inputs. This expands the former
Farmland-only content checkpoint; it is an output inventory measurement, not an
APK compressed size or a physical-phone resident-memory budget. All three themes
share their common assets. Additional ground textures account for 12 MiB of the
increase; the remaining selected scenery/body meshes are needed by their scenes.

## Verification

The optimized dev target builds with strict warnings. Focused native suites pass:

- `world_content_contracts`: location selection labels and spoken values, no
  mid-run selector, default/cycle behavior, unique startup content IDs, actual
  cooked mesh/animation presence, all rendered meshes across fifteen streamed
  location windows (including section joins and 100 km), and resident/farmer
  animation plus matching headwear.
- `product_accessibility_contracts`: actual layout and control semantics across
  seventy-two small/wide phone screen combinations.
- `style_animation_preload_contracts`: all six real HRA/HRM pairs prepare before
  play, cached access survives removal of package files, and missing, malformed,
  oversized and mismatched pairs fail startup.

Three standard-library recipe regressions pass. The existing isolated actual
CMake/cooker producer-graph regression also passes after output deletion.

Native three-theme footage and final visual review follow the coordinated source
freeze. This pass does not claim physical-device performance, input, memory or
release qualification. No aggregate or mobile rebuild was repeated during
incomplete content integration.
