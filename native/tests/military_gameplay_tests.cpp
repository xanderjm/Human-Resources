#include "run_state.h"
#include "test_support.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>

namespace game = human_resources::gameplay;
namespace tune = game::tuning;
using test_support::require;
using test_support::require_near;

namespace {
constexpr float dt = 1.F / 60;
constexpr std::array roles{game::InfantryRole::rifleman, game::InfantryRole::shotgunner,
    game::InfantryRole::rocketeer};
unsigned groups{};
void pass(const char* label) { ++groups; std::cout << "PASS " << label << '\n'; }
float distance(game::Vec2 a, game::Vec2 b) { return std::hypot(a.x - b.x, a.z - b.z); }
void near(game::Vec2 a, game::Vec2 b, float tolerance, const char* label) {
  require(distance(a, b) <= tolerance, label);
}
game::detail::Actor soldier(game::InfantryRole role, unsigned id = 1) {
  game::detail::Actor actor;
  actor.view.id = id;
  actor.view.kind = game::EntityKind::soldier;
  actor.view.infantry_role = role;
  actor.view.position = {2, 8};
  actor.view.hull = actor.view.maximum_hull = 400;
  actor.aim = actor.view.warning_aim = {-2, -3};
  return actor;
}
void encounter_stream_and_bounds() {
  std::array<unsigned, 4> seen{};
  for (unsigned seed = 1; seed <= 128; ++seed) {
    game::Game::State actual, reference;
    actual.random_state = reference.random_state = seed;
    actual.spawn(game::EntityKind::soldier);
    for (unsigned sample = 0; sample < 4; ++sample) (void)reference.random_unit();
    require(actual.random_state == reference.random_state,
        "infantry role selection consumes exactly the four established encounter samples");
    require(actual.view.director_spawn_attempts == 1 && actual.actors.size() == 1,
        "one infantry spawn remains one encounter and one actor");
    const auto role = actual.actors.front().view.infantry_role;
    require(role >= game::InfantryRole::rifleman && role <= game::InfantryRole::rocketeer,
        "each soldier has a valid equipped role");
    ++seen[static_cast<std::size_t>(role)];
    const auto expected_hash = (std::uint64_t{0} ^ reference.random_state ^
        static_cast<unsigned>(game::EntityKind::soldier)) * 1099511628211ULL;
    require(actual.view.director_schedule_hash == expected_hash,
        "adding roles preserves the existing encounter schedule hash");
    game::Game::State full;
    full.random_state = seed;
    for (unsigned index = 0; index < 18; ++index) full.actors.push_back(soldier(roles[index % 3], index + 1));
    full.spawn(game::EntityKind::soldier);
    require(full.actors.size() == 18 && full.random_state == actual.random_state &&
        full.view.director_schedule_hash == actual.view.director_schedule_hash,
        "full soldier population still consumes the same samples without growing past eighteen");
  }
  for (const auto role : roles) require(seen[static_cast<std::size_t>(role)] > 0,
      "all three roles occur naturally across fixed encounter seeds");
  pass("roles reuse encounter RNG and retain schedule hash and population cap");
}
void planted_telegraph_and_recovery() {
  for (const auto role : roles) {
    game::Game::State state;
    state.view.player = {};
    state.actors.push_back(soldier(role));
    state.enemies(dt);
    auto& actor = state.actors.front();
    require(actor.view.warning_seconds > 0, "eligible soldier begins a visible warning");
    const auto locked_position = actor.view.position;
    const auto committed_aim = actor.view.warning_aim;
    const float expected_warning = role == game::InfantryRole::rocketeer ? 1.2F : .9F;
    require_near(actor.view.warning_seconds, expected_warning, dt + .0001F,
        "each role has its explicit warning duration");
    unsigned warning_steps{};
    while (state.bullets.empty() && warning_steps++ < 100) {
      // The target moves after the lock. The enemy must finish the committed shot.
      state.view.player = {-5, -1};
      state.enemies(dt);
      near(actor.view.position, locked_position, .00001F,
          "warning and firing never slide the infantry root");
      near(actor.view.velocity, {}, .00001F, "planted warning publishes zero movement");
      near(actor.view.warning_aim, committed_aim, .00001F, "warning aim remains committed");
    }
    require(!state.bullets.empty(), "warning eventually emits an actual authoritative volley");
    require_near(actor.view.fire_recovery_seconds, .6F, dt + .0001F,
        "fire enters the explicit six-tenths-second recovery");
    unsigned recovery_steps{};
    while (actor.view.fire_recovery_seconds > dt + .0001F && recovery_steps++ < 60) {
      state.enemies(dt);
      near(actor.view.position, locked_position, .00001F, "fire recovery keeps infantry planted");
      near(actor.view.velocity, {}, .00001F, "fire recovery publishes zero movement");
    }
    for (unsigned frame = 0; frame < 3; ++frame) state.enemies(dt);
    require(distance(actor.view.position, locked_position) > .0001F,
        "ordinary soldier pursuit resumes after recovery");
  }
  pass("committed warnings and fire recovery plant each role before pursuit resumes");
}
void attack_cadence() {
  for (const auto role : roles) {
    game::Game::State state;
    state.view.player = {};
    state.actors.push_back(soldier(role));
    std::array<float, 3> fire_times{};
    unsigned volleys{};
    std::size_t bullet_count{};
    for (unsigned frame = 0; frame < 60 * 12 && volleys < fire_times.size(); ++frame) {
      state.enemies(dt);
      if (state.bullets.size() > bullet_count) {
        fire_times[volleys++] = static_cast<float>(frame) * dt;
        bullet_count = state.bullets.size();
      }
    }
    require(volleys == fire_times.size(), "cadence fixture observes three complete volleys");
    for (unsigned i = 1; i < fire_times.size(); ++i)
      require_near(fire_times[i] - fire_times[i - 1], 3.9F, dt * 3,
          "warning plus wait remains a 3.9-second attack cycle including recovery");
  }
  pass("all infantry roles preserve the reviewed full attack budget");
}
void cancellation_and_warning_cap() {
  for (const auto role : roles) {
    for (const bool lifted : {false, true}) {
      game::Game::State state;
      auto actor = soldier(role);
      actor.view.warning_seconds = .2F;
      actor.view.fire_recovery_seconds = .4F;
      actor.lifted = lifted;
      if (!lifted) actor.view.disabled_seconds = 1;
      state.actors.push_back(actor);
      const auto before = state.actors.front().view.position;
      for (unsigned frame = 0; frame < 15; ++frame) state.enemies(dt);
      require(state.bullets.empty(), "capture or disable preempts a committed infantry shot");
      require(state.actors.front().view.warning_seconds == 0,
          "interrupted infantry no longer advertises a live warning");
      near(state.actors.front().view.position, before, .00001F,
          "captured or disabled infantry cannot continue pursuit");
    }
  }
  for (const auto [second, cap] : std::array<std::pair<double, unsigned>, 3>{{
      {0, tune::encounter_build_warning_cap}, {tune::encounter_build_seconds + 1, tune::encounter_assault_warning_cap},
      {tune::encounter_build_seconds + tune::encounter_assault_seconds + 1, tune::encounter_recovery_warning_cap}}}) {
    game::Game::State full;
    full.view.stats.active_seconds = second;
    for (unsigned i = 0; i < 18; ++i) full.actors.push_back(soldier(roles[i % 3], i + 1));
    full.enemies(dt);
    const auto warnings = static_cast<unsigned>(std::count_if(full.actors.begin(), full.actors.end(),
        [](const auto& actor) { return actor.view.warning_seconds > 0; }));
    require(warnings == cap, "all infantry roles share the actual encounter-phase warning budget");
    require(warnings <= tune::warning_active_cap, "encounter warning budget retains the global eight-warning ceiling");
  }
  pass("capture and disable preempt attacks and warning population stays bounded");
}
void volleys_and_capacity() {
  for (const auto role : roles) {
    game::Game::State state;
    state.next_id = 2;
    auto actor = soldier(role);
    state.shoot(actor);
    state.sync();
    const auto muzzle = game::infantry_shot(role, actor.view.position, actor.aim);
    require(muzzle.valid, "ordinary committed geometry has a physical muzzle solution");
    const auto& bullets = state.view.projectiles;
    const std::size_t expected = role == game::InfantryRole::shotgunner ? 3 : 1;
    require(bullets.size() == expected, "role emits its explicit number of projectiles");
    float total_damage{};
    for (const auto& projectile : bullets) {
      require(projectile.source_id == actor.view.id && projectile.infantry_role == role,
          "public projectile retains source identity and equipped role");
      const float speed = role == game::InfantryRole::rocketeer ? 4.5F : 6.F;
      require_near(std::hypot(projectile.velocity.x, projectile.velocity.z), speed, .0001F,
          "role projectile uses the reviewed speed");
      total_damage += projectile.damage;
      near(projectile.position, projectile.launch_position, .00001F,
          "new projectile starts at its published source-derived muzzle");
      near(projectile.launch_position, muzzle.origin, .00001F,
          "actual emitted projectile uses the public source-derived muzzle solution");
      require_near(projectile.launch_height, muzzle.height, .00001F,
          "actual projectile height agrees with the authored source muzzle");
      require(projectile.launch_height > 0 && std::isfinite(projectile.rise_per_second) &&
          projectile.age_seconds == 0, "published vertical trajectory starts finite at the muzzle");
      const float flight = distance(projectile.launch_position, actor.aim) / speed;
      require_near(projectile.launch_height + projectile.rise_per_second * flight,
          tune::projectile_target_height, .0001F,
          "projectile reaches the ship plane at its committed ground target");
    }
    require_near(total_damage, 6, .00001F, "entire shotgun fan shares the six-point attack budget");
    if (expected == 3) {
      auto angle = [](const auto& p) { return std::atan2(p.velocity.z, p.velocity.x); };
      auto delta = [](float a, float b) { return std::remainder(a - b, 6.283185307F); };
      require_near(std::fabs(delta(angle(bullets[0]), angle(bullets[1]))), .12F, .0001F,
          "left shotgun pellet has the reviewed angular separation");
      require_near(std::fabs(delta(angle(bullets[2]), angle(bullets[1]))), .12F, .0001F,
          "right shotgun pellet has the reviewed angular separation");
      near(bullets[0].launch_position, bullets[1].launch_position, .00001F,
          "pellets originate from one weapon muzzle");
      near(bullets[2].launch_position, bullets[1].launch_position, .00001F,
          "entire fan retains one committed muzzle");
    }
    state.bullets.resize(95);
    state.shoot(actor);
    require(state.bullets.size() <= 96, "partial fan capacity cannot exceed ninety-six projectiles");
    const auto full_count = state.bullets.size();
    state.bullets.resize(96);
    state.shoot(actor);
    require(state.bullets.size() == 96 && full_count >= 95,
        "full projectile budget rejects further emissions");
  }
  pass("role volleys preserve damage, speed, muzzle metadata, spread and projectile cap");
}
void muzzle_geometry_and_invalid_aims() {
  for (const auto role : roles) {
    const auto attack = game::infantry_attack(role);
    for (unsigned step = 0; step < 24; ++step) {
      const float theta = static_cast<float>(step) * 6.283185307F / 24;
      const game::Vec2 actor{1.25F, 17};
      const game::Vec2 aim{actor.x + 8 * std::sin(theta), actor.z + 8 * std::cos(theta)};
      const auto shot = game::infantry_shot(role, actor, aim);
      require(shot.valid && std::isfinite(shot.origin.x) && std::isfinite(shot.origin.z),
          "every ordinary aim quadrant has a finite muzzle solution");
      require_near(std::hypot(shot.direction.x, shot.direction.z), 1, .00001F,
          "muzzle direction is unit length");
      const game::Vec2 to{aim.x - shot.origin.x, aim.z - shot.origin.z};
      require_near(to.x * shot.direction.z - to.z * shot.direction.x, 0, .00001F,
          "muzzle ray intersects the committed point despite cross-body weapon yaw");
      require(to.x * shot.direction.x + to.z * shot.direction.z > 0,
          "muzzle ray travels toward the committed point");
      require_near(distance(actor, shot.origin), std::hypot(attack.neutral_muzzle[0], attack.neutral_muzzle[2]),
          .00001F, "aiming preserves the physical source muzzle offset from the root");
      require_near(shot.height, attack.neutral_muzzle[1], .00001F,
          "planar aiming preserves the actual source muzzle height");
      const game::Vec2 translated_actor{actor.x + 3.5F, actor.z - 7};
      const game::Vec2 translated_aim{aim.x + 3.5F, aim.z - 7};
      const auto translated = game::infantry_shot(role, translated_actor, translated_aim);
      require(translated.valid, "translated ordinary shot remains valid");
      near(translated.direction, shot.direction, .00001F,
          "aiming direction is independent of the world origin");
      near(translated.origin, {shot.origin.x + 3.5F, shot.origin.z - 7}, .00001F,
          "muzzle solution translates with its actor");
    }
    require(!game::infantry_shot(role, {}, {}).valid &&
        !game::infantry_shot(role, {}, {.001F, .001F}).valid,
        "zero and near-body targets cannot create backwards or nonfinite muzzle shots");
    const float invalid = std::numeric_limits<float>::quiet_NaN();
    require(!game::infantry_shot(role, {invalid, 0}, {0, 8}).valid &&
        !game::infantry_shot(role, {}, {0, invalid}).valid,
        "invalid source and committed aim reject before emitting a shot");
    game::Game::State state;
    auto actor = soldier(role);
    actor.aim = actor.view.position;
    state.shoot(actor);
    require(state.bullets.empty() && actor.view.fire_recovery_seconds == 0,
        "invalid committed geometry does not emit a projectile or fake firing recovery");
  }
  for (const auto invalid_role : {game::InfantryRole::none, static_cast<game::InfantryRole>(255)}) {
    require(game::infantry_attack(invalid_role).pellets == 0 &&
        !game::infantry_shot(invalid_role, {}, {0, 8}).valid,
        "none and out-of-range roles have no attack or valid muzzle solution");
  }
  pass("source muzzle rays meet committed aims in every quadrant and reject invalid geometry");
}
void choose(game::Game& run, unsigned frame) {
  if (run.snapshot().phase == game::Phase::choosing_upgrade)
    require(run.choose_upgrade(frame % 3), "continued military fixture accepts its earned draft");
}
void continuation_and_revision() {
  for (const auto role : roles) {
    game::Game run;
    run.start({.seed = 317});
    auto& state = *run.state_;
    state.actors = {soldier(role)};
    state.next_id = 2;
    state.actors.front().view.warning_seconds = .01F;
    state.enemies(dt);
    state.projectiles(.1F);
    state.sync();
    require(state.actors.front().view.fire_recovery_seconds > 0 && !run.snapshot().projectiles.empty(),
        "continuation fixture contains actual recovery and a live emitted projectile");
    const auto bytes = run.save_state();
    require(!bytes.empty(), "military authority produces a valid bounded continuation");
    game::Game restored;
    require(restored.restore_state(bytes) == game::SaveStateIssue::none && restored.save_state() == bytes,
        "role, recovery, committed aim and projectile authority round-trip exactly");
    require(restored.snapshot().entities.front().infantry_role == role &&
        restored.snapshot().entities.front().fire_recovery_seconds > 0,
        "restored public actor exposes the same role and remaining recovery");
    const auto& actual = run.snapshot().projectiles.front();
    const auto& copy = restored.snapshot().projectiles.front();
    require(actual.source_id == copy.source_id && actual.infantry_role == copy.infantry_role &&
        actual.age_seconds == copy.age_seconds && actual.launch_height == copy.launch_height &&
        actual.rise_per_second == copy.rise_per_second,
        "restored public projectile retains exact origin identity and vertical trajectory");
    near(actual.launch_position, copy.launch_position, 0,
        "restored public projectile retains exact launch position");
    for (unsigned frame = 0; frame < 600; ++frame) {
      choose(run, frame); choose(restored, frame);
      const game::Control control{{std::sin(static_cast<float>(frame) * .03F), .2F}, frame % 137 == 0};
      run.advance(dt, control); restored.advance(dt, control);
      require(run.save_state() == restored.save_state(),
          "military continuation preserves exact future authority under identical inputs");
    }
    const auto protected_state = restored.save_state();
    auto old_revision = bytes;
    old_revision[8] = std::byte{1};
    old_revision[9] = old_revision[10] = old_revision[11] = std::byte{};
    require(game::inspect_saved_state(old_revision).issue == game::SaveStateIssue::incompatible_rules &&
        restored.restore_state(old_revision) == game::SaveStateIssue::incompatible_rules,
        "old authority revision is identified as incompatible before reading changed fields");
    require(restored.save_state() == protected_state,
        "rejecting old military authority is transactional and preserves live state");
  }
  pass("military continuation preserves public metadata and exact futures; old revision is protected");
}
void malformed_military_authority() {
  game::Game run;
  run.start({.seed = 917});
  auto& state = *run.state_;
  state.actors = {soldier(game::InfantryRole::rifleman)};
  state.next_id = 2;
  state.shoot(state.actors.front());
  state.sync();
  const auto good = run.save_state();
  require(!good.empty(), "wire corruption fixture starts with valid actual military state");
  // Revision-four fieldwise offsets are deliberately explicit corruption fixtures.
  constexpr std::size_t actor = 378, role = actor + 49, recovery = actor + 50;
  constexpr std::size_t projectile = actor + 101 + 4, projectile_role = projectile + 37;
  auto seal = [](auto& bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t i = 0; i < bytes.size() - 8; ++i)
      hash = (hash ^ std::to_integer<std::uint8_t>(bytes[i])) * 1099511628211ULL;
    for (unsigned i = 0; i < 8; ++i)
      bytes[bytes.size() - 8 + i] = static_cast<std::byte>((hash >> (8 * i)) & 255U);
  };
  auto reject = [&](auto bad) {
    seal(bad);
    require(game::inspect_saved_state(bad).issue == game::SaveStateIssue::malformed &&
        run.restore_state(bad) == game::SaveStateIssue::malformed,
        "invalid new military authority fields are rejected by inspection and restore");
    require(run.save_state() == good, "malformed military restore leaves live authority intact");
  };
  auto bad = good; bad[role] = std::byte{255}; reject(bad);
  bad = good; bad[role] = std::byte{0}; reject(bad);
  bad = good; bad[actor + 4] = static_cast<std::byte>(game::EntityKind::cow); reject(bad);
  const auto negative = std::bit_cast<std::uint32_t>(-.1F);
  bad = good;
  for (unsigned i = 0; i < 4; ++i) bad[recovery + i] = static_cast<std::byte>((negative >> (8 * i)) & 255U);
  reject(bad);
  const auto excessive = std::bit_cast<std::uint32_t>(.8F);
  bad = good;
  for (unsigned i = 0; i < 4; ++i) bad[recovery + i] = static_cast<std::byte>((excessive >> (8 * i)) & 255U);
  reject(bad);
  bad = good; bad[projectile_role] = std::byte{255}; reject(bad);
  bad = good; bad[projectile_role] = static_cast<std::byte>(game::InfantryRole::shotgunner); reject(bad);
  pass("malformed role, recovery and source-role metadata reject transactionally");
}
} // namespace

int main() {
  encounter_stream_and_bounds();
  planted_telegraph_and_recovery();
  attack_cadence();
  cancellation_and_warning_cap();
  volleys_and_capacity();
  muzzle_geometry_and_invalid_aims();
  continuation_and_revision();
  malformed_military_authority();
  std::cout << "RESULT " << groups << " military gameplay groups passed, zero failures\n";
}
