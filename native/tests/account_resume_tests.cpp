#include <human_resources/gameplay/gameplay.h>
#include <human_resources/progression/account_session.h>

#include "progression_test_backend.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>

namespace {
namespace gp = human_resources::gameplay;
namespace pr = human_resources::progression;
namespace st = human_resources::storage;
using test_support::require;
using progression_tests::MemoryBackend;

pr::RunReceipt receipt(const pr::AccountSession& session, const gp::Game& game) {
  const auto& stats = game.snapshot().stats;
  return {.run_id = session.account().checkpoint.run_id,
      .active_milliseconds = static_cast<std::uint64_t>(stats.active_seconds * 1000),
      .score = stats.score, .captures = stats.captures, .soldier_kills = stats.soldier_kills,
      .jeep_kills = stats.jeep_kills, .tank_kills = stats.tank_kills,
      .vehicle_captures = stats.vehicle_captures, .boss_kills = stats.boss_kills};
}
void start(pr::AccountSession& session, gp::Game& game, const std::uint8_t location = 2,
           const unsigned seed = 9317) {
  require(session.load().playable(), "load native account");
  game.start({.seed = seed, .permanent_ranks = session.account().ranks,
      .unlocked_blueprints = session.account().unlocked_blueprints});
  require(session.begin_run(pr::continuation_for(game, location)).succeeded(), "begin atomically stores initial actual Game and location");
}
void tick(gp::Game& game) {
  if (game.snapshot().phase == gp::Phase::choosing_upgrade)
    require(game.choose_upgrade(0), "actual earned choice");
  else game.advance(0.25F);
}
void play_to(gp::Game& game, const double seconds) {
  for (unsigned frame = 0; frame < 10000 && game.snapshot().stats.active_seconds < seconds; ++frame) {
    require(game.snapshot().phase != gp::Phase::dead, "early checkpoint fixture remains alive");
    tick(game);
  }
  require(game.snapshot().stats.active_seconds >= seconds, "actual native time reaches checkpoint");
}
void cold_resume_does_not_bank_and_end_is_terminal() {
  MemoryBackend backend; pr::AccountSession original(backend); gp::Game game;
  start(original, game); play_to(game, 31);
  const auto checkpoint = receipt(original, game);
  require(original.checkpoint(checkpoint, pr::continuation_for(game, 2), true).succeeded(), "background saves full actual state");
  const auto state = original.account().continuation.game_state;
  pr::AccountSession resumed(backend);
  require(resumed.load().issue == pr::LoadIssue::resumable_run && resumed.account().credits == 0 &&
      resumed.account().counters.completed_runs == 0 && resumed.account().continuation.location == 2,
      "cold active run retains identity, credits, and Desert instead of settling");
  gp::Game restored;
  require(restored.restore_state(resumed.account().continuation.game_state) == gp::SaveStateIssue::none &&
      restored.save_state() == state, "full authoritative state restores exactly");
  restored.pause(); const auto time = restored.snapshot().stats.active_seconds;
  restored.advance(1000);
  require(restored.snapshot().stats.active_seconds == time, "restored pause excludes off-process wall time");
  require(resumed.settle(receipt(resumed, restored)).succeeded(), "explicit End and Bank settles resumed run");
  const auto balance = resumed.account().credits;
  require(resumed.account().continuation.game_state.empty() && balance == pr::base_credits(checkpoint),
      "settlement clears continuation in same account commit");
  pr::AccountSession again(backend);
  require(again.load().issue == pr::LoadIssue::loaded && again.account().credits == balance &&
      again.account().checkpoint.run_id == 0 && again.account().continuation.game_state.empty(),
      "settled run cannot reappear as active after restart");
  require(again.settle(checkpoint).issue == pr::MutationIssue::duplicate, "same resumed receipt banks once");
}
void choice_reroll_and_pick_are_durable_boundaries() {
  MemoryBackend backend; pr::AccountSession session(backend); gp::Game game; start(session, game, 1);
  for (unsigned frame = 0; frame < 2000 && game.snapshot().phase != gp::Phase::choosing_upgrade; ++frame) game.advance(0.25F);
  require(game.snapshot().phase == gp::Phase::choosing_upgrade, "actual first draft earned");
  require(session.checkpoint(receipt(session, game), pr::continuation_for(game, 1), true).succeeded(), "draft entry committed before display");
  pr::AccountSession reopened(backend); require(reopened.load().issue == pr::LoadIssue::resumable_run, "pending draft resumes");
  gp::Game draft; require(draft.restore_state(reopened.account().continuation.game_state) == gp::SaveStateIssue::none,
      "draft identities restore without RNG redraw");
  require(draft.snapshot().phase == gp::Phase::choosing_upgrade && draft.snapshot().choices[0].description == game.snapshot().choices[0].description,
      "same earned options and gain text after termination");
  require(draft.reroll(), "one actual reroll");
  require(reopened.checkpoint(receipt(reopened, draft), pr::continuation_for(draft, 1), true).succeeded(), "reroll and new RNG committed together");
  const auto rerolled = draft.save_state();
  pr::AccountSession third(backend); (void)third.load(); gp::Game after;
  require(after.restore_state(third.account().continuation.game_state) == gp::SaveStateIssue::none && after.save_state() == rerolled &&
      after.snapshot().rerolls_remaining == 1, "restart does not refund reroll or regenerate offers");
  require(after.choose_upgrade(0) && third.checkpoint(receipt(third, after), pr::continuation_for(after, 1), true).succeeded(),
      "accepted pick and surplus XP are durable together");
  pr::AccountSession fourth(backend); (void)fourth.load();
  require(gp::inspect_saved_state(fourth.account().continuation.game_state).stats.level == after.snapshot().stats.level,
      "pick state survives without granting an extra level");
}
void failed_full_checkpoint_retains_exact_bytes() {
  MemoryBackend backend; pr::AccountSession session(backend); gp::Game game; start(session, game);
  play_to(game, 10); require(session.checkpoint(receipt(session, game), pr::continuation_for(game, 2), true).succeeded(), "old checkpoint");
  play_to(game, 21); const auto exact = pr::continuation_for(game, 2);
  backend.read_failure = st::ReadIssue::io_failure;
  require(!session.checkpoint(receipt(session, game), exact, true).succeeded() && session.pending(), "retain failed full continuation");
  game.pause(); backend.read_failure = st::ReadIssue::none;
  require(session.retry_save().succeeded() && session.account().continuation.game_state == exact.game_state &&
      session.account().counters.completed_runs == 0, "protected reload retries exact same full state without settlement");
  pr::AccountSession again(backend);
  require(again.load().issue == pr::LoadIssue::resumable_run && again.account().continuation.game_state == exact.game_state,
      "process death after retry preserves exact latest successful state");
  auto wrong = receipt(again, game); ++wrong.captures;
  require(again.checkpoint(wrong, exact, true).issue == pr::MutationIssue::invalid_receipt,
      "receipt and gameplay bytes cannot diverge");
  auto other_location = exact; other_location.location = 1;
  require(again.checkpoint(receipt(again, game), other_location, true).issue == pr::MutationIssue::invalid_receipt,
      "active run location cannot be silently changed");
  require(again.checkpoint(receipt(again, game), exact, true).succeeded() && !again.pending(),
      "rejected mismatched data never poisons the latest valid receipt or continuation");
}
void full_settlement_supersedes_failed_checkpoint_without_losing_authority() {
  MemoryBackend backend; pr::AccountSession session(backend); gp::Game game; start(session, game);
  play_to(game, 10);
  require(session.checkpoint(receipt(session, game), pr::continuation_for(game, 2), true).succeeded(), "last durable full checkpoint");
  play_to(game, 31); const auto full = receipt(session, game); const auto state = pr::continuation_for(game, 2);
  auto invented = full; ++invented.score;
  require(session.settle(invented).issue == pr::MutationIssue::invalid_receipt,
      "a full run cannot bank counters absent from its actual retained Game state");
  backend.read_failure = st::ReadIssue::io_failure;
  require(!session.checkpoint(full, state, true).succeeded(), "retain newer full checkpoint after read protection");
  require(!session.settle(full).succeeded() && session.pending_operation() == pr::SessionOperation::settle,
      "intentional End and Bank supersedes failed checkpoint while retaining full gameplay authority");
  backend.read_failure = st::ReadIssue::none;
  require(session.retry_save().succeeded() && session.account().results.receipt.active_milliseconds == full.active_milliseconds &&
      session.account().credits == pr::base_credits(full) && session.account().continuation.game_state.empty(),
      "retry banks exact final run rather than old ten-second approximation");
  pr::AccountSession again(backend);
  require(again.load().issue == pr::LoadIssue::loaded && again.account().counters.completed_runs == 1,
      "full settlement continuation is terminal and exact-once");
}
void dead_checkpoint_cannot_resume_or_revive() {
  MemoryBackend backend; pr::AccountSession session(backend); gp::Game game; start(session, game);
  for (unsigned frame = 0; frame < 20000 && game.snapshot().phase != gp::Phase::dead; ++frame) tick(game);
  require(game.snapshot().phase == gp::Phase::dead, "real native run naturally dies");
  const auto dead = receipt(session, game);
  require(session.checkpoint(dead, pr::continuation_for(game, 2), true).succeeded(), "death phase is persisted before interruption");
  pr::AccountSession reopened(backend);
  require(reopened.load().issue == pr::LoadIssue::recovered_checkpoint &&
      reopened.account().results.receipt.run_id == dead.run_id && reopened.account().continuation.game_state.empty(),
      "cold dead checkpoint banks to results rather than restoring live hull");
  require(reopened.account().credits == pr::base_credits(dead) && !reopened.store().revive_available(dead),
      "dead run preserves exact earned reward and cannot revive through restart");
}
void completed_revive_commits_the_actual_live_continuation() {
  // Earn eligibility through ordinary forward weaving and ready Dash, then
  // release the controls after 91 seconds and let actual combat end the run.
  MemoryBackend backend; pr::AccountSession session(backend); gp::Game game; start(session, game, 2, 1);
  for (unsigned frame = 0; frame < 20000 && game.snapshot().phase != gp::Phase::dead; ++frame) {
    if (game.snapshot().phase == gp::Phase::choosing_upgrade) {
      require(game.choose_upgrade(0), "actual earned choice in revive fixture");
      continue;
    }
    gp::Control control{};
    if (game.snapshot().stats.active_seconds < 91) {
      control.movement = {static_cast<float>(std::sin(game.snapshot().stats.active_seconds * .8)), 1};
      control.dash = game.snapshot().dash_cooldown_seconds <= 0;
    }
    game.advance(.25F, control);
  }
  require(game.snapshot().phase == gp::Phase::dead && game.revive_available(), "eligible real death fixture");
  const auto dead = receipt(session, game);
  gp::Game candidate;
  require(candidate.restore_state(game.save_state()) == gp::SaveStateIssue::none && candidate.revive(),
      "prepare actual revived Game in an isolated candidate");
  const auto revived = pr::continuation_for(candidate, 2);
  backend.write_failure = st::WriteIssue::quota;
  require(!session.revive(dead, revived, pr::PlacementOutcome::completed).succeeded() &&
      session.pending_operation() == pr::SessionOperation::revive && !session.account().revive_used,
      "failed revived state commit retains outcome without publishing a consumed-only reward");
  backend.write_failure = st::WriteIssue::none;
  require(session.retry_save().succeeded() && session.account().revive_used, "retry atomically commits revived authority and consumption");
  pr::AccountSession cold(backend);
  require(cold.load().issue == pr::LoadIssue::resumable_run, "termination before publishing live candidate still restores earned revive");
  gp::Game recovered;
  require(recovered.restore_state(cold.account().continuation.game_state) == gp::SaveStateIssue::none &&
      recovered.snapshot().phase != gp::Phase::dead && recovered.snapshot().hull > 0 && recovered.snapshot().shield == 0 &&
      gp::inspect_saved_state(cold.account().continuation.game_state).revive_count == 1,
      "durable revived hull, zero shield and one consumed revive survive restart together");
  require(cold.revive(dead, revived, pr::PlacementOutcome::completed).issue == pr::MutationIssue::ineligible,
      "duplicate completed revive cannot grant another life");
}
std::vector<std::byte> legacy_payload(const pr::Account& account) {
  require(account.continuation.game_state.empty(), "legacy fixture cannot contain new Game bytes");
  auto bytes = pr::encode_account(account);
  require(bytes.size() >= 40, "current account prefix exists");
  bytes.resize(bytes.size() - 40); // Five schema2 tail values; schema1 prefix stays byte-exact.
  bytes[8] = std::byte{1};
  return bytes;
}
void legacy_migration_and_future_protection() {
  MemoryBackend source; pr::AccountSession old(source); (void)old.load(); (void)old.begin_run();
  const pr::RunReceipt run{.run_id = old.account().checkpoint.run_id, .active_milliseconds = 90000, .score = 500};
  require(old.checkpoint(run, true).succeeded(), "legacy receipt fixture");
  MemoryBackend backend; backend.files["account.bin"] = st::encode_envelope(1, legacy_payload(old.account()));
  const auto original = backend.files["account.bin"];
  backend.write_failure = st::WriteIssue::quota;
  pr::AccountSession migrated(backend);
  require(!migrated.load().playable() && backend.files["account.bin"] == original, "failed migration preserves original schema1 bytes");
  backend.write_failure = st::WriteIssue::none;
  require(migrated.retry_save().succeeded() && migrated.account().credits == 111 && migrated.account().results.recovered &&
      st::decode_envelope(backend.files["account.bin"], 2, 65536).valid(), "legacy checkpoint settles once and atomically migrates to schema2");
  pr::AccountSession again(backend);
  require(again.load().issue == pr::LoadIssue::loaded && again.account().credits == 111, "migration never duplicates settlement");
  backend.files["account.bin"] = st::encode_envelope(3, {});
  pr::AccountSession future(backend);
  require(future.load().issue == pr::LoadIssue::unsupported_schema && !future.begin_run().succeeded(), "future envelope stays protected");
}
class RecoveryFailureBackend final : public st::Backend {
public:
  MemoryBackend memory;
  bool fail_document{};
  st::ReadResult read(st::Scope scope, const st::RelativePath& path, std::size_t limit) override { return memory.read(scope, path, limit); }
  st::WriteResult write_atomically(st::Scope scope, const st::RelativePath& path, std::span<const std::byte> bytes) override {
    if (fail_document && path.value == "account.bin") return {.issue = st::WriteIssue::io_failure};
    return memory.write_atomically(scope, path, bytes);
  }
};
void incompatible_run_backs_up_before_recorded_settlement(bool old_revision) {
  RecoveryFailureBackend backend; pr::AccountSession original(backend); gp::Game game; start(original, game);
  play_to(game, 31); require(original.checkpoint(receipt(original, game), pr::continuation_for(game, 2), true).succeeded(), "compatible fixture");
  auto account = original.account();
  if (old_revision) {
    // A former Game authority header remains protected even inside the current
    // account envelope. It cannot be reinterpreted with newly added actor fields.
    account.continuation.game_state[8] = std::byte{1};
    for (std::size_t i = 9; i < 12; ++i) account.continuation.game_state[i] = std::byte{};
  } else
    account.continuation.game_state[12] ^= std::byte{1}; // Known Game header's numeric-rule fingerprint.
  const auto recorded = account.continuation.earned_base_credits;
  backend.memory.files["account.bin"] = st::encode_envelope(2, pr::encode_account(account));
  const auto incompatible = backend.memory.files["account.bin"];
  pr::AccountSession blocked(backend);
  require(blocked.load().issue == pr::LoadIssue::incompatible_continuation && !blocked.writable() &&
      !blocked.begin_run().succeeded(), "known account with incompatible Game remains protected");
  backend.fail_document = true;
  require(!blocked.preserve_and_bank_checkpoint({"incompatible-backup.bin"}).succeeded() && blocked.pending() &&
      backend.memory.files["incompatible-backup.bin"] == incompatible && backend.memory.files["account.bin"] == incompatible,
      "backup verifies before a failed replacement and preserves both exact originals");
  backend.fail_document = false;
  pr::AccountSession after_process_death(backend);
  require(after_process_death.load().issue == pr::LoadIssue::incompatible_continuation &&
      after_process_death.preserve_and_bank_checkpoint({"incompatible-backup.bin"}).succeeded(),
      "existing identical verified backup permits retry after process death");
  require(after_process_death.account().credits == recorded && after_process_death.account().results.recovered &&
      after_process_death.account().continuation.game_state.empty(), "fallback banks stored historical reward without resuming incompatible rules");
  pr::AccountSession again(backend);
  require(again.load().issue == pr::LoadIssue::loaded && again.account().credits == recorded,
      "fallback cannot duplicate settlement on another launch");
}
void historical_results_do_not_recompute_current_reward() {
  MemoryBackend backend; pr::AccountSession session(backend); (void)session.load(); (void)session.begin_run();
  const pr::RunReceipt run{.run_id = session.account().checkpoint.run_id, .active_milliseconds = 90000, .score = 500};
  require(session.settle(run).succeeded(), "historical fixture");
  auto historical = session.account();
  historical.results.base_credits = 120; historical.results.bonus_credits = 60; historical.results.bonus_consumed = true;
  historical.counters.base_credits = 120; historical.credits = 180;
  auto bytes = pr::encode_account(historical);
  require(!bytes.empty(), "bounded recorded reward can differ from today's formula");
  backend.files["account.bin"] = st::encode_envelope(2, bytes);
  pr::AccountSession reopened(backend);
  require(reopened.load().playable() && reopened.account().credits == 180 && reopened.account().results.base_credits == 120,
      "historical reward and completed bonus remain exact after tuning changes");
  historical.results.bonus_credits = 59;
  require(pr::encode_account(historical).empty(), "contradictory policy1 bonus still rejected");
}
void malformed_current_payload_is_not_a_future_schema() {
  MemoryBackend source; pr::AccountSession original(source); (void)original.load();
  const auto payload = pr::encode_account(original.account());
  for (std::size_t size = 0; size < payload.size(); ++size) {
    MemoryBackend backend;
    backend.files["account.bin"] = st::encode_envelope(2, std::span(payload).first(size));
    pr::AccountSession session(backend);
    require(session.load().issue == pr::LoadIssue::corrupt && !session.writable(),
        "every truncated current account is corrupt rather than silently fresh or future");
  }
}
}
int main() {
  cold_resume_does_not_bank_and_end_is_terminal(); choice_reroll_and_pick_are_durable_boundaries();
  failed_full_checkpoint_retains_exact_bytes(); dead_checkpoint_cannot_resume_or_revive();
  full_settlement_supersedes_failed_checkpoint_without_losing_authority();
  completed_revive_commits_the_actual_live_continuation();
  legacy_migration_and_future_protection();
  incompatible_run_backs_up_before_recorded_settlement(false);
  incompatible_run_backs_up_before_recorded_settlement(true);
  historical_results_do_not_recompute_current_reward();
  malformed_current_payload_is_not_a_future_schema();
}
