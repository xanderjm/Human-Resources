# Code Organization And Separation Contract

This document is binding for product-owned code. It defines how to divide systems, files, types, and functions without turning line counts into an architecture substitute.

The governing rule is ownership: code that changes for the same reason and protects the same invariants belongs together. Code with different owners, dependencies, lifetimes, authority, or test contracts belongs behind a named boundary.

## 1. Decision Order

When deciding whether to keep code together or split it, evaluate in this order:

1. architecture and dependency direction;
2. ownership of state and invariants;
3. runtime lifetime, threading, and authority;
4. test contract and failure policy;
5. change cadence and readability; and
6. size signals.

Line count is deliberately last. A small mixed-responsibility file is still badly organized, while a larger cohesive state machine, parser, table, or performance-sensitive loop can be correct.

Do not create pass-through classes, one-call wrapper files, or tiny functions solely to satisfy a metric. An extraction must create a meaningful name, dependency seam, independently testable operation, or clearly owned concept.

## 2. Systems, Targets, And Components

A system is a CMake target or similarly explicit service boundary with one product responsibility and a declared dependency contract. Systems do not have a line limit.

- Target boundaries follow `docs/NATIVE_GAME_PROJECT_BLUEPRINT.md`. Never fix a large file by moving code across an invalid dependency edge.
- A target may contain multiple internal components. Give a component its own files when it owns a distinct resource lifetime, state machine, algorithm family, concurrency policy, platform capability, data format, or test vocabulary.
- Public target interfaces expose product operations and durable value contracts. Concrete implementation types remain private to the target.
- Dependencies point one way. Circular target dependencies, back-door includes, service locators, and duplicated domain rules are prohibited.
- Renderer, platform, input, UI, audio, diagnostics, persistence, networking, tools, gameplay state, and any selected authority profile remain separate owners even when an initial slice is small.
- When deterministic authority is selected, local play, bots, replay, and hosted matches may compose authority differently, but they do not receive separate rule implementations.

Create or split a component when any of these becomes true:

- it has a second independent reason to change;
- it owns data with a different lifetime or synchronization rule;
- it needs dependencies that the surrounding component must not acquire;
- it can fail, retry, suspend, or recover independently;
- its tests require a different fixture family;
- it has a stable input/output contract that can be named; or
- navigating the current owner requires knowing unrelated systems.

## 3. File Ownership

- Each file has one principal concept or one tightly coupled family of value types. Its filename should reveal that owner.
- Do not use `Utils`, `Helpers`, `Common`, `Manager`, or `Misc` as dumping grounds. A shared helper must be named for the domain operation or primitive it owns.
- Keep public headers under the target's public include tree. Keep implementation-only headers beside the source or in an explicitly private `detail` subtree.
- Prefer several cohesive `.cpp` files behind one stable public interface over one accumulating implementation file.
- Keep serialization, validation, execution, presentation, and persistence in separate files once they are non-trivial; sharing a data type does not make them one responsibility.
- Do not duplicate a declaration or algorithm to avoid creating the correct dependency boundary.
- Generated bindings, cooked data, and generated platform projects live under `build/` and are never hand-edited.

When an existing oversized file is touched, do not perform arbitrary line-only surgery. Preserve behavior with focused tests, carve out one coherent owner at a time, and keep structural refactoring separate from behavior changes when practical. Never add a new unrelated responsibility to an already mixed or escalated file.

## 4. Functions

- A function performs one logical operation at one level of abstraction.
- Its name describes the operation or question at the call site. Comments explain contracts, invariants, or non-obvious decisions; they do not compensate for an unnamed operation.
- Prefer early validation and early returns when they reduce nesting. Avoid deeply nested control flow and `else` blocks after an unconditional return.
- Keep variable scope as narrow as possible. Do not reuse one variable for different meanings.
- Make mutation visible in the interface. Avoid hidden global state and surprising side effects.
- Prefer return values for outputs. Multiple related outputs should normally be a named result type.
- Seven or more parameters trigger interface review. Use a parameter object only when the fields form a coherent domain concept, not as an untyped bag that hides complexity.
- Avoid boolean mode parameters when `run(true)` and `run(false)` have materially different behavior. Prefer a named enum, policy object, or separate operations that make the call readable.
- Extract repeated or independently meaningful predicates and operations. Do not extract a fragment that cannot be named without referring to its line position.
- Do not split a measured hot loop merely for line count. Keep its invariants visible, test it directly, measure before and after structural changes, and isolate surrounding policy from the hot mechanism.

