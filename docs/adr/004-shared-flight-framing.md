# Shared flight framing

Date: 25 October 2026. Status: accepted product presentation decision; physical
device qualification remains separate.

The expanded legal flight range can leave the camera on wide windows and pass
under controls on tablets. Two phone-only projection regressions did not cover
the selected iPad and desktop surfaces. Fix this through one disposable,
SDL-free `GameplayFraming` value owned by the product and composed by the client.
It carries display context, safe rectangle, actual fitted world stage, capped
render extent, normalized destination, camera and optical pixel weight.

Ordinary accepted phone projection and original ship size remain exact. Wider
flight surfaces fit a stable portrait stage at a maximum 320:568 aspect. Short
surfaces reserve measured side rails for readable status and real 44-point
controls. Compact damage copy retains the affected HP/SH layer and actual
BUMP/SHOT cause; spoken descriptions remain complete. Boss percentages may use
two measured lines. Rule coordinates, flight bounds, actor scales and saves do
not change.

The shared integer destination fit owns both renderer blits and screen-space
attachments. Normalized UI still covers the full output; hit rectangles,
accessibility bounds and stick throw remain in global logical coordinates.
Safe insets apply once. Resize/reflow uses the existing input cancellation
lifecycle. Ordinary presentation introduces no new full-resolution render target
or dependency; the world retains its 1280-pixel longest-side cap.

Title and Hangar keep their accepted full-output attract background. Settings
and blueprint views inherit their originating menu or flight context. Run,
pause and upgrade views retain consistent flight framing. Context is part of
the shared value, so scene intent cannot contradict its camera or render extent.

A following camera was rejected because runner-band attacks are committed in
world space: hiding incoming sources or warnings would change the information
available to the player. Globally shrinking the ship would violate the owner's
explicit size decision. Independent camera, vitality and renderer formulas were
rejected because rounding, safe areas and DPI can make them disagree.

Verification binds the consumed maximum-gear meshes, legal flight extremes,
bank/pitch/rotor phases, actual attack commit/retirement poses, safe insets,
translated origins, integer/noninteger DPI and compact text/control geometry.
Native comparisons against accepted `5fa75ab` preserve both ordinary phone frames
and restored menu backgrounds exactly. Synthetic extreme and damage display
fixtures are labelled separately from the actual earned 400-second checkpoint.
See `build/evidence/gameplay-framing/` for source-bound results.

The enlarged RUN follow-up resolves the old full-rig/header/boost overlap with
one 44-point strip at either end, centering actual consumed glyph ink instead of
transparent nominal bearings. It applies to noncompact enlarged phones and
tablets; ordinary phone pixels and camera remain exact. Full defence values stay
spoken/available in Pause when largest type needs a labelled fraction display.

A separate compact short-window budget defect remains: at 320 points and 3.2x
text, measured rails leave only a 14x24-point world stage. Physical reachability
on current devices is unverified. This value-contract defect needs a coherent
layout fallback; a positive-sized but unusable stage is not release acceptance.
