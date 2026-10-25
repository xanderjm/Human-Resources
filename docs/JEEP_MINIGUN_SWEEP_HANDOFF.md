# Jeep minigun: bounded whole-vehicle sweep

The original head and pedestal fit the actual four-wheel jeep through full yaw and **0–14 degrees total upward elevation**. This is an offline, unmanned assembly qualification. It does not promote assets or change shipping gameplay. The first unsafe **sample** was receiver/fork intersection at 17 degrees; this is not an exact continuous collision-onset measurement. Do not extend the operating cap from the neutral component fit.

The complete machine-readable contract is `build/animation/jeep-minigun-review/assembly-contract.json`. Geometry and continuous bounds are in `mount-qualification.json`; `ray-footprint-bounds.json` adds ray-centreline and vehicle-footprint bounds. The packed editable scene is `jeep-minigun-sweep.blend`. The 12-second, 360-frame, 1280×720/30fps review is `jeep-minigun-sweep.mp4` in the same directory. These are ignored authoring outputs, not runtime package inputs.

## Canonical actor frame and transforms

All following coordinates are metres, +Y up, +Z forward, with column vectors. `M_jeep` is the resolved **vehicle actor frame at ground Y=0**, before any chassis child offset. It does **not** contain the chassis's +0.12m translation. Apply actor placement and scale once outside this frame.

Use the original `assets/production/assemblies.json` / `jeep-four-wheels` contract: chassis child translation `[0,0.12,0]`; wheel sockets relative to that child at X ±0.78, Y 0.18, front Z 1.02 / rear Z −1.04. This puts wheel centres at actor Y 0.30m, bottoms at zero. Left wheels yaw 180 degrees. A native scene which omits the chassis +0.12m does not reproduce this qualified assembly.

| Datum | Value |
| --- | --- |
| Pedestal anchor, actor frame | `[0,0.4960820179858087,-1]` |
| Same anchor relative to translated chassis | `[0,0.3760820179858087,-1]` |
| Yaw pivot, source mount frame | `[0,0.8037945054981808,-0.005136986301369864]` |
| Pitch pivot, source mount frame | `[0,1.0017123287671232,-0.005136986301369864]` |
| Pitch pivot relative to yaw pivot | `[0,0.1979178232689424,0]` |
| Neutral pitch pivot, actor frame | `[0,1.497794346752932,-1.0051369863013699]` |
| Yaw / upward pitch axes | `+Y` / `−X` |

`N_head` and `N_mount` mean the original GLB node transforms from accessor coordinates to normalized source coordinates; their exact column-major matrices are recorded in `assembly-contract.json`. The qualification datums already use those normalized coordinates. Do not normalize twice.

```text
Head, original accessor vertices:
M_jeep * T(anchor) * T(yawPivot) * Ry(yaw)
       * T(pitchPivot-yawPivot) * Rx(-elevation) * N_head

Fixed post, original selected accessor vertices:
M_jeep * T(anchor) * N_mount

Yaw cap, original selected accessor vertices:
M_jeep * T(anchor) * T(yawPivot) * Ry(yaw) * T(-yawPivot) * N_mount
```

The original mount is split only at its disconnected components. Rotating IDs `[0,1,2,4]` contain the upper collar/fork/fasteners; all others stay fixed. The derived `exports/minigun-fixed-post.glb` has 424 triangles and bakes `N_mount`. Its draw transform is `M_jeep*T(anchor)`. The derived `exports/minigun-yaw-cap.glb` has 560 triangles, bakes `N_mount`, and subtracts the yaw pivot from its vertices. Its draw transform is `M_jeep*T(anchor)*T(yawPivot)*Ry(yaw)`. Do not subtract the pivot a second time. Reuse the unchanged original head GLB.

## Muzzles and engagement envelope

