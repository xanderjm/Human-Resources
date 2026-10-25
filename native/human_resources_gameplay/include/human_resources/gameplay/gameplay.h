#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace human_resources::gameplay {

struct Vec2 final {
  float x{}, z{};
};
// Bit order is stable and independent of catalogue display order.
enum class Module : std::uint8_t {
  laser_array,
  rail_lens,
  arc_coil,
  orbit_drones,
  emp_ring,
  beam_splitter,
  tractor_lens,
  heavy_winch,
  overtime_reactor,
  reserve_battery,
  shield_generator,
  repair_swarm,
  mirror_plating,
  vector_thrusters,
  phase_drive,
  bounty_scanner,
  count
};
enum class Research : std::uint8_t {
  weapon_damage,
  hull_reinforcement,
  shield_capacity,
  specimen_value,
  capture_speed,
  fire_rate,
  emergency_repairs,
  count
};
enum class Phase : std::uint8_t { ready,
  active,
  paused,
  choosing_upgrade,
  dead };
enum class EntityKind : std::uint8_t { farmer,
  cow,
  sheep,
  pig,
  soldier,
  jeep,
  tank,
  boss };
enum class InfantryRole : std::uint8_t { none, rifleman, shotgunner, rocketeer };
enum class EnemyProjectileKind : std::uint8_t { vehicle_missile, infantry_round, infantry_rocket, jeep_minigun, boss_missile };
struct InfantryAttack final {
  unsigned pellets{};
  float damage_fraction{}, spread_radians{}, projectile_speed{}, warning_seconds{}, cooldown_seconds{}, recovery_seconds{};
  std::array<float, 3> neutral_muzzle{}; // Source actor-local metres, +Y up/+Z forward.
  float weapon_yaw_radians{};
};
struct InfantryShot final {
  Vec2 origin, direction;
  float height{};
  bool valid{};
};
[[nodiscard]] InfantryAttack infantry_attack(InfantryRole role);
// Neutral source muzzle aligned to the fixed aim. Invalid near-body geometry
// produces no shot; this never retargets a committed warning to the live player.
[[nodiscard]] InfantryShot infantry_shot(InfantryRole role, Vec2 actor_position, Vec2 committed_aim);
[[nodiscard]] float capture_return_seconds();
enum class Rarity : std::uint8_t { common,
  uncommon,
  rare };
enum class Evolution : std::uint8_t {
  great_vacuum,
  office_shredder,
  hostile_acquisition,
  mirrorball,
  employee_benefits,
  storm_drain
};
[[nodiscard]] constexpr std::uint32_t blueprint_bit(const Module module) {
  return std::uint32_t{1} << static_cast<unsigned>(module);
}
[[nodiscard]] std::string_view module_id(Module module);
[[nodiscard]] std::uint32_t starting_blueprints();

struct RunSetup final {
  std::uint64_t seed{1};
  // damage, hull, shield, biological capture; clamped to configured limits.
  std::array<std::uint8_t, 4> permanent_ranks{};
  std::uint32_t unlocked_blueprints{}; // zero selects the ten starter blueprints
};
struct Control final {
  Vec2 movement; // normalized by gameplay; x right, z forward, metres world axes
  bool dash{};   // rising semantic press, not a held button
};
struct RunStats final {
  double active_seconds{};
  std::uint64_t score{}, captures{}, soldier_kills{}, jeep_kills{}, tank_kills{}, boss_kills{};
  std::uint64_t vehicle_captures{};
  std::uint32_t level{1}, evolutions{};
};
enum class SaveStateIssue : std::uint8_t { none, malformed, unsupported_schema, incompatible_rules };
struct SavedStateInfo final {
  SaveStateIssue issue{SaveStateIssue::malformed};
  Phase phase{Phase::ready};
  RunStats stats;
  RunSetup setup;
  std::uint32_t revive_count{};
  [[nodiscard]] bool valid() const noexcept { return issue == SaveStateIssue::none; }
};
[[nodiscard]] SavedStateInfo inspect_saved_state(std::span<const std::byte> bytes);
namespace detail { struct SaveStateCodec; }
struct EntityView final {
  std::uint32_t id{};
  EntityKind kind{};
  Vec2 position, velocity, warning_aim;
  float hull{}, maximum_hull{}, capture_progress{}, warning_seconds{}, disabled_seconds{};
  InfantryRole infantry_role{InfantryRole::none};
  float fire_recovery_seconds{};
  std::uint8_t minigun_barrel{}; // Jeep's committed/last barrel, advanced only on accepted warning.
  std::optional<float> body_heading; // Actual vehicle orientation; absent in legacy/synthetic presentation values.
};
struct MinigunGeometry final {
  std::array<float,3> anchor, yaw_pivot, pitch_offset;
  std::array<std::array<float,3>,6> muzzles;
  float spawn_forward_offset{}, maximum_elevation_radians{}, minimum_range{};
};
struct MinigunShot final {
  Vec2 origin, direction;
  float height{}, yaw_radians{}, elevation_radians{};
  std::uint8_t barrel{};
  bool valid{};
};
[[nodiscard]] MinigunGeometry minigun_geometry();
[[nodiscard]] float vehicle_body_yaw(const EntityView& actor);
// Level authoritative actor frame, exact selected source opening and fixed aim.
// Out-of-envelope/close targets reject; this never clamps a false shot ray.
[[nodiscard]] MinigunShot jeep_minigun_shot(const EntityView& actor, Vec2 aim, std::uint8_t barrel);
struct CannonGeometry final {
  std::array<float,3> socket, muzzle;
  float visible_missile_scale{}, launch_forward_offset{};
  unsigned boss_projectiles{};
  float boss_spread_radians{};
};
struct CannonShot final {
  Vec2 origin, direction;
  float height{}, yaw_radians{};
  bool valid{};
};
[[nodiscard]] CannonGeometry cannon_geometry();
[[nodiscard]] CannonShot vehicle_cannon_shot(const EntityView& actor, Vec2 aim);
struct ProjectileView final {
  std::uint32_t id{};
  Vec2 position, velocity;
  float radius{}, damage{};
  std::uint32_t source_id{};
  InfantryRole infantry_role{InfantryRole::none};
  Vec2 launch_position;
  float age_seconds{}, launch_height{}, rise_per_second{};
  EnemyProjectileKind kind{EnemyProjectileKind::vehicle_missile};
  std::uint8_t muzzle_index{};
  Vec2 launch_direction; // Physical source axis, retained for a bounded boss-fan departure bridge.
};
struct BeamView final {
  std::uint32_t target_id{};
  Vec2 target;
  float progress{};
  bool vehicle{};
};
enum class ShotKind : std::uint8_t { laser, rail, arc, drone, emp, reflection, mirrorball };
struct ShotView final {
  Vec2 from, to;
  float remaining_seconds{};
  bool secondary{};
  ShotKind kind{};
  std::uint8_t emitter_index{};
};
enum class PresentationEventKind : std::uint8_t { capture,
  impact,
  defeat,
  shield_hit,
  hull_hit,
  laser_volley,
  warning_lock,
  emp_pulse,
  reflect,
  intercept,
  mirrorball,
  phase_dash,
  repair,
  enemy_shot,
  sweep_capture,
  harvest_overdrive };