## 5. Size Review Signals

These are review triggers, not automatic failures or permission to keep mixed responsibilities below a threshold.

| Scope | Review signal | Required response |
| --- | --- | --- |
| Function or substantial lambda | About 40 executable lines, or it no longer fits comfortably on one editor screen | Check for multiple logical operations, excessive nesting, over-wide state scope, or a missing named suboperation. Split when that improves the structure. |
| Function interface | 7 or more parameters | Reconsider the operation boundary and whether a cohesive value/result type exists. |
| Public header | About 300 physical lines | Review public surface area, leaked implementation details, include cost, and whether multiple concepts have accumulated. |
| Implementation or test file | About 500 physical lines | Review ownership, change reasons, fixture families, and navigation cost. |
| Public header above 500 lines or implementation/test file above 800 lines | Escalated | Before materially expanding it, split a coherent owner or record the reason for keeping it cohesive in the development log, review description, or an ADR. No unrelated responsibility may be added. |

Blank lines, comments, generated code, schema-generated bindings, and predominantly declarative data tables make raw counts noisy. Use judgment; do not rewrite formatting or delete useful documentation to reduce a number.

There is intentionally no line limit for an entire system or target. A substantial system should grow by adding cohesive internal components behind its existing contract, not by widening one facade or implementation file indefinitely.

## 6. Headers And Interfaces

- Every public header is self-contained: it has an include guard or `#pragma once` and directly includes what its declarations require.
- Every `.cpp` includes its corresponding public or private header first so hidden transitive dependencies fail locally.
- Include as little as practical, especially from public headers. Use a forward declaration only when the incomplete-type rules, ownership, and readability remain clear.
- Do not place `using namespace` directives in headers.
- Keep implementation-only types and helper APIs out of public headers. Use a private header when several `.cpp` files genuinely share an internal contract.
- Document public invariants, ownership, lifetime, thread/realtime expectations, error behavior, units, and determinism requirements where they are not obvious from the type system.
- Prefer durable value contracts over exposing mutable containers or concrete subsystem internals.
- Use PImpl only for a real ABI, compile-time, or platform-isolation need; it is not routine ceremony for product-owned internal targets.

## 7. State, Ownership, And Failure

- Every long-lived object and resource has one clear owner. Prefer values and RAII; use `std::unique_ptr` for transferred exclusive ownership and `std::shared_ptr` only when shared lifetime is an actual requirement.
- Dependencies are passed explicitly through constructors, setup structures, or narrow function arguments. Hidden singletons and mutable global service registries are prohibited.
- Do not retain a reference or pointer beyond the documented lifetime of its owner.
- Separate configuration, durable state, derived cache, and transient frame/scratch data. Derived caches must be reconstructible unless their progress can affect the next authoritative decision, in which case they are persisted state.
- Error handling follows the owning boundary. Deterministic validation returns deterministic results; diagnostics report but do not decide gameplay; platform failure does not silently mutate authority.
- Realtime callbacks and steady-state render/audio paths must not acquire unbounded work, allocate unpredictably, wait on logging, or perform filesystem/network operations.

## 8. Stack-Specific Separation

### Deterministic core, when selected

- Organize rules by domain capability, with validation and execution explicit. Do not let a `Game`, `World`, or `Simulation` file become the location for every new rule.
- Core owns deterministic values, ordering, RNG use, validation, execution, and hashes. It does not own SDL, rendering, files, sockets, threads, locale, wall clock, or platform policy.
- Serialization and replay codecs consume core/protocol contracts but do not become alternate gameplay implementations.

### Runtime and authority, when selected

