# Diagnostics Contract

`human_resources_diagnostics` is the dependency-free structured event path used by gameplay orchestration, presentation, platform, audio, and renderer code. It does not replace crash capture, metrics export, or a profiler; it gives those systems one stable, testable input contract.

## Event Shape

Each event has:

- severity and subsystem;
- a stable event name suitable for filtering and aggregation;
- optional simulation tick and monotonic timestamp supplied by the caller;
- a bounded set of typed fields;
- public or sensitive privacy classification.

Human prose belongs in a public field when useful, but stable event names and typed fields are the contract. Do not parse log sentences to recover product state.

## Runtime Rules

- `try_record` is bounded, allocation-free, non-blocking on lock contention, and never invokes a sink.
- Producers may drop events under contention or when the queue is full. Drop counts are observable; gameplay must never wait for logging.
- `drain` runs on an owner-selected non-realtime thread or frame boundary and invokes sinks only after releasing the logger lock.
- History and queue capacities are explicit at construction and do not grow at runtime.
- Sensitive string and numeric fields are redacted before any sink sees them.
- Logging failure cannot change deterministic state, validation, execution ordering, or player outcomes.
- Audio callbacks and other realtime code may use `try_record`, but must not call `drain` or a sink.

## Sink Boundaries

The scaffold includes an SDL platform sink. File rotation, OS unified logging, remote telemetry, crash breadcrumbs, and developer overlays are optional sinks behind the same interface. Add them only when the product brief defines retention, consent, redaction, offline behavior, and an owner.

Never log credentials, purchase tokens, raw user content, precise location, stable platform identifiers, or other personal data merely because a sink is local today. Privacy classification is an additional guard, not permission to collect unnecessary data.

## Verification

Tests must cover severity filtering, bounded queue overflow, contention/drop accounting where practical, field truncation, redaction, history order, and sink fan-out. Platform tests must show that a failed or unavailable sink cannot prevent startup or gameplay.