// Presentation-only provenance supplied by the actual successful damage path.
enum class DamageCause : std::uint8_t { unknown, contact, projectile };
struct PresentationEvent final {
  std::uint64_t sequence{};
  double active_seconds{};
  PresentationEventKind kind{};
  std::uint32_t actor_id{};
  Vec2 position;
  float radius{}; // Only spatial area events; zero for point outcomes.
  DamageCause damage_cause{}; // Transient; never serialized or inferred from proximity.
};
inline constexpr std::size_t presentation_event_capacity = 64;
struct UpgradeChoice final {
  bool hardware{};
  Module module{};
  Research research{};
  Rarity rarity{};
  std::uint32_t next_rank{};
  float gain{}, before{}, after{};
  std::string title, description;
  std::string evolution_preview;
};
struct Snapshot final {
  Phase phase{Phase::ready};
  Vec2 player, velocity;
  float runner_distance{}; // monotonic world-camera station; player dodges within its band
  float forward_speed{}; // Derived from earned level/captures; not an independent saved clock.
  float hull{}, maximum_hull{}, shield{}, maximum_shield{};
  float xp{}, next_level_xp{}, capture_radius{}, movement_speed{}, weapon_damage{};
  float dash_cooldown_seconds{}, invulnerability_seconds{};
  std::uint32_t beam_capacity{}, laser_count{}, rerolls_remaining{}, sector{1};
  std::array<std::uint8_t, static_cast<std::size_t>(Module::count)> modules{};
  std::array<float, static_cast<std::size_t>(Research::count)> research{};
  std::uint32_t evolution_bits{};
  float drone_burst_seconds{}, capture_burst_seconds{}, movement_burst_seconds{};
  // Actual close moving captures build a temporary active-time harvest chain.
  std::uint8_t harvest_chain{};
  float harvest_chain_seconds{}, harvest_overdrive_seconds{};
  std::uint64_t director_spawn_attempts{}, director_schedule_hash{};
  RunStats stats;
  std::vector<EntityView> entities;
  std::vector<ProjectileView> projectiles;
  std::vector<BeamView> beams;
  std::vector<ShotView> shots;
  // Bounded recent authoritative outcomes, overwritten oldest-first. Sequence
  // resets with start(); consumers track it once and never infer a hit/kill
  // from geometry or retirement. Events cannot affect simulation decisions.
  std::array<PresentationEvent, presentation_event_capacity> presentation_events{};
  std::array<UpgradeChoice, 3> choices;
};
// Single-thread owner. No SDL, I/O, wall-clock or render dependency. All spatial
// units are metres/seconds. advance clamps catch-up and freezes every run clock
// outside active play; callers must discard platform elapsed time after resume.
class Game final {
public:
  Game();
  ~Game();
  Game(Game&&) noexcept;
  Game& operator=(Game&&) noexcept;
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;
  void start(const RunSetup& setup = {});
  void advance(float elapsed_seconds, Control control = {});
  void pause();
  void resume();
  [[nodiscard]] bool choose_upgrade(std::size_t index);
  [[nodiscard]] bool reroll();
  [[nodiscard]] bool revive(); // caller grants only after a completed local/provider reward
  [[nodiscard]] bool revive_available() const;
  [[nodiscard]] const Snapshot& snapshot() const;
  [[nodiscard]] std::vector<std::byte> save_state() const;
  // Replaces authority only after complete validation; preserves the saved phase.
  [[nodiscard]] SaveStateIssue restore_state(std::span<const std::byte> bytes);

private:
  friend struct detail::SaveStateCodec;
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace human_resources::gameplay