- Match setup, fixed-tick scheduling, turn sources, replay, snapshots, persistence, and product-service adapters are distinct owners.
- Client actions submit intents. Presentation code never mutates authoritative state directly.
- Network transport validates and conveys protocol messages; it does not duplicate simulation rules.

### Renderer, platform, and UI

- Split non-trivial renderer ownership into device/submission, shaders/pipelines, resource lifetime, upload staging, world passes, UI/text, camera/culling, effects, and diagnostics.
- SDL_GPU resources remain in the renderer. SDL events/window state and OS bridges remain in platform. Layout, hit testing, and semantics remain renderer-neutral in UI.
- Platform-specific source files implement narrow capabilities. Do not scatter `#if` platform policy through gameplay or general renderer code.
- CPU/shader interfaces use explicit layouts, size assertions, reflection checks, and one owning declaration.

### Audio and diagnostics

- Gameplay emits semantic presentation events; the audio system owns voices, buses, devices, focus, and interruption policy.
- Diagnostics producers emit bounded structured events. Sinks own files, SDL logging, telemetry, or overlays and cannot influence gameplay or block realtime work.

### Tools and content

- Offline tools operate on versioned schemas and the same runtime contracts. They do not introduce a second unofficial product format.
- Separate source parsing, validation, transformation, and package writing when each has a distinct failure contract.

## 9. Tests Follow Ownership

- Mirror production target boundaries in tests. Each test file owns one contract or closely related scenario family.
- Keep gameplay and deterministic rule tests headless where applicable. Renderer/platform smoke tests add evidence but do not replace state/runtime tests.
- Put reusable fixture builders in narrowly named test-support files; do not create a global test utility dumping ground.
- A refactor that moves ownership must preserve or improve the tests at that boundary before old paths are removed.
- Large declarative scenario matrices may stay data-oriented, but their execution, assertions, and fixture construction remain separate concerns.

## 10. Change Etiquette

- Read the complete owning interface, implementation, tests, and relevant architecture decision before editing a subsystem.
- Make the narrowest cohesive change that completes the real behavior. Do not mix unrelated cleanup into a feature or fix.
- When practical, perform behavior-preserving extraction separately from behavior modification so review and regression isolation remain clear.
- Preserve local naming and formatting conventions; the pinned formatter is the mechanical authority.
- Do not suppress warnings, tidy findings, sanitizer failures, or boundary tests globally to admit one change. Fix the cause or use the narrowest documented exception.
- Update `docs/AI_DEVELOPMENT_LOG.md` when a substantial pass changes ownership, introduces an exception, or leaves a named decomposition risk.

## 11. Review Checklist

Before completing a code change, confirm:

- the target and file own the behavior being changed;
- no forbidden dependency or duplicated domain rule was introduced;
- every new function has one nameable operation and visible mutation;
- public headers remain self-contained and implementation details remain private;
- size signals were reviewed rather than mechanically obeyed or ignored;
- deterministic, realtime, renderer, platform, and persistence implications were handled at their proper boundaries;
- focused tests cover the changed contract and required broader gates passed; and
- any justified escalated-size exception is recorded with a future split trigger.

## 12. Basis And Calibration

This contract adapts established C++ guidance to this project's production-shaped, library-layered cross-platform game architecture:

- The C++ Core Guidelines recommend packaging meaningful operations as named functions, one logical operation per function, short/simple functions, and treating functions with seven or more parameters as suspicious: <https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-functions>.
- The Google C++ Style Guide uses roughly 40 lines as a prompt to reconsider a function while explicitly rejecting a universal hard maximum: <https://google.github.io/styleguide/cppguide.html#Write_Short_Functions>.
- LLVM's coding standards require self-contained headers, explicit library layering, minimal includes, private internal headers, portable code behind defined interfaces, and early exits that reduce nesting: <https://llvm.org/docs/CodingStandards.html>.

The file-size signals are project-specific maintenance heuristics, calibrated to catch accumulating gameplay, renderer, platform, and test ownership before it becomes monolithic. They are intentionally not compiler or CI line-count gates.
