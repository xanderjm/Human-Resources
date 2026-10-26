#pragma once
#include "human_resources/gameplay/gameplay.h"
#include "human_resources/gameplay/tuning.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace human_resources::gameplay {
namespace detail {
inline constexpr std::size_t module_count = static_cast<std::size_t>(Module::count);
inline constexpr std::size_t research_count = static_cast<std::size_t>(Research::count);
[[nodiscard]] inline float length_squared(Vec2 v) {
  return v.x * v.x + v.z * v.z;
}
[[nodiscard]] inline float distance_squared(Vec2 a, Vec2 b) {
  return length_squared({a.x - b.x, a.z - b.z});
}
[[nodiscard]] inline Vec2 direction(Vec2 v) {
  const float length = std::sqrt(length_squared(v));
  return length > 0.0001F ? Vec2{v.x / length, v.z / length} : Vec2{};
}
[[nodiscard]] inline Vec2 add(Vec2 a, Vec2 b) {
  return {a.x + b.x, a.z + b.z};
}
[[nodiscard]] inline Vec2 scaled(Vec2 a, float b) {
  return {a.x * b, a.z * b};
}
[[nodiscard]] inline bool biological(EntityKind kind) {
  return kind <= EntityKind::soldier;
}
[[nodiscard]] inline bool vehicle(EntityKind kind) {
  return kind == EntityKind::jeep || kind == EntityKind::tank;
}
[[nodiscard]] inline bool military(EntityKind kind) {
  return kind >= EntityKind::soldier;
}
[[nodiscard]] inline std::size_t military_index(EntityKind kind) {
  return static_cast<std::size_t>(kind) - static_cast<std::size_t>(EntityKind::soldier);
}
[[nodiscard]] inline Vec2 infantry_turn(Vec2 forward, Vec2 toward, float dt) {
  forward = direction(forward);
  if (length_squared(forward) == 0) forward = {0, -1};
  if (length_squared(toward) == 0) return forward;
  const float angle = std::atan2(forward.x * toward.z - forward.z * toward.x,
      forward.x * toward.x + forward.z * toward.z);
  const float turn = std::clamp(angle, -tuning::infantry_turn_radians_per_second * dt,
      tuning::infantry_turn_radians_per_second * dt);
  if (std::abs(angle) <= tuning::infantry_turn_radians_per_second * dt) return direction(toward);
  return direction({forward.x * std::cos(turn) - forward.z * std::sin(turn),
      forward.x * std::sin(turn) + forward.z * std::cos(turn)});
}
enum class VehicleEscapePhase : std::uint8_t { none, reverse_turn, reverse_brake, forward_exit };
struct VehicleEscape final {
  VehicleEscapePhase phase{};
  float turn_remaining{}, forward_remaining{};
};
struct InfantryAim final {
  bool preparing{};
  float pose_seconds{}; // Real active time since weapon raise; retained through warning, cleared on fire/cancel.
};
struct Actor final {
  EntityView view;
  float age{}, attack_timer{}, slow_timer{}, focused_hits{}, focus_timer{}, grace{};
  Vec2 aim;
  Vec2 locomotion_velocity; // Passive preferred movement, or military physical forward retaining its facing through stops.
  bool lifted{}, removed{};
  float sweep_progress{}; // Capture progress earned by moving inside the core, capped at qualification.
  VehicleEscape escape;
  InfantryAim infantry_aim;
};
[[nodiscard]] inline bool reserves_threat_slot(const Actor& actor) {
  return !actor.removed && (actor.view.warning_seconds > 0 ||
      (actor.view.kind == EntityKind::soldier && actor.infantry_aim.preparing));
}
struct Projectile final {
  ProjectileView view;
  std::uint32_t source{};
  float age{};
  bool removed{};
};
struct VehicleMotion final {
  Vec2 velocity;
  Vec2 facing;
  VehicleEscape escape;
};
struct InfantryMotion final { Vec2 velocity, facing; };
// Available includes an existing preparation's own reserved slot.
struct InfantryAttackWindow final { bool permitted{}; unsigned available{}; };
struct BossState final {
  std::uint32_t actor_id{};
  BossAttackKind next_kind{BossAttackKind::rake};
  BossPhase phase{BossPhase::approach};
  std::uint16_t remaining_ticks{}, total_ticks{};
  std::uint8_t next_release{};
  bool behind{}, flank_right{};
  float body_heading{};
  std::optional<BossAttackPlan> plan;
};
} // namespace detail
struct Game::State final {
  Snapshot view;
  RunSetup setup;
  std::vector<detail::Actor> actors;
  std::vector<detail::Projectile> bullets;
  std::optional<detail::BossState> boss;
  std::uint64_t random_state{1}, draft_random_state{2}, combat_random_state{3};
  std::uint32_t next_id{1}, streak{}, revive_count{}, picks{};
  std::uint64_t next_presentation_sequence{1};
  double score{}, next_boss{};
  float accumulator{}, biological_budget{}, soldier_budget{}, jeep_budget{}, tank_budget{};
  float laser_clock{}, drone_clock{}, pulse_clock{}, mirrorball_clock{}, reflected_damage{};
  float time_since_hit{}, capture_burst{}, vehicle_burst{}, movement_burst{}, drone_burst{};
  float vehicle_capture_cooldown{}, disable_cooldown{}, intercept_cooldown{};
  float capture_shield_budget{}, shield_budget_clock{};
  Vec2 last_direction{0, 1};
  bool pending_dash{};