The source has six actual openings. All raw head datums below use forward `[0,0,1]` and Z `0.4274951076320939`. Apply the head transform **before** `N_head` to these already normalized points. Apply rotation only to forward, then normalize; apply actor transform exactly once. The cluster mean is `[0.003742661448140925,-0.00013861709067188033,0.4274951076320939]` and is a reference, not an extra physical barrel.

| Opening | X | Y |
| --- | ---: | ---: |
| muzzle_1 | −0.04823874755381602 | 0.05738747553816047 |
| muzzle_2 | 0.07818003913894327 | 0.014970645792563603 |
| muzzle_3 | 0.03243639921722116 | 0.07568493150684932 |
| muzzle_4 | −0.07318982387475535 | −0.017465753424657535 |
| muzzle_5 | 0.05489236790606655 | −0.058219178082191785 |
| muzzle_6 | −0.02162426614481406 | −0.07318982387475537 |

At target actor Y=2.5m and maximum 14-degree elevation, the lowest barrel needs at least 3.88966m horizontal distance **from its muzzle**. Use a conservative 4.5m minimum and solve from the actual transformed muzzle. At target Y=2.2m the worst exact minimum is 2.68643m. These numbers assume level actor placement and source scale. A moving or differently elevated actor needs the actual target-direction test. The total 14-degree limit includes any recoil or added aiming offset.

Closer attacks need an authoritative range/commit decision or a separately reviewed asset revision. Do not clamp the visible gun while launching along a different trajectory, or offset the head out of its trunnion to create artificial clearance. An optional 10mm forward spawn offset puts the ray past the terminal rim. The outgoing nonnegative-elevation ray centreline stays at least 24.565mm above the source chassis/windshield; finite-radius projectile clearance is not qualified.

## Verification and limits

The source head remains at its original trunnion. The pedestal's 25 deck-support rays have 1.002–3.680mm guard. All 984 mount triangles, UV corners, winding and albedo pixels are preserved across the two derived parts; independent reconstruction has maximum position error 0.113 micrometres. The collar/post mating region is an explicit existing mechanical interface, not an empty-space clearance claim.

| Continuous lower bound, full yaw and 0–14° pitch | Clearance |
| --- | ---: |
| Receiver excluding axle bosses to rotating cap | 3.503mm |
| Head to fixed post | 9.036mm |
| Axle/bore radial clearance | 2.462mm |
| Head to chassis | 180.647mm |
| Barrel cluster to chassis | 262.677mm |
| Rotating cap to chassis | 152.708mm |
| Head to windshield, Z separation | 723.380mm |

The receiver proof checks 57 pitch samples at 0.25-degree spacing with triangle distances, then subtracts the maximum between-sample vertex chord (0.9625mm) and numeric guard. Common head/cap yaw cancels; fixed-post cylinder and separate low-chassis/high-windshield bounds cover all yaw. Fixture cases include parallel triangles, triangle piercing and excluded unused vertices. There is no roof on the source jeep. The head's full swept disk remains inside the jeep's existing axis-aligned footprint.

The actual full-film pixels and 360 PNG hashes are bound to the source fingerprint in `film-validation.json`; source-sized whole-vehicle views and joint/cardinal stills were visually inspected. Source node/derived transforms are independently checked against the displayed head matrix (maximum matrix error below 0.0000005). All nine character exports and their frozen producer inputs remain unchanged. No crew is installed, and no seated-fit or barrel-spin claim is made. Native aiming, collision policy, finite projectile radius and device presentation still need their own integration evidence.

Reproduction scripts remain beside the outputs: `prepare_geometry.py`, `collision_math.py`, `probe_clearance.py`, `qualify_mount.py`, `render_sweep.py`, `render_stills.py`, `finish_sweep.py`. Geometry preparation uses configured art Python with NumPy; qualification/rendering use the repository Blender tool. `render_sweep.py --start 1 --end 360` renders the film frames; `finish_sweep.py` verifies and encodes them. Preserve the recorded source hashes before reproducing.
