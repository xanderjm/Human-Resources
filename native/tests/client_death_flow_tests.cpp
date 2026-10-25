// Exercise the real composition-root transition with memory-only storage. No
// test API, SDL window, devices, account path or process input is introduced.
#define SDL_MAIN_HANDLED 1
#include <SDL3/SDL.h>
#include <human_resources/platform/sdl_platform.h>
#include "progression_test_backend.h"
#include "test_support.h"
#include "../human_resources_platform/src/product_haptics_backend.h"

namespace death_flow_haptics {
struct State final {bool active{}, pending{}; unsigned deaths{}, cancelled{};};
State state;
class Backend final : public human_resources::platform::detail::HapticBackend {
public:
  bool available() const noexcept override { return true; }
  void set_active(bool active) noexcept override {
    if (!active && state.pending) ++state.cancelled;
    state.active=active;
    if (!active) state.pending=false;
  }
  void play(human_resources::feedback::HapticCue cue) noexcept override {
    if (state.active && cue == human_resources::feedback::HapticCue::death) { ++state.deaths;state.pending=true; }
  }
};
}
namespace human_resources::platform::detail {
std::unique_ptr<HapticBackend> create_haptic_backend() { return std::make_unique<death_flow_haptics::Backend>(); }
}

namespace human_resources::platform {
class DeathFlowStorageBackend final : public storage::Backend {
public:
  progression_tests::MemoryBackend memory;
  std::size_t fail_write{};
  DeathFlowStorageBackend(std::string_view, std::string_view) {}
  explicit DeathFlowStorageBackend(std::string) {}
  storage::ReadResult read(storage::Scope scope, const storage::RelativePath& path, std::size_t limit) override {
    return memory.read(scope, path, limit);
  }
  storage::WriteResult write_atomically(storage::Scope scope, const storage::RelativePath& path, std::span<const std::byte> bytes) override {
    const auto failure = memory.write_failure;
    if (fail_write && memory.writes + 1 == fail_write)
      memory.write_failure = storage::WriteIssue::io_failure;
    const auto result = memory.write_atomically(scope, path, bytes);
    memory.write_failure = failure;
    return result;
  }
};
}
#define SdlStorageBackend DeathFlowStorageBackend
#include "../human_resources_client/src/app.cpp"
#undef SdlStorageBackend
#include "../human_resources_client/src/resume_qa.cpp"