  [[nodiscard]] std::uint32_t random();
  [[nodiscard]] float random_unit();
  [[nodiscard]] float combat_unit();
  [[nodiscard]] float draft_unit();
  [[nodiscard]] float random_range(float minimum, float maximum);
  [[nodiscard]] unsigned rank(Module module) const;
  [[nodiscard]] float research(Research research_kind) const;
  [[nodiscard]] bool evolved(Evolution evolution) const;
  [[nodiscard]] float permanent(std::size_t track) const;
  [[nodiscard]] float damage_multiplier() const;
  [[nodiscard]] float fire_multiplier() const;
  [[nodiscard]] float capture_seconds(const detail::Actor& actor) const;
  [[nodiscard]] unsigned used_beam_slots() const;
  [[nodiscard]] detail::Actor* actor(std::uint32_t id);
  [[nodiscard]] bool capture_eligible(const detail::Actor& actor) const;
  [[nodiscard]] bool sweep_eligible(const detail::Actor& actor) const;
  void sync();
  void present(PresentationEventKind kind, Vec2 position, std::uint32_t actor_id = 0, float radius = 0, DamageCause cause = DamageCause::unknown);
  void step(float dt, Control control);
  void move(float dt, Control control);
  void advance_timers(float dt);
  void director(float dt);
  void spawn_cohort(bool initial = false, unsigned opening_group = 0);
  void spawn(EntityKind kind, bool initial = false, std::optional<Vec2> pocket_position = std::nullopt);
  [[nodiscard]] bool place_biological(detail::Actor& target, float lateral_sample, float forward_sample, bool initial);
  void move_biological(float dt);
  [[nodiscard]] Vec2 military_velocity(const detail::Actor& actor, Vec2 preferred, float dt, Vec2 carry = {}) const;
  [[nodiscard]] detail::VehicleMotion vehicle_motion(const detail::Actor& actor, Vec2 destination, float dt) const;
  [[nodiscard]] detail::InfantryMotion infantry_motion(const detail::Actor& actor, Vec2 destination, float dt) const;
  void infantry_update(detail::Actor& actor, float dt, detail::InfantryAttackWindow window);
  void place_military(detail::Actor& actor);
  void enemies(float dt);
  void shoot(detail::Actor& enemy);
  void boss_update(detail::Actor& actor, float dt);
  [[nodiscard]] bool boss_preparing() const;
  [[nodiscard]] bool boss_leading() const;
  [[nodiscard]] bool valid_boss() const;
  void boss_release(detail::Actor& actor);
  void capture(float dt);
  void captured(detail::Actor& target);
  void combat(float dt);
  void laser_fire();
  void secondary_hits(detail::Actor& target, float damage);
  void drone_fire();
  void pulse();
  void projectiles(float dt);
  void hurt(float damage, DamageCause cause = DamageCause::unknown, std::uint32_t source_id = 0);
  void hit(detail::Actor& enemy, float damage);
  void defeated(detail::Actor& enemy);
  void draw_choices();
  [[nodiscard]] bool module_eligible(std::size_t index) const;
  [[nodiscard]] bool research_eligible(std::size_t index) const;
  [[nodiscard]] UpgradeChoice make_choice(bool hardware, std::size_t index, std::size_t rarity) const;
  void apply(const UpgradeChoice& choice);
  void evolve();
  void check_level();
};
} // namespace human_resources::gameplay
