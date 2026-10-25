#include "run_state.h"
#include <bit>
#include <type_traits>

namespace human_resources::gameplay {
namespace {
constexpr std::uint32_t magic = 0x31535248U; // HRS1, explicit little-endian bytes.
constexpr std::uint32_t schema = 1;
// Increment when authority semantics/field meaning change independently of tuning.
constexpr std::uint32_t authority_revision = 3;
constexpr std::size_t maximum_bytes = 48 * 1024;
constexpr std::uint32_t maximum_actors = 128, maximum_projectiles = 96;

std::uint64_t digest(std::span<const std::byte> bytes) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const auto byte : bytes) {
    hash = (hash ^ std::to_integer<std::uint8_t>(byte)) * 1099511628211ULL;
  }
  return hash;
}
struct Writer final {
  std::vector<std::byte> bytes;
  template <class T> requires std::is_unsigned_v<T>
  void operator()(T& value) {
    for (std::size_t i = 0; i < sizeof(T); ++i)
      bytes.push_back(static_cast<std::byte>((value >> (i * 8)) & 0xffU));
  }
  template <class T> requires std::is_enum_v<T>
  void operator()(T& value) { auto raw = static_cast<std::underlying_type_t<T>>(value); (*this)(raw); }
  void operator()(bool& value) { auto raw = static_cast<std::uint8_t>(value); (*this)(raw); }
  void operator()(float& value) { auto raw = std::bit_cast<std::uint32_t>(value); (*this)(raw); }
  void operator()(double& value) { auto raw = std::bit_cast<std::uint64_t>(value); (*this)(raw); }
};
struct Reader final {
  std::span<const std::byte> bytes;
  std::size_t position{};
  bool ok{true};
  template <class T> requires std::is_unsigned_v<T>
  void operator()(T& value) {
    if (position > bytes.size() || bytes.size() - position < sizeof(T)) { ok = false; return; }
    value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i)
      value = static_cast<T>(value | static_cast<T>(static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[position++])) << (i * 8)));
  }
  template <class T> requires std::is_enum_v<T>
  void operator()(T& value) { std::underlying_type_t<T> raw{}; (*this)(raw); value = static_cast<T>(raw); }
  void operator()(bool& value) { std::uint8_t raw{}; (*this)(raw); if (raw > 1) ok = false; value = raw != 0; }
  void operator()(float& value) {
    std::uint32_t raw{}; (*this)(raw); value = std::bit_cast<float>(raw);
    if (!std::isfinite(value) || std::fabs(value) > 1.0e12F) ok = false;
  }
  void operator()(double& value) {
    std::uint64_t raw{}; (*this)(raw); value = std::bit_cast<double>(raw);
    if (!std::isfinite(value)) ok = false;
  }
};
template <class IO> void vector2(IO& io, Vec2& v) { io(v.x); io(v.z); }
template <class IO> void statistics(IO& io, RunStats& s) {
  io(s.active_seconds); io(s.score); io(s.captures); io(s.soldier_kills); io(s.jeep_kills);
  io(s.tank_kills); io(s.boss_kills); io(s.vehicle_captures); io(s.level); io(s.evolutions);
}
template <class IO> void actor_fields(IO& io, detail::Actor& a) {
  auto& v = a.view;
  io(v.id); io(v.kind); vector2(io, v.position); vector2(io, v.velocity); vector2(io, v.warning_aim);
  io(v.hull); io(v.maximum_hull); io(v.capture_progress); io(v.warning_seconds); io(v.disabled_seconds);
  io(v.infantry_role); io(v.fire_recovery_seconds); io(v.minigun_barrel);
  io(a.age); io(a.attack_timer); io(a.slow_timer); io(a.focused_hits); io(a.focus_timer); io(a.grace);
  vector2(io, a.aim); vector2(io, a.wander_velocity); io(a.lifted); io(a.removed);
}
template <class IO> void projectile_fields(IO& io, detail::Projectile& p) {
  io(p.view.id); vector2(io, p.view.position); vector2(io, p.view.velocity);
  io(p.view.radius); io(p.view.damage); io(p.source); io(p.age); io(p.removed);
  io(p.view.infantry_role); vector2(io, p.view.launch_position); io(p.view.launch_height); io(p.view.rise_per_second); io(p.view.kind); io(p.view.muzzle_index); vector2(io,p.view.launch_direction);
}
} // namespace
namespace detail {
struct SaveStateCodec final {
  template <class IO> static void authority(IO& io, Game::State& s) {
    auto& v = s.view;
    io(s.setup.seed);
    for (auto& rank : s.setup.permanent_ranks) io(rank);
    io(s.setup.unlocked_blueprints);
    io(v.phase); vector2(io, v.player); vector2(io, v.velocity); io(v.runner_distance);
    io(v.hull); io(v.shield); io(v.xp); io(v.movement_speed);
    io(v.dash_cooldown_seconds); io(v.invulnerability_seconds); io(v.rerolls_remaining);
    for (auto& rank : v.modules) io(rank);
    for (auto& research : v.research) io(research);
    io(v.evolution_bits); io(v.director_spawn_attempts); io(v.director_schedule_hash); statistics(io, v.stats);
    io(s.random_state); io(s.draft_random_state); io(s.combat_random_state);
    io(s.next_id); io(s.streak); io(s.revive_count); io(s.picks); io(s.score); io(s.next_boss);
    io(s.accumulator); io(s.biological_budget); io(s.soldier_budget); io(s.jeep_budget); io(s.tank_budget);
    io(s.laser_clock); io(s.drone_clock); io(s.pulse_clock); io(s.mirrorball_clock); io(s.reflected_damage);
    io(s.time_since_hit); io(s.capture_burst); io(s.vehicle_burst); io(s.movement_burst); io(s.drone_burst);
    io(s.vehicle_capture_cooldown); io(s.disable_cooldown); io(s.intercept_cooldown);
    io(s.capture_shield_budget); io(s.shield_budget_clock); vector2(io, s.last_direction);
  }
  template <class IO> static bool populations(IO& io, Game::State& s) {
    auto actors = static_cast<std::uint32_t>(s.actors.size()); io(actors);
    if (actors > maximum_actors) return false;
    if constexpr (std::is_same_v<IO, Reader>) { if (!io.ok) return false; s.actors.resize(actors); }
    for (auto& a : s.actors) actor_fields(io, a);
    auto bullets = static_cast<std::uint32_t>(s.bullets.size()); io(bullets);
    if (bullets > std::min(maximum_projectiles, tuning::projectile_active_cap)) return false;
    if constexpr (std::is_same_v<IO, Reader>) { if (!io.ok) return false; s.bullets.resize(bullets); }
    for (auto& p : s.bullets) projectile_fields(io, p);
    auto beams = static_cast<std::uint32_t>(s.view.beams.size()); io(beams);
    if (beams > tuning::evolution_beams) return false;
    if constexpr (std::is_same_v<IO, Reader>) { if (!io.ok) return false; s.view.beams.resize(beams); }
    for (auto& beam : s.view.beams) {
      io(beam.target_id); vector2(io, beam.target); io(beam.progress); io(beam.vehicle);
    }
    if (s.view.phase == Phase::choosing_upgrade) {
      for (auto& choice : s.view.choices) {
        io(choice.hardware); io(choice.module); io(choice.research); io(choice.rarity);
      }
    }
    return true;
  }
  static bool valid(Game::State& s) {
    auto& v = s.view;
    if (v.phase > Phase::dead || s.next_id == 0 || s.random_state == 0 ||
        v.stats.active_seconds < 0 || v.stats.active_seconds > 1.0e9 ||
        s.score < 0 || s.score >= 18446744073709551616.0 ||
        s.next_boss < 0 || s.next_boss > 1.0e12 || v.stats.level == 0 || v.stats.level > 10000000 ||
        s.picks > v.stats.level - 1 || s.revive_count > tuning::revive_maximum ||
        v.rerolls_remaining > tuning::rerolls || (s.setup.unlocked_blueprints >> module_count) != 0 ||
        v.hull < 0 || v.shield < 0 || v.xp < 0 || v.runner_distance < 0 ||
        v.movement_speed < 0 || v.dash_cooldown_seconds < 0 || v.invulnerability_seconds < 0 ||
        s.accumulator < 0 || s.accumulator > tuning::maximum_catchup_seconds ||
        s.biological_budget < 0 || s.soldier_budget < 0 || s.jeep_budget < 0 || s.tank_budget < 0 ||
        s.reflected_damage < 0 || s.time_since_hit < 0 || s.capture_burst < 0 || s.vehicle_burst < 0 ||
        s.movement_burst < 0 || s.drone_burst < 0 || s.vehicle_capture_cooldown < 0 || s.disable_cooldown < 0 ||
        s.intercept_cooldown < 0 || s.capture_shield_budget < 0 || s.shield_budget_clock < 0 || s.shield_budget_clock > 1.001F)
      return false;
    for (std::size_t i = 0; i < s.setup.permanent_ranks.size(); ++i)
      if (s.setup.permanent_ranks[i] > tuning::permanent_maximum_ranks[i]) return false;
    unsigned installed = 0;
    for (std::size_t i = 0; i < v.modules.size(); ++i) {
      if (v.modules[i] > 3 || (v.modules[i] && (s.setup.unlocked_blueprints & (1U << i)) == 0)) return false;
      if (v.modules[i]) {
        ++installed;
        for (std::size_t j = 0; j < v.modules.size(); ++j)
          if (v.modules[j] < tuning::prerequisites[i][j]) return false;
      }
    }
    if (installed > tuning::hardware_slots || (v.evolution_bits >> tuning::evolution_ingredients.size()) != 0 ||
        std::popcount(v.evolution_bits) > static_cast<int>(tuning::maximum_evolutions) ||
        v.stats.evolutions != static_cast<std::uint32_t>(std::popcount(v.evolution_bits))) return false;
    for (std::size_t i = 0; i < tuning::evolution_ingredients.size(); ++i) {
      if ((v.evolution_bits & (1U << i)) != 0)
        for (const auto ingredient : tuning::evolution_ingredients[i]) if (v.modules[ingredient] != 3) return false;
    }
    for (std::size_t i = 0; i < v.research.size(); ++i)
      if (v.research[i] < 0 || (tuning::research_caps[i] > 0 && v.research[i] > tuning::research_caps[i])) return false;
    // Do not sync untrusted score/time until conversion bounds have been checked.
    if (v.stats.score != static_cast<std::uint64_t>(s.score)) return false;
    s.sync();
    if (v.hull > v.maximum_hull || v.shield > v.maximum_shield ||
        (v.phase == Phase::dead && v.hull != 0) ||
        (v.phase != Phase::ready && v.phase != Phase::dead && v.hull <= 0)) return false;
    std::array<unsigned, 5> counts{};
    unsigned warnings = 0;
    std::vector<std::uint32_t> ids;
    ids.reserve(s.actors.size() + s.bullets.size());
    auto unique_id = [&](std::uint32_t id) {
      if (id == 0 || id >= s.next_id || std::find(ids.begin(), ids.end(), id) != ids.end()) return false;
      ids.push_back(id); return true;
    };
    for (const auto& a : s.actors) {
      if (!unique_id(a.view.id) || a.view.kind > EntityKind::boss || a.removed ||
          a.view.hull <= 0 || a.view.maximum_hull <= 0 || a.view.hull > a.view.maximum_hull ||
          a.view.capture_progress < 0 || a.view.capture_progress >= 1 || a.view.warning_seconds < 0 ||
          a.view.disabled_seconds < 0 || a.age < 0 || a.slow_timer < 0 || a.focused_hits < 0 ||
          a.focused_hits > tuning::focus_hits || a.focus_timer < 0 || a.grace < 0) return false;
      const auto role = a.view.infantry_role;
      if (role > InfantryRole::rocketeer ||
          (a.view.kind == EntityKind::soldier ? role == InfantryRole::none : role != InfantryRole::none) ||
          a.view.fire_recovery_seconds < 0 ||
          a.view.fire_recovery_seconds > (vehicle(a.view.kind) ? tuning::capture_return_seconds : infantry_attack(role).recovery_seconds) + .0001F ||
          (a.view.kind == EntityKind::jeep ? a.view.minigun_barrel >= 6 : a.view.minigun_barrel != 0))
        return false;
      ++counts[a.view.kind < EntityKind::soldier ? 0 : military_index(a.view.kind) + 1];
      if (a.view.warning_seconds > 0) ++warnings;
    }
    if (counts[0] > tuning::biological_active_cap || warnings > tuning::warning_active_cap) return false;
    for (std::size_t i = 1; i < counts.size(); ++i)
      if (static_cast<float>(counts[i]) > tuning::enemy_active_cap[i - 1]) return false;
    for (const auto& p : s.bullets) {
      // A projectile may outlive its firing actor, but its source must precede
      // the projectile in the shared monotonic identity space.
      if (!unique_id(p.view.id) || p.source == 0 || p.source >= p.view.id || p.removed ||
          p.view.radius <= 0 || p.view.damage <= 0 || p.age < 0 || p.age > tuning::projectile_lifetime_seconds ||
          p.view.infantry_role > InfantryRole::rocketeer || p.view.launch_height < 0 || p.view.launch_height > 10 ||
          p.view.rise_per_second < 0 || p.view.rise_per_second > 1024 || p.view.kind > EnemyProjectileKind::boss_missile ||
          (p.view.kind == EnemyProjectileKind::jeep_minigun ? (p.view.muzzle_index >= 6 || p.view.infantry_role != InfantryRole::none) : p.view.muzzle_index != 0) ||
          (p.view.kind == EnemyProjectileKind::infantry_round ? (p.view.infantry_role != InfantryRole::rifleman && p.view.infantry_role != InfantryRole::shotgunner) :
           p.view.kind == EnemyProjectileKind::infantry_rocket ? p.view.infantry_role != InfantryRole::rocketeer : p.view.infantry_role != InfantryRole::none))
        return false;
      if (std::abs(length_squared(p.view.launch_direction)-1.F)>.001F) return false;
      if (const auto* source = s.actor(p.source); source && (!military(source->view.kind) ||
          (source->view.kind == EntityKind::jeep) != (p.view.kind == EnemyProjectileKind::jeep_minigun) ||
          (source->view.kind == EntityKind::boss) != (p.view.kind == EnemyProjectileKind::boss_missile))) return false;
      if (const auto* source = s.actor(p.source); source && source->view.infantry_role != p.view.infantry_role) return false;
      if (std::any_of(s.bullets.begin(), s.bullets.end(), [&](const Projectile& other) { return other.view.id == p.source; })) return false;
    }
    std::vector<std::uint32_t> targets;
    unsigned slots = 0;
    for (const auto& beam : v.beams) {
      const auto* target = s.actor(beam.target_id);
      if (!target || target->view.kind == EntityKind::boss || beam.vehicle != vehicle(target->view.kind) ||
          beam.progress < 0 || beam.progress > 1 ||
          std::find(targets.begin(), targets.end(), beam.target_id) != targets.end()) return false;
      targets.push_back(beam.target_id); slots += beam.vehicle ? 2U : 1U;
    }
    if (slots > v.beam_capacity) return false;
    for (const auto& a : s.actors)
      if (a.lifted && std::find(targets.begin(), targets.end(), a.view.id) == targets.end()) return false;
    if (v.phase == Phase::choosing_upgrade) {
      std::array<bool, module_count> modules{};
      std::array<bool, research_count> research{};
      for (auto& choice : v.choices) {
        const auto module = static_cast<std::size_t>(choice.module);
        const auto study = static_cast<std::size_t>(choice.research);
        const auto rarity = static_cast<std::size_t>(choice.rarity);
        if (module >= module_count || study >= research_count || rarity >= 3) return false;
        if (choice.hardware) {
          if (modules[module] || !s.module_eligible(module) || rarity != tuning::module_rarity[module]) return false;
          modules[module] = true;
        } else {
          if (research[study] || !s.research_eligible(study)) return false;
          research[study] = true;
        }
        choice = s.make_choice(choice.hardware, choice.hardware ? module : study, rarity);
      }
    }
    s.pending_dash = false;
    s.next_presentation_sequence = 1;
    v.shots.clear(); v.presentation_events = {};
    return true;
  }
  static SaveStateIssue decode(std::span<const std::byte> bytes, Game::State& candidate) {
    if (bytes.size() < 28 || bytes.size() > maximum_bytes) return SaveStateIssue::malformed;
    Reader reader{bytes};
    std::uint32_t tag{}, version{}, revision{}; std::uint64_t fingerprint{};
    reader(tag); reader(version);
    if (tag != magic) return SaveStateIssue::malformed;
    if (version != schema) return SaveStateIssue::unsupported_schema;
    reader(revision); reader(fingerprint);
    if (revision != authority_revision || fingerprint != tuning::save_rules_fingerprint) return SaveStateIssue::incompatible_rules;
    Reader trailer{bytes.last(8)}; std::uint64_t expected{}; trailer(expected);
    if (expected != digest(bytes.first(bytes.size() - 8))) return SaveStateIssue::malformed;
    reader.bytes = bytes.first(bytes.size() - 8);
    authority(reader, candidate);
    if (!reader.ok || !populations(reader, candidate) || !reader.ok || reader.position != reader.bytes.size() || !valid(candidate))
      return SaveStateIssue::malformed;
    return SaveStateIssue::none;
  }
  static SavedStateInfo inspect(std::span<const std::byte> bytes) {
    Game::State candidate;
    const auto issue = decode(bytes, candidate);
    if (issue != SaveStateIssue::none) return {issue, Phase::ready, {}, {}, 0};
    return {issue, candidate.view.phase, candidate.view.stats, candidate.setup, candidate.revive_count};
  }
  static std::vector<std::byte> encode(const Game::State& source) {
    // Work on an isolated candidate so text rebuilding and validation cannot
    // mutate simulation, consume RNG, or emit presentation outcomes.
    auto candidate = source;
    if (!valid(candidate)) return {};
    Writer writer;
    writer.bytes.reserve(1024 + candidate.actors.size() * 113 + candidate.bullets.size() * 62);
    auto tag = magic, version = schema, revision = authority_revision;
    auto fingerprint = tuning::save_rules_fingerprint;
    writer(tag); writer(version); writer(revision); writer(fingerprint);
    authority(writer, candidate);
    if (!populations(writer, candidate) || writer.bytes.size() + 8 > maximum_bytes) return {};
    auto hash = digest(writer.bytes); writer(hash);
    return std::move(writer.bytes);
  }
};
} // namespace detail
std::vector<std::byte> Game::save_state() const { return detail::SaveStateCodec::encode(*state_); }
SaveStateIssue Game::restore_state(std::span<const std::byte> bytes) {
  auto candidate = std::make_unique<State>();
  const auto issue = detail::SaveStateCodec::decode(bytes, *candidate);
  if (issue == SaveStateIssue::none) state_ = std::move(candidate);
  return issue;
}
SavedStateInfo inspect_saved_state(std::span<const std::byte> bytes) { return detail::SaveStateCodec::inspect(bytes); }
} // namespace human_resources::gameplay
