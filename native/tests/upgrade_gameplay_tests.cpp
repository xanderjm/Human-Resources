#include "run_state.h"
#include "test_support.h"
#include <bit>
#include <iostream>
namespace gp = human_resources::gameplay;
namespace tune = gp::tuning;
using test_support::require;
using test_support::require_near;
namespace {
constexpr std::array retired{gp::Module::reserve_battery, gp::Module::bounty_scanner,
    gp::Module::mirror_plating, gp::Module::overtime_reactor};
void retired_hardware_cannot_enter_a_run() {
  require(std::popcount(tune::available_blueprints) == 12 && std::popcount(tune::starter_blueprints) == 9,
      "twelve live families include nine fresh-account families");
  gp::Game run; run.start({.unlocked_blueprints = 65535U});
  auto& s = *run.state_;
  require(s.setup.unlocked_blueprints == tune::available_blueprints,
      "legacy account bits cannot reactivate retired hardware in a new flight");
  for (const auto m : retired) {
    const auto i = static_cast<unsigned>(m);
    require(!s.module_eligible(i) && gp::module_id(m).empty() && tune::module_names[i].empty(),
        "retired durable slots have no live card or display identity");
    s.view.modules[i] = 3;
    require(s.rank(m) == 0 && run.save_state().empty(), "retired installation is inert and cannot become a legal continuation");
    s.view.modules[i] = 0;
  }
  for (unsigned draw = 0; draw < 1000; ++draw) {
    s.draw_choices();
    for (const auto& c : s.view.choices)
      require(!c.hardware || (gp::blueprint_bit(c.module) & tune::available_blueprints),
          "repeated actual draws cannot offer removed hardware at any rarity");
  }
  s.view.modules[static_cast<unsigned>(gp::Module::shield_generator)] = 3;
  s.view.modules[static_cast<unsigned>(gp::Module::mirror_plating)] = 3;
  s.evolve();
  require(!(s.view.evolution_bits & (1U << static_cast<unsigned>(gp::Evolution::mirrorball))),
      "retired Mirrorball cannot evolve even from an explicitly invalid fixture");
  require(tune::active_evolutions.size() == 5 && std::popcount(tune::available_evolutions) == 5,
      "five recipes remain and the retired evolution bit is reserved");
}
void beam_splitter_is_randomly_available_from_a_fresh_account() {
  bool offered = false, absent = false;
  for (unsigned seed = 1; seed <= 64; ++seed) {
    gp::Game run; run.start({.seed = seed});
    const auto index = static_cast<unsigned>(gp::Module::beam_splitter);
    require(run.state_->module_eligible(index) && run.snapshot().modules[index] == 0 && run.snapshot().beam_capacity == 1,
        "Beam Splitter is eligible immediately with one ordinary starting beam and no achievement");
    for (unsigned frame = 0; frame < 900 && run.snapshot().phase == gp::Phase::active; ++frame)
      run.advance(1.F / 60);
    require(run.snapshot().phase == gp::Phase::choosing_upgrade, "fresh natural play reaches its first real draft");
    bool found = false;
    for (const auto& c : run.snapshot().choices) found = found || (c.hardware && c.module == gp::Module::beam_splitter);
    offered = offered || found; absent = absent || !found;
    if (found) {
      const auto saved = run.save_state(); gp::Game restored;
      require(restored.restore_state(saved) == gp::SaveStateIssue::none && restored.save_state() == saved,
          "an actual fresh Beam Splitter offer preserves exact cold choices");
    }
  }
  require(offered && absent, "Beam Splitter appears in real first drafts but is neither guaranteed nor preinstalled");
}
void removed_stat_effects_do_not_survive_as_hidden_bonuses() {
  gp::Game run; run.start(); auto& s = *run.state_;
  for (const auto m : retired) s.view.modules[static_cast<unsigned>(m)] = 3;
  require_near(s.damage_multiplier(), 1, 0, "retired Reserve Battery contributes no idle-beam damage");
  s.view.beams.push_back({});
  require_near(s.fire_multiplier(), 1, 0, "retired Overtime Reactor contributes no occupied-beam fire rate");
  gp::detail::Actor a; a.view.kind = gp::EntityKind::sheep; a.view.id = 1;
  s.streak = 100; s.captured(a);
  require_near(static_cast<float>(s.score), gp::abduction_profile(gp::EntityKind::sheep).base_score, 0,
      "retired Bounty Scanner and its streak cannot inflate capture rewards");
}
}
int main() {
  retired_hardware_cannot_enter_a_run();
  beam_splitter_is_randomly_available_from_a_fresh_account();
  removed_stat_effects_do_not_survive_as_hidden_bonuses();
  std::cout << "Twelve-hardware availability, fresh random Beam Splitter and retirement contracts passed\n";
}
