#include "human_resources/gameplay/gameplay.h"
#include "human_resources/gameplay/tuning.h"
#include "test_support.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>

namespace game = human_resources::gameplay;
using test_support::require;
namespace {
using Bytes = std::vector<std::byte>;
// HRS1 authority-eight retains these no-boss fieldwise offsets from revision three.
constexpr std::size_t phase_offset = 36, player_offset = 37, hull_offset = 57;
constexpr std::size_t actor_count_offset = 374, actor_stride = 101;
void put32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
  require(offset + 4 <= bytes.size(), "mutation offset exists");
  for (std::size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 255U);
}
std::uint32_t get32(const Bytes& bytes, std::size_t offset) {
  require(offset + 4 <= bytes.size(), "read offset exists");
  std::uint32_t value{};
  for (std::size_t i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(std::to_integer<unsigned>(bytes[offset + i])) << (i * 8);
  return value;
}
void put64(Bytes& bytes, std::size_t offset, std::uint64_t value) {
  for (std::size_t i = 0; i < 8; ++i) bytes[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 255U);
}
void put_float(Bytes& bytes, std::size_t offset, float value) { put32(bytes, offset, std::bit_cast<std::uint32_t>(value)); }
void seal(Bytes& bytes) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (std::size_t i = 0; i < bytes.size() - 8; ++i) hash = (hash ^ std::to_integer<std::uint8_t>(bytes[i])) * 1099511628211ULL;
  for (std::size_t i = 0; i < 8; ++i) bytes[bytes.size() - 8 + i] = static_cast<std::byte>((hash >> (i * 8)) & 255U);
}
void same(const game::Game& a, const game::Game& b) {
  const auto first = a.save_state(), second = b.save_state();
  require(!first.empty(), "live authority has a valid save");
  require(first == second, "all future simulation authority is bit-exact after continuation");
  const auto& old_boss = a.snapshot().boss_combat;
  const auto& restored_boss = b.snapshot().boss_combat;
  require(old_boss.has_value() == restored_boss.has_value(), "boss presence is exact on the first restored projection");
  if (old_boss) {
    require(old_boss->actor_id == restored_boss->actor_id && old_boss->phase == restored_boss->phase &&
        old_boss->phase_remaining == restored_boss->phase_remaining && old_boss->phase_duration == restored_boss->phase_duration &&
        old_boss->next_release == restored_boss->next_release,
        "first restored boss projection uses canonical saved phase and release clocks");
    const auto entity = [](const game::Snapshot& view, std::uint32_t id) {
      return std::find_if(view.entities.begin(), view.entities.end(), [id](const auto& actor) { return actor.id == id; });
    };
    const auto old_actor = entity(a.snapshot(), old_boss->actor_id), new_actor = entity(b.snapshot(), restored_boss->actor_id);
    require(old_actor != a.snapshot().entities.end() && new_actor != b.snapshot().entities.end() &&
        old_actor->body_heading.has_value() && old_actor->body_heading == new_actor->body_heading &&
        old_actor->warning_seconds == new_actor->warning_seconds && old_actor->fire_recovery_seconds == new_actor->fire_recovery_seconds,
        "cold/paused first draw retains boss heading and coherent warning/recovery projection without waiting for advance");
  }
}
void tick(game::Game& a, std::size_t frame) {
  if (a.snapshot().phase == game::Phase::choosing_upgrade) {
    if (frame % 3 == 0) (void)a.reroll();
    require(a.choose_upgrade(frame % 3), "continued draft is selectable");
  }
  const float x = std::sin(static_cast<float>(frame) * 0.021F);
  const float z = std::cos(static_cast<float>(frame) * 0.013F) * 0.7F;
  a.advance(frame % 5 == 0 ? 0.021F : 0.016F, {{x, z}, frame % 97 == 0});
}
void roundtrip(game::Game& original, std::size_t frame, std::size_t future = 240) {
  const auto bytes = original.save_state();
  require(!bytes.empty() && bytes.size() <= 48 * 1024, "save fits bounded portable payload");
  const auto info = game::inspect_saved_state(bytes);
  require(info.valid() && info.phase == original.snapshot().phase, "inspection validates and preserves exact phase");
  require(info.stats.score == original.snapshot().stats.score && info.stats.level == original.snapshot().stats.level,
      "inspection exposes exact receipt counters");
  game::Game restored;
  restored.start({987});
  require(restored.restore_state(bytes) == game::SaveStateIssue::none, "valid state restores transactionally");
  same(original, restored);
  require(restored.snapshot().shots.empty(), "restored shot history is cleared");
  require(std::all_of(restored.snapshot().presentation_events.begin(), restored.snapshot().presentation_events.end(),
      [](const auto& event) { return event.sequence == 0; }), "restored feedback history is cleared");
  for (std::size_t step = 0; step < future; ++step) {
    tick(original, frame + step); tick(restored, frame + step);
    same(original, restored);
  }
}
void phases_and_future() {
  game::Game run;
  roundtrip(run, 0, 0);
  game::RunSetup setup{19, {2, 3, 4, 5}, (1U << static_cast<unsigned>(game::Module::count)) - 1};
  run.start(setup);
  const auto info = game::inspect_saved_state(run.save_state());
  require(info.setup.seed == setup.seed && info.setup.permanent_ranks == setup.permanent_ranks &&
      info.setup.unlocked_blueprints == setup.unlocked_blueprints, "original normalized setup is retained");
  run.advance(0.005F);
  roundtrip(run, 0, 0); // Fractional fixed-step accumulator.
  run.pause();
  const auto paused = run.save_state();
  roundtrip(run, 1, 20);
  require(run.save_state() == paused, "paused save and all private clocks freeze");
  run.resume();
  bool choice = false, active = false, death = false, bullets = false, beams = false;
  for (std::size_t frame = 0; frame < 60000; ++frame) {
    run.advance(1.0F / 60);
    bullets = bullets || !run.snapshot().projectiles.empty();
    beams = beams || !run.snapshot().beams.empty();
    if (run.snapshot().phase == game::Phase::choosing_upgrade) {
      choice = true;
      const auto cards = run.snapshot().choices;
      const auto bytes = run.save_state();
      game::Game restored;
      require(restored.restore_state(bytes) == game::SaveStateIssue::none, "choice restores");
      for (std::size_t i = 0; i < 3; ++i) {
        require(restored.snapshot().choices[i].title == cards[i].title && restored.snapshot().choices[i].description == cards[i].description &&
            restored.snapshot().choices[i].evolution_preview == cards[i].evolution_preview &&
            restored.snapshot().choices[i].effect_label == cards[i].effect_label &&
            restored.snapshot().choices[i].effect_unit == cards[i].effect_unit &&
            restored.snapshot().choices[i].conditions == cards[i].conditions,
            "draft identity rebuilds exact complete and structured offer text without reroll");
      }
      require(run.reroll() == restored.reroll(), "reroll availability retained");
      same(run, restored);
      roundtrip(run, frame);
    } else if (run.snapshot().phase == game::Phase::active && frame % 997 == 0) {
      active = true; roundtrip(run, frame);
    }
    if (run.snapshot().phase == game::Phase::dead) {
      death = true; roundtrip(run, frame);
      if (run.revive_available()) {
        game::Game restored;
        require(restored.restore_state(run.save_state()) == game::SaveStateIssue::none && restored.revive(), "dead save explicitly revives only after caller grants it");
        require(run.revive(), "matching original revive succeeds");
        same(run, restored);
        require(game::inspect_saved_state(run.save_state()).revive_count == 1, "revive use persists");
        roundtrip(run, frame);
      } else break;
    }
  }
  require(choice && active && death && bullets && beams, "actual gameplay covers all phases, projectiles and captures");
}
void corruption_and_rollback() {
  game::Game run;
  run.start({17});
  const auto good = run.save_state();
  require(get32(good, actor_count_offset) == run.snapshot().entities.size(), "wire actor-count offset is versioned");
  auto reject = [&](const Bytes& bad, game::SaveStateIssue issue = game::SaveStateIssue::malformed) {
    require(run.restore_state(bad) == issue, "invalid state reports specific failure");
    require(game::inspect_saved_state(bad).issue == issue, "inspection applies identical validation");
    require(run.save_state() == good, "failed restore leaves existing authority untouched");
  };
  for (std::size_t length = 0; length < good.size(); ++length) reject(Bytes(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(length)));
  auto bad = good; bad.push_back(std::byte{}); reject(bad);
  reject(Bytes(48 * 1024 + 1));
  bad = good; put32(bad, 4, 2); reject(bad, game::SaveStateIssue::unsupported_schema);
  bad = good; put32(bad, 8, 0xffffffffU); reject(bad, game::SaveStateIssue::incompatible_rules);
  bad = good; put32(bad, 8, 7); bad.resize(28);
  reject(bad, game::SaveStateIssue::incompatible_rules); // Reject old authority before missing body/checksum.
  bad = good; bad[12] ^= std::byte{1}; reject(bad, game::SaveStateIssue::incompatible_rules);
  bad = good; bad[100] ^= std::byte{1}; reject(bad);
  bad = good; put32(bad, player_offset, 0x7fc00000U); seal(bad); reject(bad);
  bad = good; bad[phase_offset] = std::byte{255}; seal(bad); reject(bad);
  bad = good; put32(bad, hull_offset, std::bit_cast<std::uint32_t>(-1.0F)); seal(bad); reject(bad);
  bad = good; put32(bad, actor_count_offset, 129); seal(bad); reject(bad);
  const auto first_actor = actor_count_offset + 4;
  bad = good; put32(bad, first_actor + actor_stride, get32(good, first_actor)); seal(bad); reject(bad);
  const auto projectile_count = first_actor + actor_stride * get32(good, actor_count_offset);
  require(get32(good, projectile_count) == 0, "starter projectile count is zero");
  bad = good; put32(bad, projectile_count, 97); seal(bad); reject(bad);
  const auto beam_count = projectile_count + 4;
  require(get32(good, beam_count) == 0, "starter beam count is zero");
  bad = good;
  put32(bad, beam_count, 1);
  bad.insert(bad.end() - 8, 17, std::byte{}); // id, target, progress, bool
  put32(bad, beam_count + 4, 0xffffU); seal(bad); reject(bad);
}
void many_natural_checkpoints() {
  std::size_t saved = 0;
  for (std::uint64_t seed = 1; seed <= 12; ++seed) {
    game::Game run;
    run.start({seed, {5, 5, 5, 5}, 0xffffU});
    for (std::size_t frame = 0; frame < 24000; ++frame) {
      tick(run, frame);
      if (frame % 113 == 0 || run.snapshot().phase == game::Phase::choosing_upgrade) {
        if (run.save_state().empty()) {
          std::cerr << "Invalid natural checkpoint seed " << seed << " frame " << frame << '\n';
          require(false, "natural authority always encodes");
        }
        roundtrip(run, frame, 8); ++saved;
      }
      if (run.snapshot().phase == game::Phase::dead) {
        if (!run.revive()) break;
      }
    }
  }
  require(saved > 500, "broad public gameplay exercises hundreds of private-clock checkpoints");
}
void hidden_authority_fixtures() {
  // Explicitly synthetic legal six-slot builds exercise every private combat
  // clock and all six evolution identities without claiming earned progression.
  const std::array<std::array<game::Module, 6>, 3> builds{{
      {{game::Module::beam_splitter, game::Module::tractor_lens, game::Module::emp_ring,
        game::Module::vector_thrusters, game::Module::phase_drive, game::Module::bounty_scanner}},
      {{game::Module::laser_array, game::Module::rail_lens, game::Module::beam_splitter,
        game::Module::heavy_winch, game::Module::overtime_reactor, game::Module::reserve_battery}},
      {{game::Module::shield_generator, game::Module::mirror_plating, game::Module::orbit_drones,
        game::Module::repair_swarm, game::Module::phase_drive, game::Module::laser_array}}
  }};
  const std::array<std::uint32_t, 3> evolutions{{(1U << 0) | (1U << 5), (1U << 1) | (1U << 2), (1U << 3) | (1U << 4)}};
  for (std::size_t build = 0; build < builds.size(); ++build) {
    game::Game run;
    run.start({77, {5, 5, 5, 5}, 0xffffU});
    auto bytes = run.save_state();
    for (const auto module : builds[build]) bytes[85 + static_cast<std::size_t>(module)] = std::byte{3};
    put32(bytes, 129, evolutions[build]); put32(bytes, 217, 2);
    put64(bytes, 157, 10); put64(bytes, 261, std::bit_cast<std::uint64_t>(10.625)); // Fractional score.
    put_float(bytes, 65, 64); // Surplus XP must continue through successive drafts.
    put_float(bytes, 277, 0.005F); // Preserve partial fixed step.
    // Differing, nonzero values make a missing/zeroed/swapped private field fail
    // the immediate byte equality or the subsequent real gameplay continuation.
    for (std::size_t offset = 281; offset <= 353; offset += 4)
      put_float(bytes, offset, 0.01F * static_cast<float>(1 + (offset - 281) / 4));
    seal(bytes);
    require(run.restore_state(bytes) == game::SaveStateIssue::none, "synthetic hidden-clock fixture is valid");
    require(run.save_state() == bytes, "all named private clocks and fractional score survive fieldwise roundtrip");
    roundtrip(run, 11, 600);
  }
}
void append32(Bytes& bytes, std::uint32_t value) {
  const auto offset = bytes.size(); bytes.resize(offset + 4); put32(bytes, offset, value);
}
void append_float(Bytes& bytes, float value) { append32(bytes, std::bit_cast<std::uint32_t>(value)); }
Bytes maximum_boss_population(const Bytes& actual_recovery) {
  // Deliberately synthetic codec-capacity fixture, using a real completed rake
  // plan and mobile phase from Game::advance rather than rebuilding boss fields.
  constexpr std::size_t projectile_stride = 64, beam_stride = 17;
  const auto actor_count = get32(actual_recovery, actor_count_offset);
  const auto first_actor = actor_count_offset + 4;
  Bytes boss_actor;
  for (std::size_t i = 0; i < actor_count; ++i) {
    const auto offset = first_actor + i * actor_stride;
    if (actual_recovery[offset + 4] == static_cast<std::byte>(game::EntityKind::boss))
      boss_actor = Bytes(actual_recovery.begin() + static_cast<std::ptrdiff_t>(offset),
          actual_recovery.begin() + static_cast<std::ptrdiff_t>(offset + actor_stride));
  }
  require(boss_actor.size() == actor_stride, "actual recovery has exactly one boss record");
  const auto boss_id = get32(boss_actor, 0);
  const auto projectile_count_offset = first_actor + actor_count * actor_stride;
  const auto beam_count_offset = projectile_count_offset + 4 + get32(actual_recovery, projectile_count_offset) * projectile_stride;
  const auto boss_suffix_offset = beam_count_offset + 4 + get32(actual_recovery, beam_count_offset) * beam_stride;
  require(boss_suffix_offset < actual_recovery.size() - 8, "version-eight conditional boss suffix follows populations");
  Bytes bytes(actual_recovery.begin(), actual_recovery.begin() + static_cast<std::ptrdiff_t>(actor_count_offset));
  unsigned total = game::tuning::biological_active_cap;
  for (const auto cap : game::tuning::enemy_active_cap) total += static_cast<unsigned>(cap);
  append32(bytes, total);
  bytes.insert(bytes.end(), boss_actor.begin(), boss_actor.end());
  std::uint32_t next_id = boss_id + 1;
  auto add_actor = [&](game::EntityKind kind, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
      Bytes actor(actor_stride);
      put32(actor, 0, next_id++); actor[4] = static_cast<std::byte>(kind);
      put_float(actor, 5, static_cast<float>(i % 5) * 3.F - 6.F);
      put_float(actor, 9, 20.F + static_cast<float>(i / 5) * 3.5F);
      put_float(actor, 29, 10); put_float(actor, 33, 10);
      if (kind == game::EntityKind::soldier) actor[49] = static_cast<std::byte>(game::InfantryRole::rifleman);
      bytes.insert(bytes.end(), actor.begin(), actor.end());
    }
  };
  add_actor(game::EntityKind::farmer, game::tuning::biological_active_cap);
  add_actor(game::EntityKind::soldier, static_cast<unsigned>(game::tuning::enemy_active_cap[0]));
  add_actor(game::EntityKind::jeep, static_cast<unsigned>(game::tuning::enemy_active_cap[1]));
  add_actor(game::EntityKind::tank, static_cast<unsigned>(game::tuning::enemy_active_cap[2]));
  append32(bytes, game::tuning::projectile_active_cap);
  for (unsigned i = 0; i < game::tuning::projectile_active_cap; ++i) {
    const auto start = bytes.size();
    append32(bytes, next_id++); // identity
    append_float(bytes, static_cast<float>(i % 12) - 5.5F); append_float(bytes, 20.F + static_cast<float>(i / 12));
    append_float(bytes, game::tuning::projectile_speed[3]); append_float(bytes, 0); // velocity
    append_float(bytes, game::tuning::projectile_radius[3]); append_float(bytes, game::tuning::director_boss_damage);
    append32(bytes, boss_id); append_float(bytes, .5F); // source and age
    bytes.push_back(std::byte{}); bytes.push_back(static_cast<std::byte>(game::InfantryRole::none));
    append_float(bytes, 0); append_float(bytes, 20); // launch position
    append_float(bytes, 2); append_float(bytes, 0); // launch height, rise
    bytes.push_back(static_cast<std::byte>(game::EnemyProjectileKind::boss_missile)); bytes.push_back(std::byte{});
    append_float(bytes, 1); append_float(bytes, 0); // physical source axis
    require(bytes.size() - start == projectile_stride, "projectile fixture wire stride is explicit");
  }
  append32(bytes, 0); // No beam is needed for the actor/projectile capacity bound.
  bytes.insert(bytes.end(), actual_recovery.begin() + static_cast<std::ptrdiff_t>(boss_suffix_offset), actual_recovery.end() - 8);
  bytes.resize(bytes.size() + 8); put32(bytes, 245, next_id); seal(bytes);
  game::Game restored;
  require(restored.restore_state(bytes) == game::SaveStateIssue::none && restored.save_state() == bytes,
      "all legal biological/military/projectile caps and actual complete boss plan fit exact codec roundtrip");
  require(restored.snapshot().entities.size() == total && restored.snapshot().projectiles.size() == game::tuning::projectile_active_cap,
      "capacity fixture actually fills all requested population slots");
  require(bytes.size() <= 48 * 1024, "maximum legal actor/projectile population and full boss plan retain the unchanged 48KiB Game cap");
  return bytes;
}
void malformed_boss_plans_preserve_live_run(const Bytes& warning) {
  constexpr std::size_t projectile_stride = 64, beam_stride = 17;
  const auto projectiles = actor_count_offset + 4 + get32(warning, actor_count_offset) * actor_stride;
  const auto beams = projectiles + 4 + get32(warning, projectiles) * projectile_stride;
  const auto boss = beams + 4 + get32(warning, beams) * beam_stride;
  // Revision-eight conditional prefix is18 bytes; a complete three-shell plan
  // is160 more. Explicit offsets intentionally test the wire trust boundary.
  require(warning.size() - 8 - boss == 178, "warning fixture has exactly one full conditional boss plan");
  game::Game live;
  require(live.restore_state(warning) == game::SaveStateIssue::none, "unmodified actual boss warning is valid");
  auto reject = [&](Bytes bad) {
    seal(bad);
    require(game::inspect_saved_state(bad).issue == game::SaveStateIssue::malformed &&
        live.restore_state(bad) == game::SaveStateIssue::malformed && live.save_state() == warning,
        "invalid boss plan is rejected without replacing an existing live continuation");
  };
  auto byte = [&](std::size_t relative, std::byte value) { auto bad = warning; bad[boss + relative] = value; reject(std::move(bad)); };
  auto scalar = [&](std::size_t relative, float value) { auto bad = warning; put_float(bad, boss + relative, value); reject(std::move(bad)); };
  const auto boss_id = get32(warning, boss);
  std::size_t boss_actor = actor_count_offset + 4;
  while (boss_actor < projectiles && get32(warning, boss_actor) != boss_id) boss_actor += actor_stride;
  require(boss_actor < projectiles, "conditional boss state has an actor for cache corruption tests");
  auto bad = warning; put_float(bad, boss_actor + 41, 0); reject(std::move(bad)); // Saved warning contradicts active phase.
  bad = warning; put_float(bad, boss_actor + 50, .1F); reject(std::move(bad)); // Recovery cannot run during warning.
  bad = warning; put32(bad, boss, 0); reject(std::move(bad)); // Missing owner.
  byte(4, std::byte{255}); byte(5, std::byte{255}); // Pattern/phase enums.
  byte(6, std::byte{255}); byte(7, std::byte{255}); // Remaining exceeds its finite phase budget.
  byte(8, std::byte{}); // No canonical total budget.
  byte(10, std::byte{4}); // Fourth release does not exist.
  byte(11, std::byte{2}); byte(12, std::byte{2}); // Booleans must be canonical.
  scalar(13, std::numeric_limits<float>::quiet_NaN());
  byte(17, std::byte{}); byte(18, std::byte{255}); // Missing/invalid plan.
  scalar(19, 1000); // Stored root no longer owns its solved source frame.
  scalar(27, 0); scalar(31, -1); // Warning/salvo duration cannot be fabricated.
  scalar(43, 1000); // First physical origin no longer agrees with the locked frame.
  bad = warning; put_float(bad, boss + 51, 0); put_float(bad, boss + 55, 0); reject(std::move(bad)); // Non-unit physical ray.
  scalar(84, .1F); // The first shell cannot be retimed after commitment.
}
void boss_phase_continuations() {
  game::Game run; run.start({.seed = 514, .permanent_ranks = {5, 5, 5, 5}});
  auto early = run.save_state();
  // Existing documented field offsets isolate an early encounter; the director
  // itself must spawn the boss and own every plan, turn, timer and release.
  put64(early, 269, std::bit_cast<std::uint64_t>(0.0));
  early.resize(actor_count_offset + 4 + 4 + 4 + 8);
  put32(early, actor_count_offset, 0); put32(early, actor_count_offset + 4, 0); put32(early, actor_count_offset + 8, 0); seal(early);
  require(run.restore_state(early) == game::SaveStateIssue::none, "early ordinary director fixture is valid");
  std::array<Bytes, 8> checkpoints;
  for (unsigned frame = 0; frame < 3600 && std::any_of(checkpoints.begin(), checkpoints.end(), [](const auto& b) { return b.empty(); }); ++frame) {
    if (run.snapshot().phase == game::Phase::choosing_upgrade) require(run.choose_upgrade(0), "boss fixture uses actual earned draft");
    if (run.snapshot().phase != game::Phase::active) {
      std::cerr << "Boss checkpoint fixture ended at " << run.snapshot().stats.active_seconds << "s, saved phases:";
      for (std::size_t i = 0; i < checkpoints.size(); ++i) std::cerr << ' ' << i << '=' << !checkpoints[i].empty();
      std::cerr << " boss kills=" << run.snapshot().stats.boss_kills << '\n';
    }
    require(run.snapshot().phase == game::Phase::active, "bounded boss encounter remains active");
    run.advance(1.F / 60);
    const auto& boss = run.snapshot().boss_combat;
    if (!boss || !boss->plan) continue;
    const bool mid = boss->phase_remaining > .03F && boss->phase_remaining + .1F < boss->phase_duration;
    const bool rake = boss->plan->kind == game::BossAttackKind::rake;
    auto keep = [&](std::size_t slot, bool condition) {
      if (condition && checkpoints[slot].empty()) checkpoints[slot] = run.save_state();
    };
    keep(0, mid && !rake && boss->phase == game::BossPhase::warning);
    keep(1, mid && rake && boss->phase == game::BossPhase::warning);
    keep(2, boss->phase == game::BossPhase::salvo && boss->next_release == 1);
    keep(3, boss->phase == game::BossPhase::salvo && boss->next_release == 2);
    keep(4, mid && !rake && boss->phase == game::BossPhase::recovery);
    keep(5, mid && rake && boss->phase == game::BossPhase::recovery);
    keep(6, mid && boss->phase == game::BossPhase::reposition);
    keep(7, mid && boss->phase == game::BossPhase::braking);
  }
  for (std::size_t phase = 0; phase < checkpoints.size(); ++phase) {
    require(!checkpoints[phase].empty(), "actual director/boss advance reaches every saved attack phase and inter-shell boundary");
    game::Game continued;
    require(continued.restore_state(checkpoints[phase]) == game::SaveStateIssue::none, "actual boss phase checkpoint restores");
    continued.pause(); const auto paused = continued.save_state(); continued.advance(10);
    require(continued.save_state() == paused, "boss plan and all phase clocks freeze while paused");
    continued.resume();
    roundtrip(continued, phase * 7, 600);
  }
  malformed_boss_plans_preserve_live_run(checkpoints[0]);
  const auto maximum = maximum_boss_population(checkpoints[5]);
  std::cout << "Boss continuation phases=" << checkpoints.size() << " full-capacity-bytes=" << maximum.size() << '\n';
}
void area_damage_retires_owned_beam() {
  game::Game run;
  run.start({78, {}, 0xffffU});
  auto bytes = run.save_state();
  bytes[85 + static_cast<std::size_t>(game::Module::emp_ring)] = std::byte{3};
  const auto actor = actor_count_offset + 4;
  const auto target_id = get32(bytes, actor);
  bytes[actor + 4] = static_cast<std::byte>(game::EntityKind::soldier);
  bytes[actor + 49] = static_cast<std::byte>(game::InfantryRole::rifleman);
  put_float(bytes, actor + 5, 0); put_float(bytes, actor + 9, 0);
  bytes[actor + 95] = std::byte{1}; // Stable lifted authority, owned by the beam below.
  const auto beam_count = actor + actor_stride * get32(bytes, actor_count_offset) + 4;
  put32(bytes, beam_count, 1);
  bytes.insert(bytes.end() - 8, 17, std::byte{});
  put32(bytes, beam_count + 4, target_id);
  seal(bytes);
  require(run.restore_state(bytes) == game::SaveStateIssue::none, "EMP/capture ownership regression fixture restores");
  run.advance(1.0F / 60);
  require(std::none_of(run.snapshot().entities.begin(), run.snapshot().entities.end(),
      [target_id](const auto& actor_view) { return actor_view.id == target_id; }), "actual EMP kills the lifted soldier");
  require(std::none_of(run.snapshot().beams.begin(), run.snapshot().beams.end(),
      [target_id](const auto& beam) { return beam.target_id == target_id; }), "same-tick area defeat retires capture beam ownership");
  require(!run.save_state().empty(), "real post-area-damage state remains checkpointable");
}
} // namespace
int main() {
  phases_and_future(); corruption_and_rollback(); many_natural_checkpoints(); hidden_authority_fixtures(); boss_phase_continuations(); area_damage_retires_owned_beam();
  std::cout << "Gameplay continuation codec contracts passed\n";
}
