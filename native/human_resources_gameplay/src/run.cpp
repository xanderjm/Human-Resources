#include "run_state.h"

namespace human_resources::gameplay {
using namespace detail;
namespace {
float forward_speed(const Snapshot& view) {
  // Active time accelerates the opening; successful harvesting adds momentum.
  // Existing saved counters reconstruct the curve without a new saved clock.
  const float opening = static_cast<float>(std::clamp(
      view.stats.active_seconds / tuning::scroll_ramp_seconds, 0.0, 1.0)) *
      tuning::scroll_time_bonus_metres_per_second;
  return tuning::forward_scroll_metres_per_second + std::min(
      tuning::scroll_maximum_metres_per_second - tuning::forward_scroll_metres_per_second,
      opening + static_cast<float>(view.stats.captures) * tuning::scroll_per_capture_metres_per_second +
          static_cast<float>(view.stats.level - 1) * tuning::scroll_per_level_metres_per_second);
}
void update_forward_speed(Snapshot& view) {
  const float speed = forward_speed(view);
  // Scroll is camera carry, not steering: changes must not accelerate the pilot.
  // A zero derived speed marks initialization/decoding, where velocity already
  // has its initial/saved meaning and sync only reconstructs the missing speed.
  if (view.forward_speed > 0) view.velocity.z += speed - view.forward_speed;
  view.forward_speed = speed;
}
} // namespace

std::string_view module_id(Module module) {
  const auto index = static_cast<std::size_t>(module);
  return index < tuning::module_ids.size() ? tuning::module_ids[index] : std::string_view{};
}
std::uint32_t starting_blueprints() {
  return tuning::starter_blueprints;
}
Game::Game() : state_(std::make_unique<State>()) {
  state_->sync();
}
Game::~Game() = default;
Game::Game(Game&&) noexcept = default;
Game& Game::operator=(Game&&) noexcept = default;
const Snapshot& Game::snapshot() const {
  return state_->view;
}
void Game::State::present(PresentationEventKind kind, Vec2 position, std::uint32_t actor_id, float radius, DamageCause cause) {
  const auto sequence = next_presentation_sequence++;
  view.presentation_events[(sequence - 1) % presentation_event_capacity] = {sequence, view.stats.active_seconds, kind, actor_id, position, radius, cause};
}
void Game::start(const RunSetup& setup) {
  *state_ = State{};
  auto& s = *state_;
  s.setup = setup;
  s.setup.unlocked_blueprints = (s.setup.unlocked_blueprints | starting_blueprints()) & tuning::available_blueprints;
  for (std::size_t i = 0; i < s.setup.permanent_ranks.size(); ++i) {
    s.setup.permanent_ranks[i] = std::min(setup.permanent_ranks[i], tuning::permanent_maximum_ranks[i]);
  }
  s.random_state = setup.seed ? setup.seed : 1;
  s.draft_random_state = s.random_state ^ 0x9e3779b97f4a7c15ULL;
  s.combat_random_state = s.random_state ^ 0xd1b54a32d192ed03ULL;
  s.actors.reserve(64);
  s.bullets.reserve(tuning::projectile_active_cap);
  s.view.entities.reserve(64);
  s.view.projectiles.reserve(tuning::projectile_active_cap);
  s.view.beams.reserve(tuning::evolution_beams);
  s.view.shots.reserve(64);
  s.view.phase = Phase::active;
  s.view.rerolls_remaining = tuning::rerolls;
  s.view.hull = tuning::base_hull + s.permanent(1);
  s.view.shield = tuning::base_shield + s.permanent(2);
  s.next_boss = tuning::director_boss_first_second;
  s.time_since_hit = tuning::base_shield_recharge_delay_seconds;
  for (unsigned i = 0; i < tuning::initial_biological_count / tuning::encounter_cohort_size; ++i) {
    s.spawn_cohort(true, i);
  }
  s.sync();
  s.view.velocity.z = s.view.forward_speed;
}
void Game::advance(float elapsed, Control control) {
  auto& s = *state_;
  if (s.view.phase != Phase::active || !std::isfinite(elapsed) || elapsed <= 0) {
    return;
  }
  if (!std::isfinite(control.movement.x) || !std::isfinite(control.movement.z)) {
    control.movement = {};
  }
  if (control.flight_bounds) {
    const auto area = *control.flight_bounds;
    if (std::isfinite(area.left) && std::isfinite(area.right) && std::isfinite(area.backward) && std::isfinite(area.forward) &&
        area.left < 0 && area.right > 0 && area.backward < 0 && area.forward > 0 &&
        area.left >= -256 && area.right <= 256 && area.backward >= -256 && area.forward <= 256)
      s.flight_bounds = area;
  }
  s.accumulator += std::min(elapsed, tuning::maximum_catchup_seconds);
  constexpr float dt = 1 / tuning::fixed_hz;
  if (control.dash && !s.pending_dash) {
    s.pending_dash = true;
    s.pending_dash_direction = control.dash_direction;
  }
  bool dash = s.pending_dash;
  while (s.accumulator + 0.000001F >= dt && s.view.phase == Phase::active) {
    s.accumulator = std::max(0.0F, s.accumulator - dt);
    control.dash = dash;
    control.dash_direction = s.pending_dash_direction;
    s.step(dt, control);
    dash = false;
    s.pending_dash = false;
    s.pending_dash_direction = {};
  }
  if (s.view.phase != Phase::active) {
    s.accumulator = 0;
  }
  s.sync();
}
void Game::pause() {
  if (state_->view.phase == Phase::active) {
    state_->view.phase = Phase::paused;
    state_->accumulator = 0;
    state_->pending_dash = false;
  }
}
void Game::resume() {
  if (state_->view.phase == Phase::paused) {
    state_->view.phase = Phase::active;
    state_->accumulator = 0;
    state_->pending_dash = false;
  }
}
bool Game::choose_upgrade(std::size_t index) {
  auto& s = *state_;
  if (s.view.phase != Phase::choosing_upgrade || index >= s.view.choices.size()) {
    return false;
  }
  s.apply(s.view.choices[index]);
  ++s.picks;
  s.view.phase = Phase::active;
  s.sync();
  s.check_level();
  s.sync();
  return true;
}
bool Game::reroll() {
  if (state_->view.phase != Phase::choosing_upgrade || state_->view.rerolls_remaining == 0) {
    return false;
  }
  --state_->view.rerolls_remaining;
  state_->draw_choices();
  return true;
}
bool Game::revive_available() const {
  return state_->view.phase == Phase::dead && state_->revive_count < tuning::revive_maximum &&
         state_->view.stats.active_seconds >= tuning::revive_minimum_seconds;
}
bool Game::revive() {
  if (!revive_available()) {
    return false;
  }
  auto& s = *state_;
  ++s.revive_count;
  s.view.hull = s.view.maximum_hull * tuning::revive_hull_fraction;
  s.view.shield = tuning::revive_shield_after;
  s.view.invulnerability_seconds = tuning::revive_invulnerability_seconds;
  std::erase_if(s.actors, [](const Actor& a) { return a.view.kind != EntityKind::boss; });
  s.bullets.clear();
  s.view.beams.clear();
  s.view.phase = Phase::active;
  s.accumulator = 0;
  s.check_level();
  s.sync();
  return true;
}
namespace {
std::uint32_t next_random(std::uint64_t& state) {
  state ^= state >> 12;
  state ^= state << 25;
  state ^= state >> 27;
  return static_cast<std::uint32_t>((state * 2685821657736338717ULL) >> 32);
}
} // namespace
std::uint32_t Game::State::random() {
  return next_random(random_state);
}
float Game::State::combat_unit() {
  return static_cast<float>(next_random(combat_random_state) >> 8) / 16777216.0F;
}
float Game::State::draft_unit() {
  return static_cast<float>(next_random(draft_random_state) >> 8) / 16777216.0F;
}
float Game::State::random_unit() {
  return static_cast<float>(random() >> 8) / 16777216.0F;
}
float Game::State::random_range(float minimum, float maximum) {
  return minimum + (maximum - minimum) * random_unit();
}
unsigned Game::State::rank(Module module) const {
  const auto index = static_cast<std::size_t>(module);
  return index < view.modules.size() && (tuning::available_blueprints & (1U << index)) ? view.modules[index] : 0;
}
float Game::State::research(Research kind) const {
  return view.research[static_cast<std::size_t>(kind)];
}
bool Game::State::evolved(Evolution evolution) const {
  return (view.evolution_bits & (1U << static_cast<unsigned>(evolution))) != 0;
}
float Game::State::permanent(std::size_t track) const {
  return static_cast<float>(setup.permanent_ranks[track]) * tuning::permanent_per_rank[track];
}
Actor* Game::State::actor(std::uint32_t id) {
  const auto found = std::find_if(actors.begin(), actors.end(), [id](const Actor& a) { return a.view.id == id && !a.removed; });
  return found == actors.end() ? nullptr : &*found;
}
void Game::State::sync() {
  update_forward_speed(view);
  view.dash_remaining_seconds = dash_remaining;
  view.drone_burst_seconds = drone_burst;
  view.capture_burst_seconds = capture_burst;
  view.movement_burst_seconds = movement_burst;
  view.maximum_hull = tuning::base_hull + permanent(1) + research(Research::hull_reinforcement);
  view.maximum_shield = tuning::base_shield + permanent(2) + research(Research::shield_capacity) +
                        static_cast<float>(rank(Module::shield_generator)) * tuning::shield_capacity_step;
  view.capture_radius = tuning::base_capture_radius_metres * (1 + static_cast<float>(rank(Module::tractor_lens)) * tuning::lens_radius_step);
  view.beam_capacity = evolved(Evolution::great_vacuum) ? tuning::evolution_beams : static_cast<unsigned>(tuning::base_tractor_beams) + rank(Module::beam_splitter);
  view.laser_count = static_cast<unsigned>(tuning::base_lasers) + rank(Module::laser_array);
  view.weapon_damage = tuning::base_laser_damage * damage_multiplier() * (evolved(Evolution::office_shredder) ? 1 + tuning::evolution_laser_bonus : 1);
  view.next_level_xp = tuning::xp_base + tuning::xp_slope * static_cast<float>(view.stats.level);
  view.stats.score = static_cast<std::uint64_t>(score);
  view.sector = 1 + static_cast<unsigned>(view.stats.active_seconds / tuning::sector_seconds);
  view.entities.clear();
  for (const auto& a : actors) {
    if (!a.removed) {
      auto entity = a.view;
      if (entity.kind == EntityKind::boss && boss && boss->actor_id == entity.id)
        entity.body_heading = boss->body_heading;
      else if (military(entity.kind) && length_squared(a.locomotion_velocity) > .0001F)
        entity.body_heading = std::atan2(a.locomotion_velocity.x, -a.locomotion_velocity.z);
      if (entity.kind == EntityKind::soldier) {
        entity.preparing = a.infantry_aim.preparing;
        entity.aim_pose_seconds = entity.preparing || entity.warning_seconds > 0 ?
            std::optional<float>{a.infantry_aim.pose_seconds} : std::nullopt;
      }
      view.entities.push_back(entity);
    }
  }
  if (boss && actor(boss->actor_id)) {
    view.boss_combat = BossCombatView{boss->actor_id, boss->phase,
        static_cast<float>(boss->remaining_ticks) / tuning::fixed_hz,
        static_cast<float>(boss->total_ticks) / tuning::fixed_hz, boss->next_release, boss->plan};
  } else {
    boss.reset();
    view.boss_combat.reset();
  }
  for (auto& beam : view.beams) {
    const auto* target = actor(beam.target_id);
    beam.sweep_fraction = target ? std::clamp(target->sweep_progress / tuning::harvest_sweep_fraction, 0.F, 1.F) : 0;
    beam.sweep_active = target && target->lifted && sweep_eligible(*target);
  }
  view.projectiles.clear();
  for (const auto& p : bullets) {
    if (!p.removed) {
      auto projectile = p.view;
      projectile.source_id = p.source;
      projectile.age_seconds = p.age;
      view.projectiles.push_back(projectile);
    }
  }
}
void Game::State::step(float dt, Control control) {
  view.stats.active_seconds += dt;
  update_forward_speed(view);
  view.runner_distance += view.forward_speed * dt;
  scroll_military(view.forward_speed * dt);
  view.xp += dt * tuning::survival_xp_per_second;
  advance_timers(dt);
  move(dt, control);
  director(dt);
  enemies(dt);
  capture(dt);
  combat(dt);
  projectiles(dt);
  // Capture/EMP may interrupt after enemies() has prepared a weapon this tick.
  // Settle that interruption in authority before publishing or saving the pose.
  for (auto& actor : actors) {
    if (actor.view.kind == EntityKind::soldier && (actor.lifted || actor.view.capture_progress > 0 || actor.view.disabled_seconds > 0)) {
      actor.infantry_aim = {};
      actor.view.warning_seconds = 0;
      actor.view.fire_recovery_seconds = 0;
      actor.view.velocity = {};
    }
  }
  std::erase_if(actors, [](const Actor& a) { return a.removed; });
  // Area damage or a reflected projectile can retire a captured actor after
  // capture() has published its beam. Keep ownership valid between ticks.
  std::erase_if(view.beams, [this](const BeamView& beam) { return actor(beam.target_id) == nullptr; });
  std::erase_if(bullets, [](const Projectile& p) { return p.removed; });
  std::erase_if(view.shots, [](const ShotView& shot) { return shot.remaining_seconds <= 0; });
  if (view.hull <= 0) {
    view.hull = 0;
    for (auto& actor : actors) if (actor.view.kind == EntityKind::soldier) {
      actor.infantry_aim = {};
      actor.view.warning_seconds = 0;
      actor.view.fire_recovery_seconds = 0;
      actor.view.velocity = {};
    }
    view.phase = Phase::dead;
    return;
  }
  check_level();
}
void Game::State::advance_timers(float dt) {
  view.harvest_chain_seconds = std::max(0.F, view.harvest_chain_seconds - dt);
  view.harvest_overdrive_seconds = std::max(0.F, view.harvest_overdrive_seconds - dt);
  if (view.harvest_chain_seconds == 0) view.harvest_chain = 0;
  for (float* timer : {&view.dash_cooldown_seconds, &view.invulnerability_seconds, &capture_burst, &vehicle_burst,
           &movement_burst, &drone_burst, &vehicle_capture_cooldown, &disable_cooldown, &intercept_cooldown}) {
    *timer = std::max(0.0F, *timer - dt);
  }
  for (auto& shot : view.shots) {
    shot.remaining_seconds -= dt;
  }
  time_since_hit += dt;
  shield_budget_clock += dt;
  if (shield_budget_clock >= 1) {
    shield_budget_clock -= 1;
    capture_shield_budget = 0;
  }
  const auto shield_rank = rank(Module::shield_generator);
  const auto delay = shield_rank ? tuning::shield_delays[shield_rank - 1] : tuning::base_shield_recharge_delay_seconds;
  if (time_since_hit >= delay) {
    view.shield = std::min(view.maximum_shield, view.shield + dt * (tuning::base_shield_regen_per_second + static_cast<float>(shield_rank) * tuning::shield_recharge_step));
  }
}
void Game::State::move(float dt, Control control) {
  auto desired = control.movement;
  if (length_squared(desired) > 1) {
    desired = direction(desired);
  }
  if (length_squared(desired) > 0.01F) {
    last_direction = direction(desired);
  }
  const auto thrusters = static_cast<float>(rank(Module::vector_thrusters));
  const float movement = std::min(tuning::movement_maximum_base_multiplier, 1 + thrusters * tuning::movement_step + (movement_burst > 0 ? tuning::movement_burst : 0));
  const float penalty = view.beams.empty() ? 0 : std::max(0.0F, tuning::base_capture_movement_penalty_percent / 100 - thrusters * tuning::capture_penalty_step);
  view.movement_speed = tuning::base_movement_metres_per_second * movement * (1 - penalty);
  desired = scaled(desired, view.movement_speed);
  desired.z += view.forward_speed;
  // A radial acceleration budget gives diagonal and axial turns equal response.
  // Exponential convergence softens the catch without a dead time at reversal.
  const bool braking = length_squared(control.movement) < .000001F;
  const float response = braking ? tuning::flight_braking_response : tuning::flight_steering_response;
  const auto error = Vec2{desired.x - view.velocity.x, desired.z - view.velocity.z};
  const float change = std::min(-std::expm1(-response * dt),
      tuning::steering_acceleration * dt / std::max(.000001F, std::hypot(error.x, error.z)));
  view.velocity = add(view.velocity, scaled(error, change));
  const Vec2 previous = view.player;
  view.player = add(view.player, scaled(view.velocity, dt));
  const auto phase_rank = rank(Module::phase_drive);
  const bool dashed = control.dash && view.dash_cooldown_seconds <= 0 && dash_remaining <= 0;
  if (dashed) {
    const float rank_offset = phase_rank > 0 ? static_cast<float>(phase_rank - 1) : 0;
    dash_distance = phase_rank > 0 ? tuning::dash_distance : tuning::starter_dash_distance;
    dash_direction = last_direction;
    const auto requested = control.dash_direction;
    const float magnitude = std::hypot(requested.x, requested.z);
    if (std::isfinite(magnitude) && magnitude > .0001F)
      dash_direction = {requested.x / magnitude, requested.z / magnitude};
    last_direction = dash_direction;
    dash_remaining = tuning::flight_dash_seconds;
    present(PresentationEventKind::phase_dash, previous);
    view.dash_cooldown_seconds = phase_rank > 0 ? tuning::dash_cooldown - rank_offset * tuning::dash_cooldown_step : tuning::starter_dash_cooldown;
    const float dash_protection = phase_rank > 0 ? tuning::dash_invulnerability + rank_offset * tuning::dash_invulnerability_step : tuning::starter_dash_invulnerability;
    view.invulnerability_seconds = std::max(view.invulnerability_seconds, dash_protection);
    laser_clock = 0; // Counter-volley uses the existing automatic target/weapon rules.
    next_laser = 0;
    laser_targets.fill(0);
  }
  // Integrate a decelerating burst over active ticks, retaining ordinary flight
  // underneath it so steering flows straight through the catch. Bounds absorb
  // outward travel immediately; no displacement is stored for a later release.
  if (dash_remaining > 0) {
    const float before = dash_remaining / tuning::flight_dash_seconds;
    dash_remaining = std::max(0.F, dash_remaining - dt);
    if (dash_remaining < .000001F) dash_remaining = 0;
    const float after = dash_remaining / tuning::flight_dash_seconds;
    view.player = add(view.player, scaled(dash_direction, dash_distance * (before * before - after * after)));
    if (dash_remaining == 0) { dash_direction = {}; dash_distance = 0; }
  }
  const auto unclamped = view.player;
  view.player.x = std::clamp(view.player.x, flight_bounds.left, flight_bounds.right);
  view.player.z = std::clamp(view.player.z, view.runner_distance + flight_bounds.backward, view.runner_distance + flight_bounds.forward);
  if (view.player.x != unclamped.x) view.velocity.x = 0;
  if (view.player.z != unclamped.z) view.velocity.z = view.forward_speed;
}
void Game::State::check_level() {
  if (view.phase != Phase::active) {
    return;
  }
  const float cost = tuning::xp_base + tuning::xp_slope * static_cast<float>(view.stats.level);
  if (view.xp < cost) {
    return;
  }
  view.xp -= cost;
  ++view.stats.level;
  view.phase = Phase::choosing_upgrade;
  draw_choices();
}
} // namespace human_resources::gameplay
