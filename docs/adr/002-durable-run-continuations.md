# Atomic account and active-run continuations

Status: implemented portable contract, 25 October 2026. Native client and physical-device qualification are separate evidence.

## Context

The first playable saved account progress and an earned-reward checkpoint, then settled an interrupted run to Results. The release goal requires the same active run to survive OS termination. Resuming from visible `Snapshot` values would lose private RNG, AI, capture, cooldown and draft authority. Saving a separate run file would create two failure-ordering points between account rewards and gameplay.

## Decision and ownership

Keep one protected `account.bin` document with envelope/account schema 2, bounded to 64 KiB of payload. Account state, receipt, stable location identity, recorded checkpoint credits and a bounded gameplay continuation commit atomically. The Game payload is independently versioned HRS1 schema 1 and at most 48 KiB. No new dependency, replay log, renderer cache, SDK or generic serialization framework is introduced.

- Gameplay owns explicit field encoding/validation, normalized original setup, exact saved phase and the candidate-then-replace restore operation. It preserves actor ordering/identities, three RNG streams, modules/research/evolutions, compact draft identities, fractional score, XP surplus, runner station, private timers, capture ownership/grace/progress, AI/wander/aim and projectiles. Draft descriptions rebuild without consuming RNG. Numeric runtime tuning has a generated compatibility fingerprint; independent authority changes require a manual authority revision.
- Progression owns the account schema, monotonic run receipt, recorded rewards and the atomic account/continuation transaction. It checks that Game stats, original account setup and revive consumption match the receipt/account. Location uses stable identities 0 farmland, 1 suburbia, 2 desert; it contains no renderer data.
- `AccountSession` retains the exact full pending continuation or settlement intent when a write fails, and retries only against the unchanged prior document. Rejected candidate data must not mutate retained intent. An established file that disappears or changes externally remains protected.
- Client composition captures state at safe simulation boundaries, cancels held input, clears presentation/audio/haptic history, resets wall-clock timing, and restores to a user-controlled pause. The existing earned draft resumes with the same options. Shot/event history and pending input edges are deliberately transient.

A new run first commits its initial Game state together with its run ID. Periodic checkpoints remain ten active seconds apart. Choice entry/acceptance/reroll, death, pause and background boundaries flush full state. A failed full checkpoint pauses interaction while its precise pending state is retried; simulation must not outrun the retained bytes. The native client owns these boundary calls.

Death is terminal across cold restart: a saved dead Game settles to Results. End and Bank is an intentional settlement that clears the continuation and banks credits in the same write. A settled run has no continuation to restore. A completed revive commits already-revived Game bytes and the consumed revive flag together, before publishing the live candidate; termination between storage and publication cannot lose the earned revive or grant another one. Production has no live rewarded-ad provider.

## Migration and incompatible data

Schema 1 remains readable through an explicit legacy-envelope whitelist. It has no full Game payload, so its interrupted receipt retains the prior once-only checkpoint settlement. Successful mutation/migration emits schema 2 atomically. A failed write leaves the original schema 1 bytes untouched. Missing, corrupt, unsupported, unavailable, oversized and I/O-failed documents stay distinct; a future outer/account schema cannot become a fresh account.

Known account schema with incompatible or unsupported Game continuation stays locked. The recovery action first preserves and reads back the exact original under a separate backup path, then banks the bounded recorded checkpoint credits and removes the incompatible Game bytes in one commit. It does not rerun historical reward calculations with current tuning. An identical existing backup may be verified and reused after a process died between backup and replacement; a different backup is never overwritten. Corrupt/unreadable outer account data cannot use this summary-recovery path.

Stored Results use their recorded reward-policy identity. Policy 1 is the accepted once-only +50% base-credit bonus. Validation checks bounds, consumption and policy contradictions rather than recalculating historical base rewards with today's tuning. Account unlock ownership is preserved when cumulative progress grants further unlocks.

## Alternatives and consequences

A separate linked run file was rejected: it would need a journal/generation protocol, cleanup and reconciliation to avoid stale worlds and duplicate rewards. Replaying an entire input history was rejected: it introduces unbounded logs and requires stronger cross-version determinism than this offline single-player game selects. Serializing `Snapshot` or raw C++ layouts was rejected because presentation values omit authority and native layouts are not portable.

The bounded Game blob adds small copies and encoding work only at save boundaries. Atomic file-write latency, background completion and physical-device storage protection still need actual device measurement. This change does not imply cloud synchronization, anti-cheat security, distribution readiness or uninterrupted 30-minute-run evidence.

## Verification and rollback

`gameplay_save_contracts` covers phases, exact future continuation, RNG/drafts, hidden timers/evolutions, malformed/truncated/oversized values, relationships, schema/rules mismatch and transactional restore. `account_resume_contracts` covers actual native runs, selected location, paused resume, exact pending bytes, durable draft/reroll/pick boundaries, terminal death, completed-revive failure/retry/restart, schema 1 migration, backed mismatch recovery and historical reward validation. The original eight `account_session_contracts` scenarios remain. Storage regressions cover explicit legacy acceptance, future protection and idempotent verified preservation. Optimized host and sanitizer evidence is recorded under `build/evidence/account-resume-*` and `gameplay-save-*`.

Older applications protect schema 2 as unsupported; they must not reset it automatically. Preserved backups are retained for explicit recovery. Actual Android process-kill continuation, native normal-input recovery UI and physical mobile storage behavior require separate qualification against final source hashes.