namespace {
using test_support::require;
void begin(App& app) {
  require(app.store.load().playable(), "memory-only actual App account loads");
  app.qa = true; app.qa_seed = 21;
  start(app);
  require(app.screen == Screen::run && app.run_id && !app.save_error, "actual App durable start publishes fresh run");
}
void natural_death_before_routing(App& app) {
  begin(app);
  for (unsigned tick=0; tick<36000; ++tick) {
    const auto phase=app.game.snapshot().phase;
    if (phase == hr::gameplay::Phase::choosing_upgrade) {
      dispatch(app, Command::pick0); // Actual earned offer, no fixture upgrades.
      app.promotion.advance(1.F);
      continue;
    }
    require(phase == hr::gameplay::Phase::active, "natural-death fixture remains active until lethal public Game step");
    app.game.advance(1.F/60);
    if (app.game.snapshot().phase == hr::gameplay::Phase::dead)
      return; // Caller injects failure before this step's real post-advance route.
    complete_flight_advance(app, phase);
    require(!app.save_error, "ordinary actual App checkpoints succeed before failure injection");
  }
  require(false, "bounded idle public Game reaches a natural death without health overrides");
}
void saved_once_and_next_flight() {
  App app; natural_death_before_routing(app);
  const auto ended=receipt(app);
  const auto expected=hr::progression::base_credits(ended);
  const auto before=app.backend.memory.writes;
  complete_flight_advance(app, hr::gameplay::Phase::active);
  require(app.screen == Screen::results && !app.save_error && !app.store.pending() && !app.run_id, "natural death routes straight through durable finish to saved report");
  require(app.backend.memory.writes == before + 2, "one boundary checkpoint and one settlement, without per-frame duplicate write");
  require(app.store.account().credits == expected && app.store.account().results.receipt.run_id == ended.run_id, "actual ended receipt is credited exactly once");
  const auto saved=app.backend.memory.files;
  complete_flight_advance(app, hr::gameplay::Phase::active);
  complete_flight_advance(app, hr::gameplay::Phase::dead);
  require(app.backend.memory.writes == before+2 && app.backend.memory.files == saved, "report frames never settle or checkpoint again");
  dispatch(app, Command::retry);
  require(app.screen == Screen::run && app.run_id && app.run_id != ended.run_id && app.game.snapshot().stats.active_seconds == 0, "saved loss can begin the next actual durable flight directly");
}
void checkpoint_failure_keeps_loss_context_and_retry_banks_once() {
  App app; natural_death_before_routing(app);
  const auto ended=receipt(app);const auto expected=hr::progression::base_credits(ended);
  const auto files=app.backend.memory.files;
  app.backend.memory.write_failure=hr::storage::WriteIssue::io_failure;
  complete_flight_advance(app, hr::gameplay::Phase::active);
  require(app.screen == Screen::death && app.save_error && app.bank_after_checkpoint && app.store.pending_operation() == hr::progression::SessionOperation::checkpoint, "failed death boundary stays in loss context with retained bank-after-checkpoint intent");
  require(app.game.snapshot().phase == hr::gameplay::Phase::dead && app.backend.memory.files == files && app.store.account().credits == 0, "checkpoint failure preserves prior durable bytes and no reward is claimed");
  const auto writes=app.backend.memory.writes;
  complete_flight_advance(app, hr::gameplay::Phase::active);
  dispatch(app, Command::retry);dispatch(app, Command::finish);
  require(app.backend.memory.writes == writes, "protected failure context rejects another flight or finish");
  app.backend.memory.write_failure=hr::storage::WriteIssue::none;
  retry_save(app);
  require(app.screen == Screen::results && !app.save_error && !app.bank_after_checkpoint && !app.store.pending() && !app.run_id && app.store.account().credits == expected, "Retry completes retained checkpoint then settlement once");
  const auto final=app.backend.memory.files;const auto final_writes=app.backend.memory.writes;
  retry_save(app);complete_flight_advance(app, hr::gameplay::Phase::active);
  require(app.backend.memory.files == final && app.backend.memory.writes == final_writes, "repeated retry/report cannot award twice");
}
void settlement_failure_retains_receipt_for_retry() {
  App app; natural_death_before_routing(app);
  const auto ended=receipt(app);const auto expected=hr::progression::base_credits(ended);
  const auto writes=app.backend.memory.writes;
  app.backend.fail_write=writes+2; // Boundary succeeds, settlement alone fails.
  complete_flight_advance(app, hr::gameplay::Phase::active);
  require(app.screen == Screen::results && app.save_error && app.pending_settlement && app.store.pending_operation() == hr::progression::SessionOperation::settle, "settlement failure retains existing pending Results protection");
  require(app.pending_receipt.run_id == ended.run_id && app.pending_receipt.active_milliseconds == ended.active_milliseconds && app.pending_receipt.score == ended.score && app.pending_receipt.captures == ended.captures && app.run_id == ended.run_id && app.store.account().credits == 0, "actual pending receipt/run survive failed settlement without optimistic bank");
  const auto protected_files=app.backend.memory.files;
  dispatch(app, Command::retry);complete_flight_advance(app, hr::gameplay::Phase::active);
  require(app.backend.memory.files == protected_files && app.backend.memory.writes == writes+2, "pending settlement cannot restart or resettle per frame");
  app.backend.fail_write=0;retry_save(app);
  require(app.screen == Screen::results && !app.save_error && !app.pending_settlement && !app.store.pending() && !app.run_id && app.store.account().credits == expected, "Retry settles the retained actual receipt once");
  const auto final=app.backend.memory.files;retry_save(app);
  require(app.backend.memory.files == final && app.store.account().credits == expected, "retry after settlement has no second reward");
}
void generic_dead_restore_and_pause_banking_are_explicit() {
  App app; natural_death_before_routing(app);
  const auto before=app.backend.memory.files;const auto writes=app.backend.memory.writes;
  complete_flight_advance(app, hr::gameplay::Phase::dead);
  require(app.screen == Screen::run && app.backend.memory.files == before && app.backend.memory.writes == writes, "an already-dead Game even under Run cannot pretend an accepted active advance");
  require(checkpoint_game(app,true), "retain actual dead continuation for cold restore");
  require(publish_saved_game(app,RestorePurpose::recovery_prompt) && app.screen == Screen::pause && app.restored_run, "actual dead continuation retains explicit recovery Pause");
  const auto restored=app.backend.memory.files;const auto restored_writes=app.backend.memory.writes;
  complete_flight_advance(app,hr::gameplay::Phase::active);
  require(app.screen == Screen::pause && app.backend.memory.files == restored && app.backend.memory.writes == restored_writes, "generic restored dead state does not auto-bank");
  dispatch(app,Command::finish);
  require(app.screen == Screen::results && !app.run_id && !app.save_error, "explicit restored banking still completes existing durable finish");
  App paused;begin(paused);
  paused.game.advance(1.F/60);dispatch(paused,Command::pause);
  require(paused.screen == Screen::pause && paused.run_id && paused.store.account().credits == 0, "ordinary Pause still does not finish a flight");
  dispatch(paused,Command::finish);
  require(paused.screen == Screen::results && !paused.run_id && !paused.save_error && paused.game.snapshot().phase == hr::gameplay::Phase::paused, "explicit Pause End remains available and is not a natural loss");
}
void guest_keeps_existing_ephemeral_finish() {
  App app;natural_death_before_routing(app);app.guest=true;
  complete_flight_advance(app,hr::gameplay::Phase::active);
  require(app.screen == Screen::results && !app.save_error && !app.run_id, "guest still follows its existing normal ephemeral session finish");
  hr::product::View view{Screen::results,&app.game.snapshot(),&app.store.store(),{0,0,390,844},{0,20,0,0}};
  view.guest=true;
  const auto nodes=hr::product::accessibility_snapshot(view,hr::product::interface(view));
  require(nodes.size()>1 && nodes[1].value.find("no saved credits") != std::string::npos, "guest saved report never promises persistent rewards");
}
void checkpoint_retry_can_fail_settlement_without_losing_receipt() {
  App app;natural_death_before_routing(app);
  const auto ended=receipt(app);const auto expected=hr::progression::base_credits(ended);
  app.backend.memory.write_failure=hr::storage::WriteIssue::io_failure;
  complete_flight_advance(app,hr::gameplay::Phase::active);
  require(app.bank_after_checkpoint, "checkpoint failure retains end intent");
  app.backend.memory.write_failure=hr::storage::WriteIssue::none;
  app.backend.fail_write=app.backend.memory.writes+2;
  retry_save(app);
  require(app.screen == Screen::results && app.pending_settlement && app.save_error && app.store.pending_operation() == hr::progression::SessionOperation::settle && app.store.account().credits == 0, "retry's checkpoint success followed by settlement failure retains protected pending results");
  require(app.pending_receipt.run_id == ended.run_id && app.pending_receipt.active_milliseconds == ended.active_milliseconds, "second-boundary failure retains the original full death receipt");
  app.backend.fail_write=0;retry_save(app);
  require(!app.save_error && !app.run_id && app.store.account().credits == expected && !app.pending_settlement, "second Retry banks exactly once after both boundary failures");
}
void natural_report_haptic_queue_lifetime_and_cancellation() {
  using death_flow_haptics::state;
  App app;natural_death_before_routing(app);
  state={};require(app.haptics.initialize(), "private existing backend factory provides observable queue lifetime");
  haptics_policy(app);require(state.active, "active flight enables actual App haptic policy");
  complete_flight_advance(app,hr::gameplay::Phase::active);
  require(app.screen == Screen::results && state.deaths == 1 && state.pending && state.active && state.cancelled == 0, "immediate saved report does not cancel queued death feedback");
  app.report_entry_seconds=.15F;haptics_policy(app);
  require(state.pending && state.active, "existing150ms platform queue remains eligible through report entry");
  app.audio_focused=false;haptics_policy(app);
  require(!state.pending && !state.active && state.cancelled == 1, "focus loss still cancels queued feedback immediately");
  app.audio_focused=true;app.report_entry_seconds=.169F;haptics_policy(app);
  require(state.active && !state.pending && state.deaths == 1, "foreground return cannot replay a cancelled death");
  app.report_entry_seconds=.17F;haptics_policy(app);
  require(!state.active, "natural-loss report haptic eligibility expires without a UI hold");
  app.report_entry_seconds=0;app.lifecycle.presence=hr::lifecycle::Presence::background;haptics_policy(app);
  require(!state.active, "background gating overrides short report entry");
  app.lifecycle.presence=hr::lifecycle::Presence::foreground;app.save_error=true;haptics_policy(app);
  require(!state.active, "save protection overrides short report entry");
  app.save_error=false;dispatch(app,Command::retry);
  require(app.report_entry_seconds<0, "new flight resets disposable report timing");
}
}
#undef main
int main() {
  saved_once_and_next_flight();
  checkpoint_failure_keeps_loss_context_and_retry_banks_once();
  settlement_failure_retains_receipt_for_retry();
  generic_dead_restore_and_pause_banking_are_explicit();
  guest_keeps_existing_ephemeral_finish();
  checkpoint_retry_can_fail_settlement_without_losing_receipt();
  natural_report_haptic_queue_lifetime_and_cancellation();
}
