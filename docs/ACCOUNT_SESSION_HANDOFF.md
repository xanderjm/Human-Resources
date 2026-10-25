# Account session integration handoff

Portable owner: `human_resources/progression/account_session.h`, implementation `src/account_session.cpp`.

`AccountSession(storage::Backend&)` owns `AccountStore`. Backend outlives session. Public API:

```cpp
LoadResult load();
MutationResult retry_save();
MutationResult preserve_and_reset(const storage::RelativePath& backup);
MutationResult begin_run();
MutationResult checkpoint(const RunReceipt&, bool background = false);
MutationResult settle(const RunReceipt&);
MutationResult purchase(Track);
MutationResult credit_bonus(std::uint64_t run_id, PlacementOutcome);
MutationResult revive(const RunReceipt&, PlacementOutcome);
MutationResult update_settings(Settings);
const AccountStore& store() const noexcept;
const Account& account() const noexcept;
const RunReceipt& latest_receipt() const noexcept;
LoadResult load_result() const noexcept;
bool pending() const noexcept;
SessionOperation pending_operation() const noexcept;
bool writable() const noexcept;
```

Use the session for **all mutations**, not a parallel AccountStore. `store()` is const and supplies existing Hangar, blueprint and placement availability views. A failed write retains its exact requested operation and the latest full run receipt. Show a save error/retry action, inhibit conflicting purchases/new attempts, and call `retry_save()`; do not call `AccountStore::load()` after a live write failure.

Integration owner must add `src/account_session.cpp` to progression CMake and register `native/tests/account_session_tests.cpp`. No app/CMake ownership is taken here. One essential AccountStore extension is `load(CheckpointRecovery recovery = CheckpointRecovery::settle)`: session-only protected reload uses `CheckpointRecovery::preserve_active`, validates the reloaded account against the last committed state, then retries the retained mutation. Default startup recovery remains unchanged. A different/missing account after an established save is a conflict, never a guest transition. This includes a missing save whose encoded default values happen to match the last committed account. A foreign older settlement with the same run ID is rejected rather than treated as the full retained run.

Initial startup `load()` retains existing process-death checkpoint recovery. A failed initial recovery can be retried. Corrupt/future documents expose an explicit `preserve_and_reset(backup)` action only when no live or pending run exists; verified backup must complete before fresh account publication. Read failures cannot reset. Settings, purchase, completed bonus and revive operations are applied to gameplay/UI only after successful persistence. Duplicate completed callbacks do not grant twice.

**Pause the actual game while any non-checkpoint operation is pending.** Do not resume gameplay, accept a new live run, or silently abandon a completed placement while a settings/purchase/bonus/revive/settlement save remains unresolved. Checkpoint failures can continue retaining monotonic full counters, and a final settlement or completed revive supersedes the pending checkpoint. Conflicting operations are rejected rather than queued.

Before Retry Save capture `pending_operation()`. If it was `SessionOperation::revive` and retry succeeds, invoke `Game::revive()` exactly once before returning to play; failed persistence must never apply the gameplay revive. Other pending operations map similarly to their original UI continuation. The session retains at most one operation plus one monotonically increasing full receipt.

Focused regression suite: `native/tests/account_session_tests.cpp`; eight scenarios cover full settlement after protected reads, active checkpoint retry, failed cold recovery, foreign stale settlement, failed verified backup/reset, completed revive/bonus retry, vanished established save, and failed fresh-save write after verified backup. Verification: standalone production-source `clang++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror` compile passed, all eight scenario functions passed, and `python3 tools/project.py verify --static-only` passed on 25 October 2026. Root owns final CMake/aggregate qualification. No generated tuning or app/CMake files were edited in this pass.
