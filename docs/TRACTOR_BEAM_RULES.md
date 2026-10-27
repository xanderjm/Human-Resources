# Tractor beam commitment

Owner direction: 26 October 2026. Once abduction starts, the beam stays locked.
Require pickup range to acquire a target. The later 26 October progression pass
retunes duration while preserving commitment and relative target weights; see
[ADR009](adr/009-earned-abduction-speed.md).

`human_resources_gameplay` owns this rule. Range, vehicle hull thresholds,
Heavy Winch eligibility, cooldown and available slots gate new pickups. An
occupied beam reserves its original target and slots through completion,
including outside pickup range. Vehicle cooldown from another completed lift
does not cancel work already underway. Scrolling retirement, military lifetime
expiry and the player's own damage cannot remove a lifted target.

Movement penalty, rewards, module prerequisites and capacity remain unchanged.
Current duration is 1.6 seconds per lift-load unit, accelerated by eight base-rate
percentage points per level after the first and eligible existing modifiers. Close moving-core participation still accelerates capture and
qualifies sweeps only while the actual movement/distance requirements hold.
Leaving that core retains earned participation but continues at the ordinary
rate. Pause, upgrade choices and suspension freeze gameplay as before. A run
ending still ends its active abductions; this does not promise rewards after death.

Tractor Lens retains its 10/20/30 percent pickup-radius improvements and its
rank-three gentle pull. Obsolete grace duration is removed from the live
catalogue, tuning generator and offer text. Historical offline simulation inputs
remain evidence of their original studies, not current runtime rules.

The existing beam/actor state already owns commitment and continuation; no new
capture fields or platform APIs are needed. Keep the old serialized `grace`
float as a zero reserved slot rather than moving every actor field. The rules
change advances gameplay authority and changes the numeric tuning fingerprint.
Older continuations follow existing protected backup/once-bank handling;
permanent account progress is preserved. Concurrent laser/movement work may add
its own codec fields and revision, so the source codec is the current authority.

Focused capture contracts cover lens boundaries, unchanged slow duration,
occupied slots, offscreen retention, sweep participation, exact cold continuation
and pause/movement/Dash. Vehicle contracts cover active cooldown, lifetime,
damage protection and completion. The public save/EMP regression confirms the
player's area damage preserves a lifted soldier. Host verification does not
establish physical-device behavior or human balance acceptance.
