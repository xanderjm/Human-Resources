#include <human_resources/gameplay/gameplay.h>
#include <human_resources/gameplay/tuning.h>
#include <human_resources/progression/progression.h>

#include "progression_test_backend.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>

namespace {
namespace gp = human_resources::gameplay;
namespace pr = human_resources::progression;
using test_support::require;

pr::RunReceipt receipt(const pr::AccountStore& store, const gp::Game& game) {
  const auto& stats = game.snapshot().stats;
  return {.run_id = store.account().checkpoint.run_id,
      .active_milliseconds = static_cast<std::uint64_t>(std::llround(stats.active_seconds * 1000)),
      .score = stats.score, .captures = stats.captures, .soldier_kills = stats.soldier_kills,
      .jeep_kills = stats.jeep_kills, .tank_kills = stats.tank_kills,
      .vehicle_captures = stats.vehicle_captures, .boss_kills = stats.boss_kills};
}
void start(pr::AccountStore& store, gp::Game& game, const std::uint64_t seed) {
  require(store.begin_run().succeeded(), "durable run identity before play");
  game.start({.seed = seed, .permanent_ranks = store.account().ranks,
      .unlocked_blueprints = store.account().unlocked_blueprints});
}
void tick(pr::AccountStore& store, gp::Game& game) {
  if (game.snapshot().phase == gp::Phase::choosing_upgrade) {
    require(game.choose_upgrade(0), "real earned draft accepts its first choice");
  } else {
    game.advance(0.25F);
  }
  require(store.checkpoint(receipt(store, game)).succeeded(), "real active counters checkpoint");
}
void real_play_bank_purchase_retry() {
  progression_tests::MemoryBackend backend;
  pr::AccountStore store(backend);
  require(store.load().playable(), "fresh native account playable");
  gp::Game game;
  bool captured{}, fought{}, revived{};
  std::uint64_t completed{};
  for (std::uint64_t attempt = 0; attempt < 12 && store.account().credits < 80; ++attempt) {
    start(store, game, 17 + attempt);
    const auto initial_time = game.snapshot().stats.active_seconds;
    game.pause(); game.advance(30);
    require(game.snapshot().stats.active_seconds == initial_time, "pause wall time never earns rewards");
    game.resume();
    bool died{};
    for (unsigned frame = 0; frame < 20000; ++frame) {
      tick(store, game);
      const auto& view = game.snapshot();
      captured = captured || view.stats.captures > 0;
      fought = fought || view.stats.soldier_kills + view.stats.jeep_kills + view.stats.tank_kills > 0;
      if (view.phase != gp::Phase::dead) continue;
      const auto dead_receipt = receipt(store, game);
      if (!revived && game.revive_available()) {
        require(store.revive_available(dead_receipt), "runtime and durable revive eligibility agree");
        const auto failed = pr::LocalPlacement{pr::PlacementOutcome::failed}.complete();
        require(!store.revive(dead_receipt, failed).succeeded() && game.snapshot().phase == gp::Phase::dead,
            "failed local placement does not revive or consume the offer");
        const auto completed_placement = pr::LocalPlacement{pr::PlacementOutcome::completed}.complete();
        require(store.revive(dead_receipt, completed_placement).succeeded() && game.revive(),
            "one completed local placement persists before real revive");
        revived = true;
        require(game.snapshot().shield == 0 && game.snapshot().stats.active_seconds == view.stats.active_seconds,
            "real revive clears shield and preserves active clock");
        continue;
      }
      const auto expected = pr::base_credits(dead_receipt);
      const auto balance = store.account().credits;
      require(store.settle(dead_receipt).succeeded(), "real death banks measured gameplay rewards");
      require(store.account().credits == balance + expected, "bank matches exact receipt formula");
      require(store.settle(dead_receipt).issue == pr::MutationIssue::duplicate, "repeated death callback never duplicates rewards");
      if (store.credit_bonus_available()) {
        const auto counters = store.account().counters;
        require(store.credit_bonus(dead_receipt.run_id, pr::PlacementOutcome::completed).succeeded(), "local results reward completes");
        require(store.account().counters.base_credits == counters.base_credits &&
            store.account().counters.captures == counters.captures, "bonus does not advance achievements");
      }
      ++completed; died = true; break;
    }
    require(died, "stationary real gameplay reaches death without a test-only kill hook");
  }
  require(captured && fought && completed > 0, "full actual play captures, fights and ends");
  require(store.account().credits >= 80, "actual repeated play funds first purchase");
  const auto before = store.account().credits;
  require(store.purchase(pr::Track::damage).succeeded() && store.account().credits == before - 80, "optional Hangar purchase exact cost");
  pr::AccountStore reopened(backend);
  require(reopened.load().playable() && reopened.account().ranks[0] == 1, "purchased rank survives app restart");
  start(reopened, game, 701);
  const auto& next = game.snapshot();
  require(std::fabs(next.weapon_damage - gp::tuning::base_laser_damage *
      (1 + gp::tuning::permanent_per_rank[0])) < 0.001F, "retry consumes purchased native weapon bonus");
  require(next.stats.level == 1 && next.xp == 0 && next.rerolls_remaining == gp::tuning::rerolls &&
      next.evolution_bits == 0 && std::all_of(next.modules.begin(), next.modules.end(), [](auto rank) { return rank == 0; }),
      "retry resets complete temporary build");
  require(next.beam_capacity == 1 && next.laser_count == 1, "Hangar never adds permanent lasers or beams");
}
void real_background_checkpoint_recovers_once() {
  progression_tests::MemoryBackend backend; pr::AccountStore store(backend); (void)store.load();
  gp::Game game; start(store, game, 9001);
  for (unsigned frame = 0; frame < 2000 && game.snapshot().stats.active_seconds < 31; ++frame) {
    require(game.snapshot().phase != gp::Phase::dead, "checkpoint scenario survives first thirty active seconds");
    tick(store, game);
  }
  require(store.checkpoint(receipt(store, game), true).succeeded(), "background flushes actual run checkpoint");
  const auto expected = pr::base_credits(receipt(store, game));
  pr::AccountStore recovered(backend);
  require(recovered.load().issue == pr::LoadIssue::recovered_checkpoint && recovered.account().credits == expected,
      "process death settles last real checkpoint to results");
  pr::AccountStore again(backend);
  require(again.load().issue == pr::LoadIssue::loaded && again.account().credits == expected,
      "relaunch preserves recovered actual reward once");
}
}
int main() {
  real_play_bank_purchase_retry();
  real_background_checkpoint_recovers_once();
}
