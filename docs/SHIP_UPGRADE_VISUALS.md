# Physical ship upgrades

Owner direction, 26 October 2026: upgrades visibly rebuild the ship's geometry,
materials and motion from above. The earlier deck-glyph proposal is superseded.
The later owner catalogue decision removes Reserve Battery, Bounty Scanner,
Mirror Plating and Overtime Reactor; twelve live hardware families remain.
Retired enum positions remain reserved by gameplay, never resurrected by art.

## Component ownership

The original GLB, painted textures, models and assembly guide stay preserved.
The playable reassembles its fixed Hull/Bearing, Dome, Dome Collar and spinning
Outer Ring as separate physical parts. Original laser, beam, antenna and rim
lens pieces retain their authored finish and socket conventions. New machinery
uses closed chamfered housings, lathed optics, structural supports and the same
material vocabulary. Every persistent upgrade casts a real depth-tested shadow.

| Hardware | Physical identity and rank I → III |
| --- | --- |
| Laser Array | Two, three, four independently aimed working guns on fixed-hull sponsons, outside/below the rotor. Count follows actual laser count. |
| Rail Lens | Replace each short barrel with progressively longer narrow accelerator casings, recessed upper channels and fitted focusing lenses. |
| Arc Coil | Replace each muzzle head with increasingly wide/long split electrodes and ceramic holders. Actual arcs retain gameplay event timing. |
| Orbit Drones | One, two, three separate armed miniature saucers. Each includes a physical gun, canopy and hull. |
| EMP Ring | Three increasingly thick stepped rotor profiles, violet finish and twelve/eighteen/twenty-four complete capacitor lenses, versus the original six. Cells remain evenly distributed and rotate with the deck. |
| Beam Splitter | A physical capture manifold with two, three, four supported apertures visible beyond the ring. Active beams/actors finish at those actual apertures. |
| Tractor Lens | Expand only the annulus, approximately ten/twenty/thirty percent in outer diameter, and increase the existing intake optics. The canopy and bearing retain their fitted size. |
| Heavy Winch | Deeper industrial intake housing, articulated supports, pivots and progressively broader/longer gripping jaws. The front jaw shoulders project beyond the ring in top view. |
| Shield Generator | Wider mast base and one/two/three substantial faceted crystals on connected supports. Physical generator persists when shield energy is depleted. |
| Repair Swarm | One/two/three recessed maintenance bays, each containing a separate repair unit. Actual hull recovery briefly deploys the units; reduced motion keeps them docked. |
| Vector Thrusters | Two substantial rear engine pods with one/two/three real nozzles per pod. Movement exhaust leaves those exact openings. |
| Phase Drive | Three lower, elongated canopy meshes with violet optics. Their original circular mounting flange stays fitted to the collar; the upper shell changes shape. |

Guns compose by count, barrel and muzzle, so Array/Rail/Arc operate on the same
working weapons. Ring diameter, profile, cells and rotation remain independent.
Capture branches occupy front/back angular bays; engines use outboard rear
positions, guns stay below the rotor, and repair bays occupy a raised fixed shelf on the right, above the crossing
capture/engine machinery.
Supports attach to the fixed hull and clear the spinning annulus. The central
intake casing stays below the rotor; Winch replaces that casing when fitted. The inner ring
radius stays 0.604m around the original 0.587m bearing; growth never expands that
joint. Lamp positions and slopes are measured from the actual cooked rank mesh.

## Research and earned combinations

Research modifies existing equipment with bounded, saturating geometry changes:
Weapon Damage thickens gun housings; Hull Reinforcement strengthens the collar;
Shield Capacity enlarges the installed crystals (or baseline antenna housing);
Specimen Value changes the antenna finish; Capture Speed enlarges fitted optics.
Fire Rate increases ring speed from 1.8 to 3.0rad/s at its research cap. Actual
Overdrive adds its own transient rotation response. Emergency Repairs has no
invented installed rank and uses the observed hull-recovery response.

The five surviving evolutions remain attached to their actual machinery:
Great Vacuum has six enlarged intake ports; Office Shredder adds fitted focusing
sleeves to its guns; Hostile Acquisition reinforces its jaws; Employee Benefits
uses armoured armed drones; Storm Drain uses its real EMP rotor and event radius.
There are no evolution badges or decorative crowns. Effects cannot invent a
shot, capture, heal or pulse that gameplay did not produce.

## Shared geometry and motion

`ship_assembly.h` is a fixed-capacity product presentation owner. One assembly
contract supplies visible parts, exact gun/drone muzzle positions, intake points,
engine exhaust and the drone exclusion envelope. Renderer code consumes those
instances; it does not own equipment state. No gameplay hitbox, balance, save
schema or installed-slot rule changes are introduced by this appearance slice.

Drones use equal angular spacing and a measured orbit outside the complete
assembled rig, including its widest ring, gun sweep, engines and capture ports.
The minimum air gap is half a drone width to the ship and one drone width between
neighbours, verified against transformed production vertices through bank/pitch
and orbit phases. A larger rig produces a larger orbit. Full-edge flight remains
allowed: the ship centre can reach the camera edge and outer equipment may leave
the screen. Interior composition and edge-reach tests are separate contracts.

The rotor phase integrates active time with smooth speed changes. Picking an
upgrade at held time never jumps the angle. Pause freezes motion. Reduced motion
preserves the complete hardware/colour/count while freezing cosmetic movement.
Intake bindings survive beam-vector compaction and retain the aperture during
confirmed capture finishers. No capture target jumps to a new port on completion.

## Cooking and verification

`assets/production/ship-components.json` explicitly lists the physical parts.
`tools/cook_ship_components.py` reuses the original editable mesh construction and
embedded GLB paint without rewriting source art. Only ignored HRMs and generated
headers are output. The runtime allowlist and source-hashed package provenance
include the recipe, cooker and original authoring tool. Retired reactor, armour
and scanner variants do not ship. No new runtime dependency is introduced.

The offline suite validates finite geometry, unit normals, nondegenerate cooking,
exact shipping inventory, original-art preservation, ring bore/profile changes,
barrel progression, fitted canopy flanges and a bounded component payload. Native
contracts decode the actual HRMs and verify all thirty-six active module/rank
shapes, real muzzle/intake endpoints, combined hardware, drone air gaps, stable
intake bindings, underbody casing clearance, raised dock clearance, continuous
rotor phase and reduced motion.

`tools/playable_qa/ship_upgrades.cpp` renders labelled synthetic native Metal
frames for all ranks, top views, three six-module loadouts, five evolutions,
twelve firing directions, edge positions and motion. Its optional neutral-material
asset tree preserves original vertices/normals/UVs/indices. Early v2 renders and
failed checks remain diagnostic evidence under `build/evidence/ship-components-v2/`;
the newest development-log entry records final accepted checks and exact limits.
Synthetic host review does not qualify physical-phone performance, human fun,
naturally earned loadouts, mobile packages or a replacement owner playtest app.
