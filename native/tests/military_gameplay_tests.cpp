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
  const auto shot = game::infantry_shot(role, actor.view.position, actor.aim);
  const float yaw = std::atan2(shot.direction.x, -shot.direction.z) - game::infantry_attack(role).weapon_yaw_radians;
  actor.locomotion_velocity = {std::sin(yaw), -std::cos(yaw)};
  actor.view.body_heading = yaw;
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
    for (unsigned frame = 0; frame < 120 && state.actors.front().view.warning_seconds == 0; ++frame)
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
void physical_preparation_aligns_and_cancels() {
  for (const auto role : roles) {
    game::Game::State state;
    auto source = soldier(role);
    source.locomotion_velocity = {0, 1};
    source.view.body_heading = 3.14159265359F;
    state.actors = {source}; state.next_id = 2;
    std::optional<game::Game::State> preparation;
    for (unsigned frame = 0; frame < 180 && state.actors.front().view.warning_seconds == 0; ++frame) {
      const auto before = state.actors.front();
      if (frame == 3) state.view.player = {-3, -1}; // Provisional aim may follow a real target change.
      state.enemies(dt); state.sync();
      const auto& actor = state.actors.front();
      const float old_yaw = std::atan2(before.locomotion_velocity.x, -before.locomotion_velocity.z);
      const float yaw = std::atan2(actor.locomotion_velocity.x, -actor.locomotion_velocity.z);
      require(std::abs(std::remainder(yaw - old_yaw, 6.28318530718F)) <= tune::infantry_turn_radians_per_second * dt + .00001F,
          "preparation physically turns each equipped body within the per-step limit");
      if (actor.infantry_aim.preparing) {
        require(actor.view.warning_seconds == 0 && actor.view.fire_recovery_seconds == 0 && state.bullets.empty(),
            "preparation cannot advertise a committed warning or release a shot early");
        near(actor.view.velocity, {}, .00001F, "preparation raises and turns from a planted root");
        if (before.infantry_aim.preparing) {
          near(actor.view.position, before.view.position, .00001F, "ongoing preparation never slides the root");
          require_near(actor.infantry_aim.pose_seconds - before.infantry_aim.pose_seconds, dt, .00001F,
              "preparation age consumes real active time rather than cooldown-derived history");
        }
        const auto& view = state.view.entities.front();
        require(view.preparing && view.aim_pose_seconds == actor.infantry_aim.pose_seconds && view.body_heading.has_value(),
            "public preparation publishes the actual physical frame and age");
        near(view.warning_aim, actor.aim, 0, "preparation projection carries the provisional physical aim");
        if (!preparation) preparation = state;
      }
    }
    auto& actor = state.actors.front();
    require(preparation.has_value() && actor.view.warning_seconds > 0 && !actor.infantry_aim.preparing,
        "a rear-facing eligible soldier exposes finite physical preparation before warning lock");
    const auto root = actor.view.position, facing = actor.locomotion_velocity, aim = actor.aim;
    const auto muzzle = game::infantry_shot(role, root, aim);
    const float physical_yaw = std::atan2(facing.x, -facing.z);
    const float aimed_yaw = std::atan2(muzzle.direction.x, -muzzle.direction.z) - game::infantry_attack(role).weapon_yaw_radians;
    require(muzzle.valid && std::abs(std::remainder(physical_yaw - aimed_yaw, 6.28318530718F)) < .001F,
        "warning begins only when actual physical body and authored muzzle solve the same aim");
    const auto locked = state;
    for (unsigned frame = 0; frame < 100 && state.bullets.empty(); ++frame) {
      state.view.player = {5, 1};
      const float age = actor.infantry_aim.pose_seconds;
      state.enemies(dt); state.sync();
      near(actor.view.position, root, 0, "committed prepared warning retains its physical source");
      near(actor.locomotion_velocity, facing, .00001F, "committed prepared warning retains body facing");
      near(actor.aim, aim, 0, "prepared lock cannot track a later target movement");
      if (state.bullets.empty()) require_near(actor.infantry_aim.pose_seconds - age, dt, .00001F,
          "warning continues the real preparation pose clock");
    }
    require(state.bullets.size() == game::infantry_attack(role).pellets && actor.view.fire_recovery_seconds > 0 &&
        actor.infantry_aim.pose_seconds == 0 && !state.view.entities.front().aim_pose_seconds.has_value(),
        "actual release emits the role volley and clears preparation before fire recovery");
    for (const auto& projectile : state.bullets) near(projectile.view.launch_position, muzzle.origin, .00001F,
        "prepared actual volley launches from the frozen source-derived muzzle");
    for (const bool during_warning : {false, true}) for (const bool lifted : {false, true}) {
      auto cancelled = during_warning ? locked : *preparation;
      auto& interrupted = cancelled.actors.front(); const auto retained = interrupted.locomotion_velocity;
      if (lifted) interrupted.lifted = true; else interrupted.view.disabled_seconds = 1;
      cancelled.enemies(dt); cancelled.sync();
      require(cancelled.bullets.empty() && !interrupted.infantry_aim.preparing && interrupted.infantry_aim.pose_seconds == 0 &&
          interrupted.view.warning_seconds == 0 && !cancelled.view.entities.front().aim_pose_seconds.has_value(),
          "capture or disable cancels actual preparation and warning without a phantom shot or stale pose");
      near(interrupted.locomotion_velocity, retained, 0, "cancellation preserves the real stopped physical facing");
    }
  }
  pass("all roles physically prepare, lock one source, fire and cancel with truthful active pose clocks");
}
unsigned occupied_threats(const game::Game::State& state) {
  return static_cast<unsigned>(std::count_if(state.actors.begin(), state.actors.end(), [](const auto& a) {
    return !a.removed && (a.view.warning_seconds > 0 || a.infantry_aim.preparing);
  }));
}
void held_warning_slots_keep_provisional_aim_fresh_and_durable() {
  game::Game run; run.start({.seed=514}); auto& state = *run.state_;
  auto source = soldier(game::InfantryRole::rifleman);
  source.locomotion_velocity = {0, 1}; source.view.body_heading = 3.14159265359F;
  state.actors = {source}; state.view.beams.clear(); state.next_id = 200;
  state.enemies(dt); state.sync();
  require(state.actors.front().infantry_aim.preparing, "reservation fixture first earns actual preparation");
  // The preparing owner already occupies one place: only cap-1 warnings fit.
  for (unsigned i = 0; i + 1 < tune::encounter_build_warning_cap; ++i) {
    auto blocker = soldier(game::InfantryRole::rifleman, 100 + i);
    blocker.view.position = {-5.F, 12.F + 3.F * static_cast<float>(i)};
    blocker.aim = blocker.view.warning_aim = {};
    const auto shot = game::infantry_shot(blocker.view.infantry_role, blocker.view.position, blocker.aim);
    const float yaw = std::atan2(shot.direction.x, -shot.direction.z) - game::infantry_attack(blocker.view.infantry_role).weapon_yaw_radians;
    blocker.locomotion_velocity = {std::sin(yaw), -std::cos(yaw)};
    blocker.view.body_heading = yaw; blocker.view.warning_seconds = .9F; blocker.age = 1;
    state.actors.push_back(blocker);
  }
  const auto vehicle_index = state.actors.size();
  game::detail::Actor vehicle;
  vehicle.view.id = 101; vehicle.view.kind = game::EntityKind::jeep;
  vehicle.view.position = {4.5F, 10}; vehicle.view.hull = vehicle.view.maximum_hull = 90;
  vehicle.locomotion_velocity = {0, -1}; vehicle.view.body_heading = 0;
  state.actors.push_back(vehicle);
  const auto waiting_index = state.actors.size();
  auto waiting = soldier(game::InfantryRole::rifleman, 102);
  waiting.view.position = {-4, 7}; waiting.locomotion_velocity = {0, -1};
  state.actors.push_back(waiting);
  require(occupied_threats(state) == tune::encounter_build_warning_cap, "fixture has one real owner and a legally full shared budget");
  std::optional<game::Game::State> preparing;
  std::vector<std::byte> saved;
  bool waiting_moved = false;
  for (unsigned frame = 0; frame < 21 && state.actors.front().view.warning_seconds == 0; ++frame) {
    const auto before = state.actors.front(); const auto waiting_before = state.actors[waiting_index].view.position;
    state.view.player.x = frame < 2 ? 2.F : 1.F; state.view.velocity.x = 0;
    state.enemies(dt); state.sync(); const auto& owner = state.actors.front();
    require_near(owner.aim.x, state.view.player.x, .00001F,
        "reserved preparation refreshes the actual provisional target until warning lock");
    require(state.actors[vehicle_index].view.warning_seconds == 0 && !state.actors[waiting_index].infantry_aim.preparing &&
        state.actors[waiting_index].view.warning_seconds == 0,
        "ready vehicles and infantry cannot steal an owned preparation slot");
    waiting_moved = waiting_moved || distance(waiting_before, state.actors[waiting_index].view.position) > .00001F;
    require(occupied_threats(state) == tune::encounter_build_warning_cap, "preparation to warning converts one slot without overbooking");
    if (owner.infantry_aim.preparing) {
      require_near(owner.infantry_aim.pose_seconds - before.infantry_aim.pose_seconds, dt, .00001F,
          "reserved preparation keeps its real active clock");
      if (!preparing) { preparing = state; saved = run.save_state(); }
    }
  }
  require(waiting_moved, "eligible actors without a slot continue ordinary movement rather than planted aim waiting");
  require(preparing.has_value() && state.actors.front().view.warning_seconds > 0,
      "an owner converts its full-budget reservation without waiting for a second free slot");
  require_near(state.actors.front().view.warning_seconds, .9F, .00001F, "reservation preserves the complete authored warning");
  for (unsigned interruption = 0; interruption < 3; ++interruption) {
    auto cancelled = *preparing;
    if (interruption == 0) cancelled.actors.front().view.disabled_seconds = 1;
    else if (interruption == 1) cancelled.actors.front().lifted = true;
    else cancelled.actors.front().removed = true;
    cancelled.enemies(dt);
    require(cancelled.actors[vehicle_index].view.warning_seconds > 0 && occupied_threats(cancelled) == tune::encounter_build_warning_cap,
        "disable, lift or removal releases the reservation for a later ready vehicle in the same actor pass");
  }
  game::Game live, cold;
  require(!saved.empty() && live.restore_state(saved) == game::SaveStateIssue::none &&
      cold.restore_state(saved) == game::SaveStateIssue::none && live.save_state() == saved && cold.save_state() == saved,
      "a legally full reserved preparation cold restores exactly");
  const auto& before = live.snapshot().entities.front(); const auto& after = cold.snapshot().entities.front();
  require(before.preparing && after.preparing && before.body_heading == after.body_heading &&
      before.aim_pose_seconds == after.aim_pose_seconds && before.warning_aim.x == after.warning_aim.x,
      "first cold reserved draw retains refreshed target, physical facing and actual pose age");
  for (unsigned future = 0; future < 180; ++future) {
    for (auto* instance : {&live, &cold}) if (instance->snapshot().phase == game::Phase::choosing_upgrade)
      require(instance->choose_upgrade(0), "reserved continuation accepts the same earned choice");
    const game::Control control{{future < 30 ? -.5F : .5F, .1F}, future == 60};
    live.advance(dt, control); cold.advance(dt, control);
    require(!live.save_state().empty() && live.save_state() == cold.save_state(),
        "reserved preparation cold future stays exact through target changes, warning release and Dash");
  }
  pass("legal preparation reservations convert, block theft, release and cold restore without freezing waiting actors");
}
void reservation_contraction_and_boss_drain() {
  game::Game::State state;
  state.view.stats.active_seconds = tune::encounter_build_seconds + 1;
  for (unsigned i = 0; i < tune::encounter_assault_warning_cap; ++i) {
    auto a = soldier(game::InfantryRole::rifleman, i + 1);
    a.view.position = {-4.5F + 3.F * static_cast<float>(i), 8};
    a.locomotion_velocity = {0, 1}; state.actors.push_back(a);
  }
  state.enemies(dt);
  require(occupied_threats(state) == tune::encounter_assault_warning_cap &&
      std::all_of(state.actors.begin(), state.actors.end(), [](const auto& a) { return a.infantry_aim.preparing; }),
      "assault fixture earns its actual preparations under the larger cap");
  auto waiting = soldier(game::InfantryRole::rifleman, 10); waiting.view.position = {-5, 12};
  state.actors.push_back(waiting);
  state.view.stats.active_seconds = tune::encounter_build_seconds + tune::encounter_assault_seconds + 1;
  bool committed = false;
  for (unsigned frame = 0; frame < 21 && !committed; ++frame) {
    state.enemies(dt);
    require(!state.actors.back().infantry_aim.preparing && state.actors.back().view.warning_seconds == 0,
        "contracted phase cap blocks fresh reservations while previous owners drain");
    committed = std::all_of(state.actors.begin(), state.actors.end() - 1, [](const auto& a) { return a.view.warning_seconds > 0; });
  }
  require(committed && occupied_threats(state) == tune::encounter_assault_warning_cap,
      "previous owners convert their own reservations despite phase cap contraction without a self-slot deadlock");
  for (const bool boss_first : {false, true}) {
    game::Game::State encounter;
    for (unsigned i = 0; i < tune::encounter_build_warning_cap; ++i) {
      auto a = soldier(game::InfantryRole::rifleman, i + 1);
      a.view.position = {i == 0 ? -4.F : 4.F, 8}; a.locomotion_velocity = {0, 1};
      encounter.actors.push_back(a);
    }
    encounter.enemies(dt);
    require(occupied_threats(encounter) == tune::encounter_build_warning_cap, "boss drain starts after actual preparation admission");
    encounter.view.stats.active_seconds = 155;
    game::detail::Actor boss; boss.view.id = 20; boss.view.kind = game::EntityKind::boss;
    boss.view.position = {0, 10}; boss.view.hull = boss.view.maximum_hull = 160; boss.view.body_heading = 0;
    if (boss_first) encounter.actors.insert(encounter.actors.begin(), boss); else encounter.actors.push_back(boss);
    const auto boss_index = boss_first ? 0U : encounter.actors.size() - 1;
    for (unsigned frame = 0; frame < 180 && encounter.actors[boss_index].view.warning_seconds == 0; ++frame) encounter.enemies(dt);
    require(encounter.actors[boss_index].view.warning_seconds > 0,
        "a boss before or after preparers still drains their reservations and commits its reserved volley");
    require(std::none_of(encounter.actors.begin(), encounter.actors.end(), [](const auto& a) {
      return a.view.kind != game::EntityKind::boss && (a.view.warning_seconds > 0 || a.infantry_aim.preparing);
    }), "planted boss warning excludes both ordinary warnings and uncommitted preparations");
  }
  pass("phase contraction retains existing owners and boss drain cancels preparations in either actor order");
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
    for (unsigned frame = 0; frame < 180 && state.bullets.empty(); ++frame)
      state.enemies(dt); // Earn the real physical preparation, lock and release.
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
  // Codec-three kind-aware offsets are deliberate corruption fixtures.
  constexpr std::size_t actor = 378, role = actor + 49, recovery = actor + 50;
  constexpr std::size_t projectile = actor + 106 + 4, projectile_role = projectile + 37;
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
void ordinary_physical_frames_plant_and_emit_exact_sources() {
  for(const auto kind:{game::EntityKind::jeep,game::EntityKind::tank})for(bool reverse:{false,true}) {
    game::Game::State state;state.view.player={0,0};state.view.hull=state.view.maximum_hull=100;
    game::detail::Actor source;source.view.id=3;source.view.kind=kind;
    source.view.hull=source.view.maximum_hull=90;
    const auto index=game::detail::military_index(kind);
    source.attack_timer=tune::enemy_attack_period[index]*.5F;
    state.view.stats.active_seconds=tune::director_boss_first_second-.5;
    source.view.position={reverse?tune::field_half_width-tune::military_body_radius[index]:-4.F,12};
    source.locomotion_velocity=reverse?game::Vec2{1,0}:game::Vec2{0,-1};
    source.view.body_heading=reverse?1.57079632679F:0.F;
    state.actors.push_back(source);
    bool moved=false;
    for(unsigned tick=0;tick<180;++tick) {
      state.enemies(dt);const auto& a=state.actors.front();
      const auto facing=game::detail::direction(a.locomotion_velocity);
      const float longitudinal=a.view.velocity.x*facing.x+a.view.velocity.z*facing.z;
      if(reverse?longitudinal<-.05F:std::abs(*a.view.body_heading)>.02F) {moved=true;break;}
    }
    if(!moved) {const auto& a=state.actors.front();std::cerr<<"physical-source kind="<<static_cast<unsigned>(kind)<<" reverse="<<reverse<<" final="<<a.view.position.x<<','<<a.view.position.z<<" v="<<a.view.velocity.x<<','<<a.view.velocity.z<<" face="<<a.locomotion_velocity.x<<','<<a.locomotion_velocity.z<<" age="<<a.age<<'\n';}
    require(moved,"real director reaches forward-turn and backing physical frames before weapon commit");
    state.view.stats.active_seconds=0;state.actors.front().attack_timer=0;state.enemies(dt);
    const auto locked=state.actors.front();
    require(locked.view.warning_seconds>0,"physical moving source commits an actual eligible warning");
    const auto shell=game::vehicle_cannon_shot(locked.view,locked.view.warning_aim);
    const auto gun=game::jeep_minigun_shot(locked.view,locked.view.warning_aim,locked.view.minigun_barrel);
    const auto origin=kind==game::EntityKind::jeep?gun.origin:shell.origin;
    const auto axis=kind==game::EntityKind::jeep?gun.direction:shell.direction;
    require(kind==game::EntityKind::jeep?gun.valid:shell.valid,"committed ray is valid in the same accepted physical chassis frame");
    for(unsigned tick=0;tick<120 && state.bullets.empty();++tick) {
      state.view.player={-2,-1};state.enemies(dt);
      const auto& a=state.actors.front();
      near(a.view.position,locked.view.position,0,"warning forbids both physical translation and reverse escape");
      near(a.locomotion_velocity,locked.locomotion_velocity,0,"warning freezes canonical chassis facing instead of following velocity sign");
      require(a.view.body_heading==locked.view.body_heading,"warning/source body yaw remains frozen exactly");
      require(a.escape.phase==locked.escape.phase && a.escape.turn_remaining==locked.escape.turn_remaining && a.escape.forward_remaining==locked.escape.forward_remaining,"planted warning retains the admitted maneuver phase, side and exit distance exactly");
    }
    require(state.bullets.size()==1,"one ordinary committed warning emits exactly one projectile");
    near(state.bullets.front().view.launch_position,origin,0,"turn/reverse warning projectile exits its exact committed physical muzzle");
    near(state.bullets.front().view.launch_direction,axis,0,"turn/reverse release uses the exact committed barrel axis");
    for(unsigned tick=0;tick<8;++tick) {state.enemies(dt);near(state.actors.front().view.position,locked.view.position,0,"existing shot recovery remains planted");}
    for(bool lifted:{false,true}) {
      auto& a=state.actors.front();a.lifted=lifted;a.view.disabled_seconds=lifted?0.F:1.F;
      const auto before=a.locomotion_velocity;const auto escape=a.escape;state.enemies(dt);
      near(a.view.velocity,{},0,"disabled and lifted vehicles have zero physical travel");
      near(a.locomotion_velocity,before,0,"disabled and lifted stops retain physical facing");
      require(a.escape.phase==escape.phase && a.escape.turn_remaining==escape.turn_remaining && a.escape.forward_remaining==escape.forward_remaining,"disable and lift cannot consume or replace the committed maneuver");
    }
  }
  pass("ordinary turn/reverse physical frames own warning, muzzle, recovery and stop poses");
}
} // namespace

int main() {
  encounter_stream_and_bounds();
  planted_telegraph_and_recovery();
  physical_preparation_aligns_and_cancels();
  held_warning_slots_keep_provisional_aim_fresh_and_durable();
  reservation_contraction_and_boss_drain();
  attack_cadence();
  cancellation_and_warning_cap();
  volleys_and_capacity();
  muzzle_geometry_and_invalid_aims();
  continuation_and_revision();
  malformed_military_authority();
  ordinary_physical_frames_plant_and_emit_exact_sources();
  std::cout << "RESULT " << groups << " military gameplay groups passed, zero failures\n";
}
