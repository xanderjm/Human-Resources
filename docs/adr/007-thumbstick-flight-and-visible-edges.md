# Thumbstick flight, flowing Dash and visible edges

Date: 26 October 2026. Owner direction: copy Midnight Menu's thumbstick and
flick-to-dash; make flight flow and settle with spring; reach every display edge,
with roughly half the ship allowed outside.

The product owns a local copy of Midnight Menu's bounded timestamp-based
`FlickGesture`, retaining its 38-point / 800-point-per-second threshold, 120ms
history, straight-stroke rejection and held-contact rearming. No sibling checkout
is used by the build or installed game. Controls use the reference's fixed
48-point touchdown anchor and continuous 15% radial deadzone. The same logical
throw drives a private thumbstick presentation component at every DPI/text size.
Movement capture remains separate from other fingers' HUD activation. Cancel,
focus loss, menu transitions and device removal discard captured input and any
unconsumed flick. Slow drags and unavailable/invalid native timestamps retain
ordinary steering without inventing a Dash.

Flick direction is a semantic value independent of the anchored stick's current
deflection. Gameplay retains an input edge until an actual fixed tick. Ordinary
flight converges exponentially within a radial acceleration limit; release uses
a faster catch. Existing starter/Phase Drive distances, protection, cooldowns
and counterfire remain their existing rules. Dash integrates a decelerating
150ms displacement over active ticks instead of teleporting. Cruise continues
underneath; bounds absorb outward movement without storing hidden travel.
A level-up may install Phase Drive during an already committed starter burst;
that burst retains its original distance. Presentation follows the observed
endpoint until the burst ends, then freezes its wake. A separate exact damped
spring controls attitude relative to automatic scroll, never authoritative
position; reduced motion removes the lean.

The owner's later edge request supersedes ADR004's portrait-only world stage
and complete-rig/HUD exclusion. The world now fills the actual output. Wider
windows reveal more horizontal world at the established portrait vertical scale;
ordinary phone camera/ship scale and the 1280-pixel world cap remain. Existing
measured safe HUD layouts overlay this world. Short windows always use the
existing top/bottom strips, so opaque side rails cannot hide the ship at its
new left/right limits. `GameplayFraming` inverts the
actual camera on the ship-centre plane to provide renderer-neutral world limits.
A centre at each clip edge permits roughly half the saucer outside; it can fly
under the HUD. The client supplies those limits through semantic `Control`.
Gameplay validates and owns the accepted rectangle, freezes it while paused and
adopts a changed device area on the first active tick. Screen size, SDL and GPU
types do not enter product rules. Spawns and encounter tuning are unchanged;
wide-window pacing and physical touch feel still require human playtests.

The first flight integration, Game codec5/authority13, adds 32 bytes alongside
the concurrent weapon work's 17 bytes: committed Dash direction, remaining duration and distance, and
the accepted four-edge rectangle. Subsequent concurrent population work advances
the combined payload to codec6/authority14; it retains these flight fields.
Account schema2 stays. Exact same-device cold
futures retain this area; a real new display supplies its own area after Resume.
Older incompatible continuations retain the established verified-backup and
once-only banking flow. Existing player apps/saves are not overwritten.

Verification must cover native event time, slow/curved/invalid strokes, release
and held flicks, foreign contacts, cancellation, sub-tick edges, exact burst
travel, protection/cooldown, all Phase Drive ranks, pause/cold future and resize.
Camera tests and consumed cooked-hull tests replace the old requirement that the
complete ship stay away from every screen edge: the new requirement is actual
edge reach with partial clipping. Safe controls, physical text size, threat
source geometry and package boundaries remain required. Native rendering and
host tests do not establish physical-phone input, performance or human fun.

## 26 October clarification — equal visible flight speed

The owner confirms that releasing controls must hold the ship steady on screen,
and backward flight must feel as fast as forward flight. Retain automatic runner
carry, but preserve pilot velocity when scroll speed changes from active time,
captures or levels. Scroll changes therefore bypass the steering acceleration
budget. A vertical boundary absorbs only pilot velocity: world velocity becomes
the current scroll speed, matching the zero lateral velocity at a side boundary.
Release at any edge holds position; steering inward has the same response on
every axis. Capture slowdown, thruster gains and Dash retain their existing rules.

The flight fix initially advances authority17 to18 without adding saved bytes.
Concurrent enemy engagement work integrates it into authority19; codec6/account2
and protected incompatible-run handling remain. Native flight regressions compare
signed velocity and visible travel through acceleration, reversal and braking,
with capture load, thrusters and changing scroll, plus stationary release and
edge response. This does not qualify an already running owner app or phone feel.
