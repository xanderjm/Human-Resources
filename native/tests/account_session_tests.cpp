#include <human_resources/progression/account_session.h>

#include "progression_test_backend.h"
#include "test_support.h"

namespace {
using namespace human_resources::progression;
using namespace human_resources::storage;
using progression_tests::MemoryBackend;
using test_support::require;

RunReceipt receipt(const AccountSession& session, const std::uint64_t milliseconds,
    const std::uint64_t captures = 0) {
  return {.run_id = session.account().checkpoint.run_id, .active_milliseconds = milliseconds,
      .score = milliseconds / 100, .captures = captures};
}
void failed_full_settlement_replays_instead_of_old_checkpoint() {
  MemoryBackend backend;
  AccountSession session(backend);
  require(session.load().playable() && session.begin_run().succeeded(), "start actual account run");
  require(session.checkpoint(receipt(session, 10000, 10)).succeeded(), "persist ten-second checkpoint");
  backend.read_failure = ReadIssue::io_failure;
  require(!session.checkpoint(receipt(session, 20000, 20)).succeeded(), "read failure protects document");
  auto full = receipt(session, 90000, 300);
  full.soldier_kills = 11; full.jeep_kills = 3; full.tank_kills = 2;
  full.vehicle_captures = 4; full.boss_kills = 1;
  require(!session.settle(full).succeeded() && session.pending_operation() == SessionOperation::settle,
      "full settlement supersedes unsaved checkpoint");
  require(session.latest_receipt().captures == 300, "retain all final biological counters");
  require(!session.begin_run().succeeded(), "cannot escape pending settlement to guest run");
  backend.read_failure = ReadIssue::none;
  require(session.retry_save().succeeded() && !session.pending(), "retry safely reloads protected checkpoint");
  require(session.account().results.receipt.active_milliseconds == 90000 &&
      session.account().counters.captures == 300 && session.account().counters.military_kills == 16 &&
      session.account().results.receipt.vehicle_captures == 4 &&
      session.account().results.receipt.boss_kills == 1 && session.account().credits == base_credits(full),
      "banks full actual run, never ten-second recovered approximation");
  require(session.account().counters.completed_runs == 1 && !session.account().results.recovered,
      "live run is settled once as actual result");
  require(session.retry_save().succeeded() && session.settle(full).issue == MutationIssue::duplicate &&
      session.account().counters.completed_runs == 1, "repeated recovery cannot double bank");
}
void active_checkpoint_retry_stays_active_and_monotonic() {
  MemoryBackend backend; AccountSession session(backend);
  (void)session.load(); (void)session.begin_run();
  require(session.checkpoint(receipt(session, 10000, 10)).succeeded(), "active baseline");
  backend.read_failure = ReadIssue::unavailable;
  require(!session.checkpoint(receipt(session, 20000, 20)).succeeded(), "active save blocked");
  const auto run_id = session.latest_receipt().run_id;
  require(session.checkpoint({.run_id = run_id, .active_milliseconds = 19000, .captures = 19}).issue ==
      MutationIssue::invalid_receipt, "regressing input cannot overwrite retained counters");
  (void)session.load(); // A lifecycle/UI refresh must not accidentally cold recover.
  backend.read_failure = ReadIssue::none;
  require(session.retry_save().succeeded() && session.account().checkpoint.active_milliseconds == 20000 &&
      session.account().checkpoint.run_id == run_id && session.account().counters.completed_runs == 0,
      "Retry Save preserves actual live run without settlement");
}
void cold_recovery_retry_is_once() {
  MemoryBackend backend;
  { AccountStore original(backend); (void)original.load(); (void)original.begin_run();
    require(original.checkpoint({.run_id = original.account().checkpoint.run_id,
        .active_milliseconds = 90000, .score = 500}).succeeded(), "cold fixture"); }
  backend.write_failure = WriteIssue::quota;
  AccountSession session(backend);
  require(!session.load().playable() && !session.writable(), "failed cold recovery is not guest play");
  backend.write_failure = WriteIssue::none;
  require(session.retry_save().succeeded() && session.account().results.recovered &&
      session.account().counters.completed_runs == 1, "retry cold recovery banks one checkpoint");
  (void)session.retry_save(); (void)session.load();
  require(session.account().counters.completed_runs == 1, "cold recovery never duplicates");
}
void foreign_settlement_is_not_full_settlement() {
  MemoryBackend backend; AccountSession session(backend);
  (void)session.load(); (void)session.begin_run();
  require(session.checkpoint(receipt(session, 10000, 5)).succeeded(), "foreign baseline");
  const auto full = receipt(session, 90000, 300);
  backend.write_failure = WriteIssue::quota;
  require(!session.settle(full).succeeded(), "retain full run");
  backend.write_failure = WriteIssue::none;
  AccountStore foreign(backend);
  require(foreign.load().issue == LoadIssue::recovered_checkpoint, "other process settled old checkpoint");
  require(!session.retry_save().succeeded() && session.pending() && session.latest_receipt().captures == 300,
      "same run ID older result is not success and full receipt stays retained");
  require(session.account().counters.completed_runs == 0, "foreign account not silently adopted");
}
void explicit_verified_reset_only() {
  for (const bool future : {false, true}) {
    MemoryBackend backend;
    const auto bad = future ? encode_envelope(3, {}) : std::vector<std::byte>{std::byte{7}};
    backend.files["account.bin"] = bad;
    AccountSession session(backend);
    require(!session.load().playable() && !session.begin_run().succeeded(), "bad data cannot launch guest");
    require(!session.retry_save().succeeded() && backend.files["account.bin"] == bad,
        "ordinary retry never resets corrupt/future account");
    backend.write_failure = WriteIssue::permission;
    require(!session.preserve_and_reset({"verified-backup.bin"}).succeeded() &&
        backend.files["account.bin"] == bad && !session.writable(), "failed backup never resets original");
    backend.write_failure = WriteIssue::none;
    require(session.preserve_and_reset({"verified-backup.bin"}).succeeded() &&
        backend.files["verified-backup.bin"] == bad && session.begin_run().succeeded(),
        "explicit verified reset permits new run");
  }
  MemoryBackend backend; AccountSession session(backend); (void)session.load();
  require(!session.preserve_and_reset({"backup.bin"}).succeeded(), "healthy account cannot arbitrary reset");
  (void)session.begin_run();
  require(!session.preserve_and_reset({"backup.bin"}).succeeded(), "live run cannot arbitrary reset");
}
void pending_revive_and_bonus_keep_continuation() {
  MemoryBackend backend; AccountSession session(backend);
  (void)session.load(); (void)session.begin_run();
  require(session.checkpoint(receipt(session, 10000, 10)).succeeded(), "revive checkpoint fixture");
  backend.read_failure = ReadIssue::io_failure;
  require(!session.checkpoint(receipt(session, 20000, 20)).succeeded(), "unsaved checkpoint before placement");
  backend.read_failure = ReadIssue::none;
  const auto full = receipt(session, 90000, 300);
  backend.write_failure = WriteIssue::io_failure;
  require(!session.revive(full, PlacementOutcome::completed).succeeded() &&
      session.pending_operation() == SessionOperation::revive && !session.account().revive_used,
      "failed revive records continuation without applying gameplay effect");
  backend.write_failure = WriteIssue::none;
  const auto continuation = session.pending_operation();
  require(session.retry_save().succeeded() && continuation == SessionOperation::revive &&
      session.account().revive_used && !session.pending(), "client may now apply one actual gameplay revive");
  require(session.revive(full, PlacementOutcome::completed).issue == MutationIssue::ineligible,
      "duplicate callback cannot consume another revive");
  require(session.settle(full).succeeded(), "settle revived run");
  const auto credits = session.account().credits;
  backend.write_failure = WriteIssue::quota;
  require(!session.credit_bonus(full.run_id, PlacementOutcome::completed).succeeded() &&
      session.pending_operation() == SessionOperation::bonus, "retain completed placement on save failure");
  backend.write_failure = WriteIssue::none;
  require(session.retry_save().succeeded() && session.account().credits == credits + credits / 2,
      "retry grants exact completed bonus once");
  require(session.credit_bonus(full.run_id, PlacementOutcome::completed).issue == MutationIssue::duplicate,
      "duplicate completed bonus rejected");
}
void vanished_established_save_is_not_fresh_account() {
  MemoryBackend backend; AccountSession session(backend); (void)session.load();
  require(session.update_settings({}).succeeded(), "persist otherwise fresh default account");
  backend.files.erase("account.bin");
  require(!session.begin_run().succeeded() && session.pending(), "deleted original locks mutation");
  require(!session.retry_save().succeeded() && session.pending() &&
      backend.files.find("account.bin") == backend.files.end(), "missing established save never guest transition");
}
class ResetFailureBackend final : public Backend {
public:
  ReadResult read(Scope scope, const RelativePath& path, std::size_t limit) override {
    return memory.read(scope, path, limit);
  }
  WriteResult write_atomically(Scope scope, const RelativePath& path,
      std::span<const std::byte> bytes) override {
    if (path.value == "account.bin" && fail_reset) return {.issue = WriteIssue::io_failure};
    return memory.write_atomically(scope, path, bytes);
  }
  MemoryBackend memory;
  bool fail_reset{true};
};
void verified_backup_then_reset_write_failure_retries() {
  ResetFailureBackend backend;
  const auto original = encode_envelope(3, {});
  backend.memory.files["account.bin"] = original;
  AccountSession session(backend); (void)session.load();
  require(!session.preserve_and_reset({"backup.bin"}).succeeded() &&
      session.pending_operation() == SessionOperation::reset && backend.memory.files["account.bin"] == original &&
      backend.memory.files["backup.bin"] == original, "failed reset preserves verified backup and original");
  backend.fail_reset = false;
  require(session.retry_save().succeeded() && session.writable() && session.begin_run().succeeded(),
      "explicit reset continuation retries only after verified backup");
}
}
int main() {
  failed_full_settlement_replays_instead_of_old_checkpoint();
  active_checkpoint_retry_stays_active_and_monotonic();
  cold_recovery_retry_is_once(); foreign_settlement_is_not_full_settlement();
  explicit_verified_reset_only(); pending_revive_and_bonus_keep_continuation();
  vanished_established_save_is_not_fresh_account();
  verified_backup_then_reset_write_failure_retries();
}
