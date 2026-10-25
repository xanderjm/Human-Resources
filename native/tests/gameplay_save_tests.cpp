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
// HRS1 revision-three fieldwise offsets are intentional corruption-test fixtures.
constexpr std::size_t phase_offset = 36, player_offset = 37, hull_offset = 57;
constexpr std::size_t actor_count_offset = 365, actor_stride = 97;
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
            restored.snapshot().choices[i].evolution_preview == cards[i].evolution_preview, "draft identity rebuilds exact readable text without reroll");
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
  bytes[actor + 95] = std::byte{1}; // revision-three lifted authority, owned by the beam below.
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
  phases_and_future(); corruption_and_rollback(); many_natural_checkpoints(); hidden_authority_fixtures(); area_damage_retires_owned_beam();
  std::cout << "Gameplay continuation codec contracts passed\n";
}
