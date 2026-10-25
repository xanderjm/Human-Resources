# Product UI Foundation

`human_resources_ui` is the dependency-free layout, interaction-geometry, focus, and semantic model for shipping UI. It knows nothing about SDL, GPU resources, font implementations, audio, gameplay, or native accessibility APIs.

## Ownership

The module owns:

- row, column, stack, grid, scroll-list, and leaf layout;
- points, percentages, weighted space, intrinsic measurement, constraints, padding, gaps, aspect ratio, and alignment;
- safe-content insets at screen roots, including mobile cutouts and console/TV safe regions supplied by platform policy;
- left-to-right and right-to-left ordering;
- design tokens for spacing, type scale, interaction geometry, colour, and motion;
- stable node IDs, semantic roles, labels, values, disabled state, focus order, and accessibility-tree extraction;
- configurable interaction-target validation, clipping-aware hit testing, and renderer-neutral draw commands.

It does not own font shaping/rasterization, GPU resources, native screen readers, device bindings, gameplay actions, or visual-authoring tools.

## Modality Policy

The generated default interaction target is a foundation-proof value selected from the initial platform profiles. It is not a universal accessibility claim. `PRODUCT_BRIEF.md` defines and tests target geometry for touch, pointer, controller/TV focus, text scale, viewing distance, and supported devices.

UI focus navigation consumes semantic actions from `human_resources_input`. Pointer or touch hit testing resolves the same stable node IDs. Activation becomes a client/product action and never mutates product state inside layout code.

## Frame Contract

1. Build or update the retained `Node` tree from presentation state.
2. Validate finite geometry and unique non-zero node IDs in development/tests.
3. Run layout with viewport, safe-content insets, text direction, text scale, and product interaction policy.
4. Derive accessibility nodes and focus order.
5. Resolve semantic navigation/activation or clipping-aware pointer hit testing.
6. Produce renderer-neutral draw commands.

Product code never hard-codes device notches, TV overscan, controller glyphs, or logical/drawable scale assumptions in components.

## Optional Dynamic Text

When `dynamic-text` is selected, SDL_ttf implements `TextMeasurer` and matching renderer text resources. Measurement and rendering use identical fonts, fallback, locale, script, wrap width, and scale. Font licences and missing-glyph behavior are package and acceptance requirements.

## Accessibility Adapters

The selected platform adapter maps the semantic tree, focus, value, state, and activation behavior to native accessibility APIs where the product supports them. SDK types remain outside this module. Tests include supported viewport classes, maximum text scale, localization expansion, RTL, reduced motion, keyboard, controller, touch where selected, offline, error, and disabled states.

Admit a third-party UI or layout library only when measured product requirements exceed this compact contract and its dependency, authoring, accessibility, package, platform, and migration costs are recorded.

## Coordinates, measurement and identity

Layout uses points. The SDL adapter derives window-units-per-point from display scale divided by pixel density, normalizes viewport and safe area, divides pointer coordinates, and converts native accessibility frames back to window units. Font/accessibility text scale is independent of display raster density. Resize events invalidate both layout and pressed targets before further hit testing; a temporarily invalid viewport suspends rendering until it is available again.

Text measurement respects resolved width and text insets. Grid cells are measured at their actual cell width, including when the grid itself is intrinsically sized. `LayoutResult.overflows` reports clipped text and clipped fixed-container children separately from invalid geometry. Products should require no accidental overflow at their accepted widths, translations and text scales.

Assign `Semantics.stable_id` from a durable product identity when rebuilding trees can change numeric node IDs. Empty identities use the node ID for compatibility; those IDs must then remain stable. Duplicate semantic identities are rejected. Native bridge implementations must retain elements by semantic identity and validate activation against the current tree.

## Native mobile bridge

The selected iOS SDL client publishes clipped semantic frames as retained UIKit accessibility elements. The Android SDL client uses retained, touch-transparent native views. Both retain identity across re-layouts, expose labels/values/disabled/checked/selected state, and route activation and adjustable-value actions through an owned `accessibility::Session`. They read system text scale and reduced-motion preferences without applying a product-specific two-times scale cap.

The session queues durable identities and resolves actions against its latest validated tree when drained. Removing, replacing or disabling an element invalidates its pending action. The queue is bounded, and closing a surface makes further callbacks inert. Product code drains actions on its own thread and still applies its normal state validation before a mutation. Screen-reader callbacks never mutate product state directly.

Desktop native screen-reader adapters are not implemented by this version: their adapter reports unavailable. Physical VoiceOver/TalkBack behavior, focus after modal/navigation transitions, locale pronunciation, supported device text scales and platform acceptance still require product-level testing. Native builds and shared contract tests are not substitutes for those checks.
