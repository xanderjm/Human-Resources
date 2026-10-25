#include <human_resources/progression/progression.h>
#include <human_resources/gameplay/tuning.h>

#include "progression_test_backend.h"
#include "test_support.h"

namespace {
using namespace human_resources::progression;
using test_support::require;
using progression_tests::MemoryBackend;

RunReceipt earned_run(AccountStore& store) {
  require(store.begin_run().succeeded(), "begin run saves unique receipt identity");
  return {.run_id = store.account().checkpoint.run_id, .active_milliseconds = 3600000,
      .score = 1500, .captures = 710, .soldier_kills = 60, .jeep_kills = 4,
      .tank_kills = 3, .vehicle_captures = 2, .boss_kills = 2};
}
void rewards_and_achievements_are_exactly_once() {
  MemoryBackend backend; AccountStore store(backend);
  require(store.load().issue == LoadIssue::missing, "absence is explicit");
  require(store.account().unlocked_blueprints == human_resources::gameplay::tuning::starter_blueprints, "all ten starter blueprints");
  const auto run = earned_run(store);
  require(base_credits(run) == 778, "exact candidate reward formula");
  require(store.settle(run).succeeded(), "settles run");
  require(store.account().credits == 778 && store.account().counters.completed_runs == 1, "banks base once");
  require(store.account().unlocked_blueprints == (65535U & ~(1U << 9)), "earned achievements automatically unlock five; reserve waits for 800 base credits");
  require(store.account().results.newly_unlocked != 0, "results show new unlocks");
  require(store.settle(run).issue == MutationIssue::duplicate && store.account().credits == 778, "duplicate death does not bank twice");
  for (const auto outcome : {PlacementOutcome::cancelled, PlacementOutcome::unavailable, PlacementOutcome::failed})
    require(store.credit_bonus(run.run_id, outcome).issue == MutationIssue::placement_incomplete, "incomplete placement adds nothing");
  require(store.credit_bonus(run.run_id, PlacementOutcome::completed).succeeded(), "completed credit placement");
  require(store.account().credits == 1167 && store.account().counters.base_credits == 778, "half bonus excludes achievement credits");
  require(store.credit_bonus(run.run_id, PlacementOutcome::completed).issue == MutationIssue::duplicate, "duplicate placement ignored");
  const auto view = store.blueprints();
  require(view[0].unlocked && view[0].progress == view[0].threshold && !view[0].requirement.empty(), "unlock progress/prerequisites portable");
  require(store.begin_run().succeeded(), "immediate retry starts fresh receipt");
  require(store.credit_bonus(run.run_id, PlacementOutcome::completed).issue == MutationIssue::invalid_receipt, "stale placement cannot affect later run");
}
void hangar_costs_caps_and_run_isolation() {
  MemoryBackend backend; AccountStore store(backend); (void)store.load();
  for (int i = 0; i < 10; ++i) { const auto run = earned_run(store); require(store.settle(run).succeeded(), "fund hangar"); }
  constexpr std::array<std::uint64_t, 5> costs{80, 140, 240, 400, 640};
  const auto before = store.account().credits;
  for (unsigned track = 0; track < 4; ++track) {
    unsigned expected_rank = 0;
    for (const auto cost : costs) {
      const auto view = store.hangar()[track];
      require(view.cost == cost, "exact next rank cost");
      constexpr std::array<unsigned, 4> gains{5, 6, 4, 3};
      const auto suffix = track == 0 || track == 3 ? "%" : "";
      require(view.owned_bonus == "+" + std::to_string(expected_rank * gains[track]) + suffix &&
          view.next_bonus == "+" + std::to_string((expected_rank + 1) * gains[track]) + suffix,
          "all owned and next totals follow actual permanent ranks, including zero and final purchase");
      require(!view.bonus_basis.empty(), "bonus retains original damage, starting/max pool or base capture basis");
      require(store.purchase(static_cast<Track>(track)).succeeded(), "purchase rank");
      ++expected_rank;
    }
    require(store.purchase(static_cast<Track>(track)).issue == MutationIssue::capped, "five rank cap");
    require(store.hangar()[track].cost == 0, "max rank no cost");
    require(store.hangar()[track].next_bonus.empty() && !store.hangar()[track].owned_bonus.empty(),
        "capped track keeps its owned power and never advertises a sixth purchase");
  }
  require(before - store.account().credits == 6000, "all twenty purchases total six thousand");
  require(!store.credit_bonus_available(), "hide bonus when all remaining upgrades affordable");
  require(store.begin_run().succeeded(), "start with permanent stats");
  require(store.purchase(Track::hull).issue == MutationIssue::run_active, "never purchase during run");
  require(store.account().results.receipt.run_id == 0, "retry clears stale results");
}
void active_checkpoints_revive_and_minimum_reward() {
  MemoryBackend backend; AccountStore store(backend); (void)store.load(); (void)store.begin_run();
  RunReceipt run{.run_id = store.account().checkpoint.run_id, .active_milliseconds = 9999};
  const auto writes = backend.writes;
  require(store.checkpoint(run).succeeded() && backend.writes == writes, "no frame-by-frame disk writes");
  run.active_milliseconds = 10000;
  require(store.checkpoint(run).succeeded() && backend.writes == writes + 1, "ten active seconds checkpoints");
  run.active_milliseconds = 10001;
  require(store.checkpoint(run, true).succeeded() && backend.writes == writes + 2, "background checkpoint flushes latest active time");
  require(!store.revive_available(run), "revive threshold is active ninety seconds");
  run.active_milliseconds = 90000;
  require(store.revive(run, PlacementOutcome::failed).issue == MutationIssue::placement_incomplete, "failed revive does not consume availability");
  require(store.revive_available(run), "failed placement retains revive");
  require(store.revive(run, PlacementOutcome::completed).succeeded(), "completed local revive persisted");
  require(!store.revive_available(run), "one revive per run");
  require(store.settle(run).succeeded(), "decline/revive failure never required for results");
  run = {.active_milliseconds = 29999, .score = 999};
  require(base_credits(run) == 0, "minimum thirty active seconds");
  run.active_milliseconds = 30000;
  require(base_credits(run) == 202, "thirty second threshold includes score");
}
void blueprint_milestones_are_distinct_from_run_prerequisites() {
  MemoryBackend backend; AccountStore store(backend); (void)store.load();
  const auto fresh = store.blueprints();
  require(fresh[0].title == "Heavy Winch" && fresh[0].milestone == "0 / 300 captures",
      "blueprints have readable names and explicit account capture units");
  require(fresh[0].requirement.find("Beam Splitter") != std::string::npos,
      "run draw prerequisite remains separate from account milestone");
  require(fresh[2].milestone == "0 / 800 base credits" && fresh[3].milestone == "0 / 60 military defeats",
      "earned base credits and military defeats have distinct milestone units");
  require(fresh[4].milestone == "0 / 1800 active seconds" && fresh[5].milestone == "0 / 3600 active seconds",
      "playtime milestones explicitly count active seconds");
  const auto run = earned_run(store);
  require(store.settle(run).succeeded(), "settle milestone fixture");
  const auto earned = store.blueprints();
  require(earned[0].milestone == "UNLOCKED / 300 captures" && earned[4].milestone == "UNLOCKED / 1800 active seconds",
      "completed milestones expose unlocked state beyond colour");
  require(earned[2].milestone == "778 / 800 base credits", "milestone progress follows banked base rewards");
}
}
int main() {
  rewards_and_achievements_are_exactly_once();
  hangar_costs_caps_and_run_isolation();
  active_checkpoints_revive_and_minimum_reward();
  blueprint_milestones_are_distinct_from_run_prerequisites();
}
