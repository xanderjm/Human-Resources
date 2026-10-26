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
// HRS1 codec3/authority10 retains the run prefix. Soldiers append the five-byte
// aim preparation; ordinary vehicles retain their nine-byte committed maneuver.
constexpr std::size_t phase_offset = 36, player_offset = 37, hull_offset = 57;
constexpr std::size_t actor_count_offset = 374, base_actor_bytes = 101;
std::size_t actor_bytes(game::EntityKind kind) {
  return base_actor_bytes+(kind==game::EntityKind::soldier?5U:
      kind==game::EntityKind::jeep||kind==game::EntityKind::tank?9U:0U);
}
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
std::size_t actor_bytes(const Bytes& bytes,std::size_t offset) {
  require(offset+5<=bytes.size(),"versioned actor record has its kind tag");
  return actor_bytes(static_cast<game::EntityKind>(std::to_integer<unsigned>(bytes[offset+4])));
}
std::size_t actors_end(const Bytes& bytes) {
  std::size_t offset=actor_count_offset+4;
  for(std::size_t i=0;i<get32(bytes,actor_count_offset);++i)offset+=actor_bytes(bytes,offset);
  require(offset<=bytes.size(),"kind-aware conditional actor records fit the portable fixture");
  return offset;
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
  for (const auto& old_actor : a.snapshot().entities) {
    if (old_actor.kind != game::EntityKind::soldier) continue;
    const auto restored = std::find_if(b.snapshot().entities.begin(), b.snapshot().entities.end(),
        [&](const auto& actor) { return actor.id == old_actor.id; });
    require(restored != b.snapshot().entities.end() && old_actor.body_heading.has_value() &&
        old_actor.body_heading == restored->body_heading && old_actor.preparing == restored->preparing &&
        old_actor.aim_pose_seconds == restored->aim_pose_seconds &&
        old_actor.warning_aim.x == restored->warning_aim.x && old_actor.warning_aim.z == restored->warning_aim.z,
        "cold first draw publishes the saved physical infantry facing and real preparation clock");
  }
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
  require(get32(good,4)==3,"durable infantry preparation explicitly owns Game codec three");
  bad = good; put32(bad, 4, 4); reject(bad, game::SaveStateIssue::unsupported_schema);
  bad = good; put32(bad, 8, 0xffffffffU); reject(bad, game::SaveStateIssue::incompatible_rules);
  bad = good; put32(bad, 8, 7); bad.resize(28);
  reject(bad, game::SaveStateIssue::incompatible_rules); // Reject old authority before missing body/checksum.
  require(get32(good, 8) == 10, "physical infantry facing explicitly owns authority ten");
  bad = good; put32(bad, 8, 8); reject(bad, game::SaveStateIssue::incompatible_rules);
  bad.resize(28); reject(bad, game::SaveStateIssue::incompatible_rules);
  for(unsigned previous:{7U,8U,9U}) {
    bad=good;put32(bad,4,previous==9?2U:1U);put32(bad,8,previous);
    if(previous==9)put64(bad,12,13450486980726334161ULL);
    bad.resize(28);reject(bad,game::SaveStateIssue::unsupported_schema);
  }
  bad = good; bad[12] ^= std::byte{1}; reject(bad, game::SaveStateIssue::incompatible_rules);
  bad = good; bad[100] ^= std::byte{1}; reject(bad);
  bad = good; put32(bad, player_offset, 0x7fc00000U); seal(bad); reject(bad);
  bad = good; bad[phase_offset] = std::byte{255}; seal(bad); reject(bad);
  bad = good; put32(bad, hull_offset, std::bit_cast<std::uint32_t>(-1.0F)); seal(bad); reject(bad);
  bad = good; put32(bad, actor_count_offset, 129); seal(bad); reject(bad);
  const auto first_actor = actor_count_offset + 4;
  bad = good; put32(bad, first_actor + actor_bytes(good,first_actor), get32(good, first_actor)); seal(bad); reject(bad);
  const auto projectile_count = actors_end(good);
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
  std::size_t offset=first_actor;
  for (std::size_t i = 0; i < actor_count; ++i) {
    const auto stride=actor_bytes(actual_recovery,offset);
    if (actual_recovery[offset + 4] == static_cast<std::byte>(game::EntityKind::boss))
      boss_actor = Bytes(actual_recovery.begin() + static_cast<std::ptrdiff_t>(offset),
          actual_recovery.begin() + static_cast<std::ptrdiff_t>(offset + stride));
    offset+=stride;
  }
  require(boss_actor.size() == base_actor_bytes, "actual recovery has exactly one unchanged boss record");
  const auto boss_id = get32(boss_actor, 0);
  const auto projectile_count_offset = actors_end(actual_recovery);
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
      Bytes actor(actor_bytes(kind));
      put32(actor, 0, next_id++); actor[4] = static_cast<std::byte>(kind);
      put_float(actor, 5, static_cast<float>(i % 5) * 3.F - 6.F);
      put_float(actor, 9, 20.F + static_cast<float>(i / 5) * 3.5F);
      put_float(actor, 29, 10); put_float(actor, 33, 10);
      if (kind == game::EntityKind::soldier) actor[49] = static_cast<std::byte>(game::InfantryRole::rifleman);
      if (kind == game::EntityKind::soldier || kind == game::EntityKind::jeep || kind == game::EntityKind::tank) put_float(actor,91,-1);
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
  const auto projectiles = actors_end(warning);
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
  // A physically legal, uncommitted preparation is still incompatible with
  // the boss's already planted quiet window, even though no extra warning exists.
  Bytes preparer(actor_bytes(game::EntityKind::soldier));
  put32(preparer, 0, get32(warning, 245)); preparer[4] = static_cast<std::byte>(game::EntityKind::soldier);
  put_float(preparer, 5, -5); put_float(preparer, 9, 8);
  put_float(preparer, 29, 10); put_float(preparer, 33, 10);
  preparer[49] = static_cast<std::byte>(game::InfantryRole::rifleman);
  put_float(preparer, 55, 1); put_float(preparer, 91, -1);
  preparer[101] = std::byte{1}; put_float(preparer, 102, .1F);
  auto busy = warning;
  busy.insert(busy.begin() + static_cast<std::ptrdiff_t>(projectiles), preparer.begin(), preparer.end());
  put32(busy, actor_count_offset, get32(warning, actor_count_offset) + 1);
  put32(busy, 245, get32(warning, 245) + 1);
  reject(std::move(busy));
  auto byte = [&](std::size_t relative, std::byte value) { auto bad = warning; bad[boss + relative] = value; reject(std::move(bad)); };
  auto scalar = [&](std::size_t relative, float value) { auto bad = warning; put_float(bad, boss + relative, value); reject(std::move(bad)); };
  const auto boss_id = get32(warning, boss);
  std::size_t boss_actor = actor_count_offset + 4;
  while (boss_actor < projectiles && get32(warning, boss_actor) != boss_id) boss_actor += actor_bytes(warning,boss_actor);
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
  const auto original_record_bytes = actor_bytes(bytes, actor);
  require(original_record_bytes == base_actor_bytes, "EMP fixture starts with a biological actor record");
  bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(actor + original_record_bytes),
      actor_bytes(game::EntityKind::soldier) - original_record_bytes, std::byte{});
  bytes[actor + 4] = static_cast<std::byte>(game::EntityKind::soldier);
  bytes[actor + 49] = static_cast<std::byte>(game::InfantryRole::rifleman);
  put_float(bytes, actor + 5, 0); put_float(bytes, actor + 9, 0);
  put_float(bytes, actor + 13, 0); put_float(bytes, actor + 17, 0);
  put_float(bytes, actor + 87, 0); put_float(bytes, actor + 91, -1);
  bytes[actor + 95] = std::byte{1}; // Stable lifted authority, owned by the beam below.
  const auto beam_count = actors_end(bytes) + 4;
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
void physical_vehicle_continuations() {
  // Versioned legal wire fixtures set geometry/facing only. Every checkpoint
  // below is then produced by public Game::advance, never a private math model.
  for (unsigned mode=0; mode<4; ++mode) {
    game::Game run;run.start({514});
    const auto starter=run.save_state();
    const auto first_actor=actor_count_offset+4;
    const auto old_end=actors_end(starter);
    Bytes bytes(starter.begin(),starter.begin()+static_cast<std::ptrdiff_t>(actor_count_offset));
    append32(bytes,mode==2?5U:1U);
    auto append_actor=[&](std::uint32_t id,game::EntityKind kind,game::Vec2 position,game::Vec2 facing,float disabled) {
      Bytes actor(actor_bytes(kind));put32(actor,0,id);actor[4]=static_cast<std::byte>(kind);
      put_float(actor,5,position.x);put_float(actor,9,position.z);
      put_float(actor,29,90);put_float(actor,33,90);put_float(actor,45,disabled);
      put_float(actor,59,5); // No new warning until the physical pose is earned.
      if(kind==game::EntityKind::soldier)actor[49]=static_cast<std::byte>(game::InfantryRole::rifleman);
      put_float(actor,87,facing.x);put_float(actor,91,facing.z);
      if(mode==3 && id==100) {
        put_float(actor,41,.4F);put_float(actor,59,game::tuning::enemy_attack_period[1]);
      }
      bytes.insert(bytes.end(),actor.begin(),actor.end());
    };
    const float edge=game::tuning::field_half_width-game::tuning::military_body_radius[1];
    append_actor(100,game::EntityKind::jeep,mode==0?game::Vec2{edge,10}:mode==1?game::Vec2{-4,16}:game::Vec2{0,10},mode==0?game::Vec2{1,0}:game::Vec2{0,-1},0);
    if(mode==2) {
      const float gap=game::tuning::military_body_radius[1]+game::tuning::military_body_radius[0]+game::tuning::military_spacing_margin;
      for(unsigned side=0;side<4;++side) {
        const float angle=static_cast<float>(side)*1.57079632679F;
        append_actor(101+side,game::EntityKind::soldier,{gap*std::cos(angle),10+gap*std::sin(angle)},{0,-1},5);
      }
    }
    bytes.insert(bytes.end(),starter.begin()+static_cast<std::ptrdiff_t>(old_end),starter.end());
    put32(bytes,245,200);seal(bytes);
    require(run.restore_state(bytes)==game::SaveStateIssue::none,"legal physical motion fixture restores through public codec");
    bool observed=false;
    for(unsigned tick_index=0;tick_index<120;++tick_index) {
      run.advance(1.F/60);
      const auto& actor=run.snapshot().entities.front();
      require(actor.id==100 && actor.body_heading.has_value(),"actual director retains canonical ordinary vehicle facing");
      const float longitudinal=actor.velocity.x*std::sin(*actor.body_heading)-actor.velocity.z*std::cos(*actor.body_heading);
      const bool ready=mode==0?longitudinal<-.05F:mode==1?std::abs(*actor.body_heading)>.02F:mode==2?std::hypot(actor.velocity.x,actor.velocity.z)<.001F:actor.warning_seconds>0;
      if(!ready)continue;
      observed=true;
      const auto saved=run.save_state();game::Game cold;
      require(!saved.empty() && cold.restore_state(saved)==game::SaveStateIssue::none,"reverse, turn, blocked and planted checkpoints cold restore");
      require(cold.snapshot().entities.front().body_heading==actor.body_heading,"first cold draw has exact physical facing before any advance");
      same(run,cold);
      if(mode==0) {
        auto reject_motion=[&](Bytes bad) {seal(bad);require(cold.restore_state(bad)==game::SaveStateIssue::malformed && game::inspect_saved_state(bad).issue==game::SaveStateIssue::malformed,"nonphysical ordinary motion is rejected by both restore and inspection");require(cold.save_state()==saved,"malformed physical motion rejects transactionally");};
        auto bad=saved;put_float(bad,first_actor+87,0);put_float(bad,first_actor+91,0);reject_motion(bad);
        const float yaw=*actor.body_heading;
        bad=saved;put_float(bad,first_actor+13,std::cos(yaw));put_float(bad,first_actor+17,std::sin(yaw));reject_motion(bad);
        const float excessive=game::tuning::military_movement_speed[1]*(game::tuning::vehicle_reverse_speed_fraction+.1F);
        bad=saved;put_float(bad,first_actor+13,-excessive*std::sin(yaw));put_float(bad,first_actor+17,excessive*std::cos(yaw));reject_motion(bad);
        auto disabled=saved;put_float(disabled,first_actor+13,0);put_float(disabled,first_actor+17,0);put_float(disabled,first_actor+45,.25F);seal(disabled);
        game::Game disabled_copy;require(disabled_copy.restore_state(disabled)==game::SaveStateIssue::none && disabled_copy.save_state()==disabled,"disabled zero-velocity vehicle keeps its nonzero physical facing in valid continuation");
      }
      run.pause();cold.pause();const auto paused=run.save_state();run.advance(5,{{1,1},true});cold.advance(5);
      require(run.save_state()==paused && cold.save_state()==paused,"inactive interruption freezes saved physical motion and reverse intent");
      run.resume();cold.resume();
      for(unsigned future=0;future<180;++future) {tick(run,future);tick(cold,future);same(run,cold);}
      break;
    }
    require(observed,"bounded actual Game reaches each physical continuation state");
  }
}
void lifted_vehicle_keeps_facing_and_collected_removal_saves() {
  game::Game run;run.start({.seed=514,.unlocked_blueprints=0xffffU});const auto starter=run.save_state();
  const auto old_end=actors_end(starter);
  Bytes bytes(starter.begin(),starter.begin()+static_cast<std::ptrdiff_t>(actor_count_offset));append32(bytes,1);
  Bytes actor(actor_bytes(game::EntityKind::jeep));put32(actor,0,100);actor[4]=static_cast<std::byte>(game::EntityKind::jeep);
  put_float(actor,9,1);put_float(actor,29,10);put_float(actor,33,90);put_float(actor,37,.999F);put_float(actor,59,2);put_float(actor,91,-1);actor[95]=std::byte{1};
  bytes.insert(bytes.end(),actor.begin(),actor.end());append32(bytes,0);append32(bytes,1);
  append32(bytes,100);append_float(bytes,0);append_float(bytes,1);append_float(bytes,.999F);bytes.push_back(std::byte{1});
  // Starter has no projectiles/beams; retain its absent-boss tag and checksum.
  require(get32(starter,old_end)==0 && get32(starter,old_end+4)==0,"versioned lifted fixture starts without populations in its trailer");
  bytes.insert(bytes.end(),starter.begin()+static_cast<std::ptrdiff_t>(old_end+8),starter.end());
  bytes[85+static_cast<unsigned>(game::Module::laser_array)]=std::byte{1};
  bytes[85+static_cast<unsigned>(game::Module::beam_splitter)]=std::byte{2};bytes[85+static_cast<unsigned>(game::Module::heavy_winch)]=std::byte{1};
  put32(bytes,245,200);put_float(bytes,297,1);seal(bytes);
  require(run.restore_state(bytes)==game::SaveStateIssue::none,"lifted zero-velocity jeep with real two-slot beam retains canonical physical facing");
  require(run.snapshot().entities.front().body_heading==0.F && run.save_state()==bytes,"lifted cold first draw preserves exact physical chassis frame");
  run.advance(1.F/60);
  require(run.snapshot().stats.vehicle_captures==1 && run.snapshot().beams.empty() && std::none_of(run.snapshot().entities.begin(),run.snapshot().entities.end(),[](const auto& a){return a.id==100;}),"actual next capture step collects and removes the owned vehicle and beam");
  const auto saved=run.save_state();game::Game cold;require(!saved.empty() && cold.restore_state(saved)==game::SaveStateIssue::none && cold.save_state()==saved,"same-tick captured vehicle removal does not falsely invalidate a save");
}
void committed_vehicle_primitive_phase_continuations() {
  game::Game run;run.start({514});const auto starter=run.save_state();
  const auto first=actor_count_offset+4;
  Bytes bytes(starter.begin(),starter.begin()+static_cast<std::ptrdiff_t>(actor_count_offset));append32(bytes,1);
  Bytes actor(actor_bytes(game::EntityKind::jeep));put32(actor,0,100);actor[4]=static_cast<std::byte>(game::EntityKind::jeep);
  put_float(actor,5,game::tuning::field_half_width-game::tuning::military_body_radius[1]);put_float(actor,9,14);
  put_float(actor,29,90);put_float(actor,33,90);put_float(actor,59,5);put_float(actor,87,1);
  bytes.insert(bytes.end(),actor.begin(),actor.end());bytes.insert(bytes.end(),starter.begin()+static_cast<std::ptrdiff_t>(actors_end(starter)),starter.end());put32(bytes,245,200);seal(bytes);
  require(run.restore_state(bytes)==game::SaveStateIssue::none,"codec2 edge fixture starts with a canonical inactive maneuver");
  std::array<Bytes,4> checkpoints;
  for(unsigned frame=0;frame<600 && std::any_of(checkpoints.begin(),checkpoints.end(),[](const auto& b){return b.empty();});++frame) {
    if(run.snapshot().phase==game::Phase::choosing_upgrade)require(run.choose_upgrade(0),"primitive fixture accepts its actual earned choice");
    run.advance(1.F/60);const auto saved=run.save_state();require(!saved.empty(),"every actual committed maneuver tick remains checkpointable");
    require(get32(saved,first)==100,"versioned maneuver fixture retains its first ordinary actor");
    const unsigned phase=std::to_integer<unsigned>(saved[first+101]);
    const auto& a=run.snapshot().entities.front();const float speed=std::hypot(a.velocity.x,a.velocity.z);
    const unsigned slot=phase==1?0U:phase==2&&speed>.001F?1U:phase==3&&speed<.00001F?2U:phase==3&&speed>.001F?3U:4U;
    if(slot<4 && checkpoints[slot].empty())checkpoints[slot]=saved;
  }
  require(std::none_of(checkpoints.begin(),checkpoints.end(),[](const auto& b){return b.empty();}),"actual public Game exposes reverse turn, reverse brake, zero-speed boundary and forward exit");
  auto check_future=[&](const Bytes& saved) {
    game::Game a,b;require(a.restore_state(saved)==game::SaveStateIssue::none && b.restore_state(saved)==game::SaveStateIssue::none,"every committed maneuver phase cold restores");
    require(a.save_state()==saved && b.save_state()==saved,"all committed phase, turn and exit residuals survive the first cold draw byte exactly");same(a,b);
    require(a.snapshot().entities.front().body_heading==b.snapshot().entities.front().body_heading,"cold first draw retains phase's exact physical body axis");
    a.pause();b.pause();const auto paused=a.save_state();a.advance(10,{{1,1},true});b.advance(10);require(a.save_state()==paused && b.save_state()==paused,"paused/background elapsed cannot consume committed turn or exit distance");a.resume();b.resume();
    for(unsigned frame=0;frame<180;++frame){tick(a,frame);tick(b,frame);same(a,b);}
  };
  for(const auto& saved:checkpoints)check_future(saved);
  // Force only the existing attack timer ready, then let the actual director
  // commit a warning while the earned reverse primitive remains in progress.
  auto warning=checkpoints[0];put_float(warning,first+59,0);seal(warning);game::Game planted;
  require(planted.restore_state(warning)==game::SaveStateIssue::none,"mid-turn fixture makes its ordinary attack ready");planted.advance(1.F/60);
  require(planted.snapshot().entities.front().warning_seconds>0,"actual director plants an ordinary warning mid-maneuver");
  warning=planted.save_state();require(std::to_integer<unsigned>(warning[first+101])!=0,"warning preserves the committed escape instead of inventing a new side");check_future(warning);
  auto reject=[&](Bytes bad){seal(bad);game::Game stable;stable.start({77});const auto before=stable.save_state();require(stable.restore_state(bad)==game::SaveStateIssue::malformed && game::inspect_saved_state(bad).issue==game::SaveStateIssue::malformed && stable.save_state()==before,"malformed maneuver rejects transactionally with identical inspection");};
  auto bad=checkpoints[0];bad[first+101]=std::byte{255};reject(bad);
  bad=checkpoints[0];put_float(bad,first+102,std::numeric_limits<float>::quiet_NaN());reject(bad);
  bad=checkpoints[0];put_float(bad,first+106,game::tuning::vehicle_recovery_clearance_metres+1);reject(bad);
  bad=checkpoints[0];bad[first+101]=std::byte{};reject(bad);
  for(unsigned slot:{0U,3U}) {
    bad=checkpoints[slot];const float x=std::bit_cast<float>(get32(bad,first+87)),z=std::bit_cast<float>(get32(bad,first+91));
    const float speed=(slot==0?.25F:-.25F)/std::hypot(x,z);put_float(bad,first+13,x*speed);put_float(bad,first+17,z*speed);reject(bad);
  }
}
void physical_infantry_preparation_continuations() {
  for (const auto role : {game::InfantryRole::rifleman, game::InfantryRole::shotgunner, game::InfantryRole::rocketeer}) {
    game::Game run; run.start({.seed=514});
    const auto starter = run.save_state();
    Bytes bytes(starter.begin(), starter.begin() + static_cast<std::ptrdiff_t>(actor_count_offset));
    append32(bytes, 1);
    Bytes actor(actor_bytes(game::EntityKind::soldier));
    put32(actor, 0, 100); actor[4] = static_cast<std::byte>(game::EntityKind::soldier);
    put_float(actor, 5, 2); put_float(actor, 9, 8);
    put_float(actor, 29, 400); put_float(actor, 33, 400);
    actor[49] = static_cast<std::byte>(role); put_float(actor, 91, 1);
    bytes.insert(bytes.end(), actor.begin(), actor.end());
    const auto old_end = actors_end(starter);
    bytes.insert(bytes.end(), starter.begin() + static_cast<std::ptrdiff_t>(old_end), starter.end());
    put32(bytes, 245, 200); seal(bytes);
    require(run.restore_state(bytes) == game::SaveStateIssue::none,
        "explicit rear-facing infantry wire fixture is legal before any physical preparation");
    const auto first = actor_count_offset + 4;
    std::array<Bytes, 4> checkpoints;
    for (unsigned frame = 0; frame < 240 && std::any_of(checkpoints.begin(), checkpoints.end(), [](const auto& b) { return b.empty(); }); ++frame) {
      if (run.snapshot().phase == game::Phase::choosing_upgrade) require(run.choose_upgrade(0), "preparation fixture resolves a real earned choice");
      run.advance(1.F / 60);
      const auto saved = run.save_state();
      require(!saved.empty(), "every actual preparation, warning and release state remains checkpointable");
      const auto entity = std::find_if(run.snapshot().entities.begin(), run.snapshot().entities.end(),
          [](const auto& a) { return a.id == 100; });
      require(entity != run.snapshot().entities.end(), "high-health codec fixture survives to expose its physical phases");
      unsigned slot = 4;
      if (entity->preparing) slot = entity->aim_pose_seconds && *entity->aim_pose_seconds > .05F ? 1U : 0U;
      else if (entity->warning_seconds > 0) slot = 2;
      else if (entity->fire_recovery_seconds > 0) slot = 3;
      if (slot < 4 && checkpoints[slot].empty()) checkpoints[slot] = saved;
    }
    require(std::none_of(checkpoints.begin(), checkpoints.end(), [](const auto& b) { return b.empty(); }),
        "each equipped role actually exposes entry/mid preparation, locked warning and post-fire recovery");
    for (const auto& saved : checkpoints) {
      game::Game live, cold;
      require(live.restore_state(saved) == game::SaveStateIssue::none && cold.restore_state(saved) == game::SaveStateIssue::none,
          "all actual infantry phase checkpoints cold restore");
      same(live, cold);
      live.pause(); const auto paused = live.save_state();
      require(cold.restore_state(paused) == game::SaveStateIssue::none, "cold first paused draw retains actual infantry phase");
      same(live, cold); live.advance(5, {{1, 1}, true});
      require(live.save_state() == paused, "paused body facing and real preparation pose clock do not advance");
      live.resume(); cold.resume();
      for (unsigned future = 0; future < 180; ++future) {
        for (auto* game : {&live, &cold}) if (game->snapshot().phase == game::Phase::choosing_upgrade)
          require(game->choose_upgrade(0), "continued physical infantry accepts the same earned offer");
        const game::Control control{{std::sin(static_cast<float>(future) * .04F), .2F}, future % 97 == 0};
        live.advance(1.F / 60, control); cold.advance(1.F / 60, control); same(live, cold);
      }
    }
    const auto protected_state = run.save_state();
    const auto reject = [&](Bytes bad) {
      seal(bad);
      require(run.restore_state(bad) == game::SaveStateIssue::malformed &&
          game::inspect_saved_state(bad).issue == game::SaveStateIssue::malformed,
          "malformed infantry facing/preparation metadata rejects before replacing live authority");
      require(run.save_state() == protected_state, "malformed infantry continuation is transactional");
    };
    auto bad = checkpoints[0]; bad[first + 101] = std::byte{2}; reject(bad);
    bad = checkpoints[0]; put_float(bad, first + 102, std::numeric_limits<float>::quiet_NaN()); reject(bad);
    bad = checkpoints[0]; put_float(bad, first + 102, 100); reject(bad);
    bad = checkpoints[0]; put_float(bad, first + 87, 0); put_float(bad, first + 91, 0); reject(bad);
    bad = checkpoints[0]; put_float(bad, first + 50, .1F); reject(bad);
    bad = checkpoints[0]; put_float(bad, first + 13, .1F); reject(bad);
    bad = checkpoints[2]; bad[first + 101] = std::byte{1}; reject(bad);
    bad = checkpoints[2]; put_float(bad, first + 87, 1); put_float(bad, first + 91, 0); reject(bad);
    bad = checkpoints[3]; put_float(bad, first + 102, .1F); reject(bad);
  }
}
void preparation_reservation_union_capacity() {
  game::Game starter; starter.start({.seed=514}); const auto original = starter.save_state();
  const auto make = [&](unsigned count) {
    Bytes bytes(original.begin(), original.begin() + static_cast<std::ptrdiff_t>(actor_count_offset)); append32(bytes, count);
    for (unsigned i = 0; i < count; ++i) {
      Bytes actor(actor_bytes(game::EntityKind::soldier));
      put32(actor, 0, 100 + i); actor[4] = static_cast<std::byte>(game::EntityKind::soldier);
      const game::Vec2 root{-4.5F + 3.F * static_cast<float>(i % 4), 8.F + 4.F * static_cast<float>(i / 4)};
      put_float(actor, 5, root.x); put_float(actor, 9, root.z);
      put_float(actor, 29, 90); put_float(actor, 33, 90); put_float(actor, 55, 2);
      actor[49] = static_cast<std::byte>(game::InfantryRole::rifleman);
      const auto shot = game::infantry_shot(game::InfantryRole::rifleman, root, {});
      const float yaw = std::atan2(shot.direction.x, -shot.direction.z) - game::infantry_attack(game::InfantryRole::rifleman).weapon_yaw_radians;
      put_float(actor, 87, std::sin(yaw)); put_float(actor, 91, -std::cos(yaw));
      if (i < 2) {
        put_float(actor, 41, .4F); put_float(actor, 59, game::infantry_attack(game::InfantryRole::rifleman).cooldown_seconds);
      } else actor[101] = std::byte{1};
      put_float(actor, 102, 1); // A real pose age is not artificially capped at half a second.
      bytes.insert(bytes.end(), actor.begin(), actor.end());
    }
    bytes.insert(bytes.end(), original.begin() + static_cast<std::ptrdiff_t>(actors_end(original)), original.end());
    put32(bytes, 245, 200); seal(bytes); return bytes;
  };
  // Explicit codec-capacity fixture, not a claim that the current two-slot
  // build phase naturally admits eight owners in one frame.
  const auto full = make(game::tuning::warning_active_cap);
  game::Game live, cold;
  require(live.restore_state(full) == game::SaveStateIssue::none && cold.restore_state(full) == game::SaveStateIssue::none,
      "global eight-slot warning/preparation union remains legal independently of the smaller current phase cap");
  require(live.save_state() == full && cold.save_state() == full && live.snapshot().entities.back().aim_pose_seconds == 1.F,
      "cold first draw retains a real preparation age above half a second without inventing an age limit");
  const auto overbooked = make(game::tuning::warning_active_cap + 1);
  require(live.restore_state(overbooked) == game::SaveStateIssue::malformed &&
      game::inspect_saved_state(overbooked).issue == game::SaveStateIssue::malformed && live.save_state() == full,
      "a ninth preparation/warning owner rejects transactionally even with only two actual warnings");
  for (unsigned frame = 0; frame < 120; ++frame) { tick(live, frame); tick(cold, frame); same(live, cold); }
}

} // namespace
int main() {
  phases_and_future(); corruption_and_rollback(); many_natural_checkpoints(); hidden_authority_fixtures(); boss_phase_continuations(); area_damage_retires_owned_beam(); physical_vehicle_continuations(); lifted_vehicle_keeps_facing_and_collected_removal_saves(); committed_vehicle_primitive_phase_continuations(); physical_infantry_preparation_continuations(); preparation_reservation_union_capacity();
  std::cout << "Gameplay continuation codec contracts passed\n";
}
