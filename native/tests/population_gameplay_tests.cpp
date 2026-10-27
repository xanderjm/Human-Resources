#include "run_state.h"
#include "test_support.h"
#include <array>
#include <iostream>

namespace gp = human_resources::gameplay;
namespace tune = gp::tuning;
using test_support::require;
using test_support::require_near;
namespace {
constexpr float dt = 1.F / 60;
using Kind = gp::EntityKind;
using Role = gp::InfantryRole;
using Pattern = gp::detail::FormationKind;
gp::detail::Actor body(Kind kind, unsigned id, gp::Vec2 position = {}) {
  gp::detail::Actor a;
  a.view.id = id; a.view.kind = kind; a.view.position = position;
  a.view.hull = a.view.maximum_hull = 1;
  if (kind == Kind::soldier) a.view.infantry_role = Role::rifleman;
  return a;
}
void ranked_captures() {
  struct Target { Kind kind; Role role; float load, score; };
  constexpr std::array<Target, 9> targets{{
      {Kind::sheep, Role::none, 1, 2}, {Kind::pig, Role::none, 1.15F, 3},
      {Kind::farmer, Role::none, 1.35F, 4}, {Kind::cow, Role::none, 1.75F, 6},
      {Kind::soldier, Role::rifleman, 1.55F, 7}, {Kind::soldier, Role::shotgunner, 1.7F, 9},
      {Kind::soldier, Role::rocketeer, 1.9F, 12}, {Kind::jeep, Role::none, 2.25F, 18},
      {Kind::tank, Role::none, 3, 35}}};
  for (const auto& target : targets) {
    const auto profile = gp::abduction_profile(target.kind, target.role);
    require_near(profile.lift_load, target.load, 0, "each target has its intended distinct load");
    require_near(profile.base_score, target.score, 0, "each target has its intended distinct reward");
    gp::Game::State s; s.view.hull = 100; s.view.phase = gp::Phase::active;
    s.view.modules[static_cast<unsigned>(gp::Module::beam_splitter)] = 2;
    s.view.modules[static_cast<unsigned>(gp::Module::heavy_winch)] = 3;
    s.sync(); s.view.velocity = {0, s.view.forward_speed};
    auto a = body(target.kind, s.next_id++); a.view.infantry_role = target.role;
    a.view.hull = .2F; s.actors.push_back(a);
    const bool vehicle = target.kind == Kind::jeep || target.kind == Kind::tank;
    const float seconds = tune::base_capture_seconds * target.load / (vehicle ? 1.4F : 1.F);
    require_near(s.capture_seconds(s.actors.front()), seconds, .00001F, "weight feeds the actual biological or Winch duration");
    unsigned frames = 0;
    while (!s.actors.front().removed && frames++ < 600) s.capture(dt);
    require(s.actors.front().removed, "actual capture completes for every capturable role");
    require_near(static_cast<float>(frames) * dt, seconds, dt * 2, "occupied time agrees with target weight");
    require_near(static_cast<float>(s.score), target.score, .00001F, "actual capture awards the target's unique score");
    const auto score = s.score; s.hit(s.actors.front(), 1000);
    require(s.score == score, "a completed capture cannot also award a destruction reward");
    require(s.view.stats.captures == (vehicle ? 0U : 1U) && s.view.stats.vehicle_captures == (vehicle ? 1U : 0U),
        "weighted captures preserve biological and vehicle progression ownership");
  }
  require(gp::abduction_profile(Kind::boss).lift_load == 0 &&
      gp::abduction_profile(Kind::soldier).lift_load == 0, "boss and invalid infantry profile cannot invent lift rewards");
}
void staged_opening_and_supply() {
  for (unsigned seed = 1; seed <= 64; ++seed) {
    gp::Game run; run.start({.seed = seed});
    const auto& opening = run.snapshot().entities;
    require(opening.size() == tune::initial_biological_count && opening[0].kind == Kind::farmer &&
        opening[1].kind == Kind::sheep && opening[2].kind == Kind::sheep && opening[3].kind == Kind::pig,
        "every seed opens with the authored farmer/sheep/pig pocket inside a populated field");
    for (const auto& a : opening) require(a.kind < Kind::soldier && a.kind != Kind::cow, "opening cannot reveal cattle or military early");
  }
  gp::Game::State s; s.view.stats.active_seconds = 200; s.cattle_introduced = true;
  unsigned since_easy = 0;
  for (unsigned delivery = 0; delivery < 100; ++delivery) {
    s.actors.clear();
    require(s.spawn_cohort(), "legal social pocket delivers atomically");
    const auto group = s.actors.front().group_id;
    bool easy = false;
    for (const auto& a : s.actors) {
      require(a.group_id == group && group != 0, "one social pocket has one durable group identity");
      const float own = tune::biological_clearance_radius[static_cast<unsigned>(a.view.kind)];
      require(std::abs(a.view.position.x) + own <= tune::field_half_width, "whole passive body is inside the field at delivery");
      for (const auto& other : s.actors) if (a.view.id != other.view.id) {
        const float gap = own + tune::biological_clearance_radius[static_cast<unsigned>(other.view.kind)];
        require(gp::detail::distance_squared(a.view.position, other.view.position) >= gap * gap,
            "social delivery reserves measured whole-body footprints rather than torso cores");
      }
      easy = easy || a.view.kind == Kind::sheep || a.view.kind == Kind::pig;
    }
    since_easy = easy ? 0 : since_easy + 1;
    require(since_easy < 3, "every three successfully delivered pockets include accessible small animals");
  }
  s.actors.clear(); s.pockets_since_easy = 2;
  for (unsigned i = 0; i < tune::biological_active_cap - 2; ++i) s.actors.push_back(body(Kind::farmer, i + 1));
  const auto before = s.actors.size();
  const auto group = s.next_group;
  require(!s.spawn_cohort() && s.actors.size() == before && s.next_group == group && s.pockets_since_easy == 2,
      "a partial/full-cap delivery cannot consume the easy-pocket guarantee or leave half a group");
}
void template_clearance_and_admission() {
  constexpr std::array patterns{Pattern::scout, Pattern::patrol, Pattern::closing, Pattern::jeep_escort,
      Pattern::rocket_screen, Pattern::tank_advance, Pattern::pincer};
  for (unsigned seed = 1; seed <= 8; ++seed) for (const auto pattern : patterns) {
    gp::Game::State s; s.random_state = seed; s.view.stats.active_seconds = 200; s.sync();
    require(s.spawn_formation(pattern), "each authored military template has a legal whole-group ingress");
    const auto size = s.actors.size();
    std::array<gp::Vec2, 4> slots{};
    for (std::size_t i = 0; i < size; ++i) {
      const auto& a = s.actors[i]; slots[i] = a.formation.slot;
      const float radius = tune::military_body_radius[static_cast<unsigned>(a.view.kind) - 4];
      require(a.group_id == s.actors.front().group_id && a.formation.active &&
          std::abs(a.view.position.x) - radius >= tune::encounter_open_route_half_width + tune::player_radius,
          "approach assignments preserve whole-body central escape clearance");
      for (std::size_t j = 0; j < i; ++j) {
        const auto& b = s.actors[j];
        const float gap = radius + tune::military_body_radius[static_cast<unsigned>(b.view.kind) - 4] + tune::military_spacing_margin;
        require(gp::detail::distance_squared(a.view.position, b.view.position) >= gap * gap,
            "actual bodies and vehicle assemblies never overlap at group admission");
      }
    }
    bool released = false;
    for (unsigned frame = 0; frame < 480; ++frame) {
      s.view.stats.active_seconds += dt; s.view.runner_distance += s.view.forward_speed * dt;
      s.view.player.z = s.view.runner_distance;
      s.enemies(dt);
      for (std::size_t i = 0; i < size; ++i) {
        const auto& a = s.actors[i];
        require(a.formation.slot.x == slots[i].x && a.formation.slot.z == slots[i].z,
            "approach slots do not mirror or reshuffle with player/world motion");
        if (a.formation.active) require(a.view.warning_seconds == 0 && !a.infantry_aim.preparing,
            "a formation visibly approaches before its individual attacks start");
        released = released || !a.formation.active;
      }
    }
    require(released, "bounded physical travel hands the formation to individual combat");
    s.actors.front().removed = true; s.enemies(dt);
    for (std::size_t i = 1; i < size; ++i) require(s.actors[i].formation.slot.x == slots[i].x,
        "removing the leading member cannot teleport or reassign survivors");
  }
  gp::Game::State full;
  for (unsigned i = 0; i < 16; ++i) full.actors.push_back(body(Kind::soldier, i + 1));
  const auto identity = full.next_id;
  require(!full.spawn_formation(Pattern::rocket_screen) && full.actors.size() == 16 && full.next_id == identity,
      "a support formation cannot leave a partial screen when its population allowance is full");
}
void local_escape_and_commitment() {
  gp::Game::State s; s.view.player = {};
  s.actors = {body(Kind::cow, 1, {2, 0})};
  for (unsigned frame = 0; frame < 20; ++frame) (void)s.civilian_heading(s.actors.front(), dt);
  require(s.actors.front().civilian_escape.remaining_seconds == 0, "cattle visibly hesitate before choosing their slow escape leg");
  for (unsigned frame = 0; frame < 25; ++frame) (void)s.civilian_heading(s.actors.front(), dt);
  require(s.actors.front().civilian_escape.remaining_seconds > 0, "cattle eventually react to local pressure");
  s.actors = {body(Kind::pig, 1, {2, 0}), body(Kind::pig, 2, {3, 0})};
  for (unsigned frame = 0; frame < 5; ++frame) for (auto& a : s.actors) (void)s.civilian_heading(a, dt);
  const auto heading = s.actors.front().civilian_escape.heading;
  require(heading.x != s.actors.back().civilian_escape.heading.x, "pigs choose distinct escape diagonals");
  s.view.player.x = 4;
  for (unsigned frame = 0; frame < 20; ++frame) (void)s.civilian_heading(s.actors.front(), dt);
  require(s.actors.front().civilian_escape.heading.x == heading.x, "pig escape direction is committed instead of twitching with every UFO crossing");
  s.view.player = {}; s.actors = {body(Kind::sheep, 1, {2, 0}), body(Kind::sheep, 2, {3, 0})};
  for (auto& a : s.actors) a.group_id = 1;
  for (unsigned frame = 0; frame < 12; ++frame) for (auto& a : s.actors) (void)s.civilian_heading(a, dt);
  require(s.actors.front().civilian_escape.remaining_seconds > 0 && s.actors.back().civilian_escape.remaining_seconds == 0,
      "flock leader reacts before its neighbour follows");
  for (unsigned frame = 0; frame < 18; ++frame) for (auto& a : s.actors) (void)s.civilian_heading(a, dt);
  require(s.actors.back().civilian_escape.heading.x == s.actors.front().civilian_escape.heading.x,
      "nearby flock members follow their local leader after a real active-time delay");
  s.actors = {body(Kind::farmer, 3, {6.9F, 0})}; s.view.player = {6.4F, 0};
  const auto open = s.civilian_heading(s.actors.front(), dt);
  require(open.x <= 0, "people choose an open route instead of fleeing straight into a field edge");
}
void future_during_groups_and_escape() {
  gp::Game original; original.start({.seed = 514}); auto& s = *original.state_;
  s.actors.clear(); s.view.beams.clear(); s.view.stats.active_seconds = 200; s.next_boss = 360;
  s.view.invulnerability_seconds = 1000; s.revealed_threats = 31; s.cattle_introduced = true;
  require(s.spawn_formation(Pattern::rocket_screen) && s.spawn_cohort(), "continuation has an actual formation and social pocket");
  s.actors.back().view.position = {2, s.view.runner_distance}; s.sync();
  for (unsigned frame = 0; frame < 35; ++frame) original.advance(dt);
  original.pause(); const auto paused = original.save_state();
  require(!paused.empty(), "approach and committed civilian escape are valid durable state");
  gp::Game cold; require(cold.restore_state(paused) == gp::SaveStateIssue::none && cold.save_state() == paused,
      "cold paused restore keeps exact group identities, approach slots and escape choices");
  original.advance(10); require(original.save_state() == paused, "pause freezes all group and species reaction clocks");
  original.resume(); cold.resume();
  for (unsigned frame = 0; frame < 900; ++frame) {
    if (original.snapshot().phase == gp::Phase::choosing_upgrade) require(original.choose_upgrade(0) && cold.choose_upgrade(0), "matching earned choices resume both futures");
    const gp::Control control{{std::sin(static_cast<float>(frame) * .02F), 0}, frame % 120 == 0};
    original.advance(dt, control); cold.advance(dt, control);
    require(!original.save_state().empty() && original.save_state() == cold.save_state(),
        "full capture/formation/escape/combat futures remain byte-exact after a process continuation");
  }
}
void natural_reveal_gates() {
  for (unsigned seed : {1U, 514U, 7841U}) {
    gp::Game run; run.start({.seed = seed}); run.state_->view.invulnerability_seconds = 1000;
    std::array<double, 7> first{}; first.fill(-1);
    constexpr std::array<double, 7> gate{8, 12, 24, 36, 48, 65, 100};
    for (unsigned frame = 0; frame < 60 * 235; ++frame) {
      if (run.snapshot().phase == gp::Phase::choosing_upgrade) require(run.choose_upgrade(0), "natural reveal trace resolves earned choices");
      run.advance(dt);
      for (const auto& a : run.snapshot().entities) {
        unsigned index = 7;
        if (a.kind == Kind::cow) index = 0;
        if (a.kind == Kind::soldier) index = a.infantry_role == Role::rifleman ? 1 : a.infantry_role == Role::shotgunner ? 2 : 4;
        if (a.kind == Kind::jeep) index = 3;
        if (a.kind == Kind::tank) index = 5;
        if (a.kind == Kind::boss) index = 6;
        if (index < 7 && first[index] < 0) first[index] = run.snapshot().stats.active_seconds;
      }
    }
    for (unsigned i = 0; i < gate.size(); ++i) require(first[i] >= gate[i] && first[i] < gate[i] + 40,
        "all staged targets arrive without early role leaks or indefinite missed reveals");
    std::cout << "REVEALS seed=" << seed;
    for (const auto time : first) std::cout << ' ' << time;
    std::cout << '\n';
  }
}
void escalating_formation_allowance() {
  for (const auto [seconds, expected] : std::array<std::pair<float, unsigned>, 3>{{{13, 1}, {19, 3}, {75, 6}}}) {
    gp::Game::State s;
    s.view.stats.active_seconds = seconds;
    s.view.forward_speed = tune::forward_scroll_metres_per_second;
    s.next_boss = 1000;
    s.revealed_threats = 31;
    // Retain every admitted group to measure simultaneous population, rather
    // than letting kills/retirement hide an accidentally restrictive cap.
    for (unsigned attempt = 0; attempt < 20; ++attempt) {
      s.soldier_budget = 4;
      s.director(0);
    }
    require(s.next_group - 1 == expected, "active-time escalation admits one, three, then six complete formations");
    require(s.actors.size() <= 18, "six formations retain the existing infantry population budget");
  }
}
} // namespace
int main() {
  ranked_captures(); staged_opening_and_supply(); template_clearance_and_admission();
  local_escape_and_commitment(); future_during_groups_and_escape(); natural_reveal_gates(); escalating_formation_allowance();
  std::cout << "Population weight, score, reveal, movement, formation and continuation contracts passed\n";
}
