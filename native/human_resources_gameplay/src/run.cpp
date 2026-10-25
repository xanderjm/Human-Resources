#include "run_state.h"

namespace human_resources::gameplay {
using namespace detail;
namespace {
float forward_speed(const Snapshot& view) {
  return tuning::forward_scroll_metres_per_second + std::min(
      tuning::scroll_maximum_metres_per_second - tuning::forward_scroll_metres_per_second,
      static_cast<float>(view.stats.captures) * tuning::scroll_per_capture_metres_per_second +
          static_cast<float>(view.stats.level - 1) * tuning::scroll_per_level_metres_per_second);
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
  s.setup.unlocked_blueprints |= starting_blueprints();
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
}
void Game::advance(float elapsed, Control control) {
  auto& s = *state_;
  if (s.view.phase != Phase::active || !std::isfinite(elapsed) || elapsed <= 0) {
    return;
  }
  if (!std::isfinite(control.movement.x) || !std::isfinite(control.movement.z)) {
    control.movement = {};
  }
  s.accumulator += std::min(elapsed, tuning::maximum_catchup_seconds);
  constexpr float dt = 1 / tuning::fixed_hz;
  s.pending_dash = s.pending_dash || control.dash;
  bool dash = s.pending_dash;
  while (s.accumulator + 0.000001F >= dt && s.view.phase == Phase::active) {
    s.accumulator = std::max(0.0F, s.accumulator - dt);
    control.dash = dash;
    s.step(dt, control);
    dash = false;
    s.pending_dash = false;
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
  return view.modules[static_cast<std::size_t>(module)];
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
  view.forward_speed = forward_speed(view);
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
      if (military(entity.kind) && entity.kind != EntityKind::soldier && length_squared(a.locomotion_velocity) > .0001F)
        entity.body_heading = std::atan2(a.locomotion_velocity.x, -a.locomotion_velocity.z);
      view.entities.push_back(entity);
    }
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
  view.forward_speed = forward_speed(view);
  view.runner_distance += view.forward_speed * dt;
  view.xp += dt * tuning::survival_xp_per_second;
  advance_timers(dt);
  move(dt, control);
  director(dt);
  enemies(dt);
  capture(dt);
  combat(dt);
  projectiles(dt);
  std::erase_if(actors, [](const Actor& a) { return a.removed; });
  // Area damage or a reflected projectile can retire a captured actor after
  // capture() has published its beam. Keep ownership valid between ticks.
  std::erase_if(view.beams, [this](const BeamView& beam) { return actor(beam.target_id) == nullptr; });
  std::erase_if(bullets, [](const Projectile& p) { return p.removed; });
  std::erase_if(view.shots, [](const ShotView& shot) { return shot.remaining_seconds <= 0; });
  if (view.hull <= 0) {
    view.hull = 0;
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
  const float max_change = tuning::steering_acceleration * dt;
  view.velocity.x += std::clamp(desired.x - view.velocity.x, -max_change, max_change);
  view.velocity.z += std::clamp(desired.z - view.velocity.z, -max_change, max_change);
  const Vec2 previous = view.player;
  view.player = add(view.player, scaled(view.velocity, dt));
  const auto phase_rank = rank(Module::phase_drive);
  const bool dashed = control.dash && view.dash_cooldown_seconds <= 0;
  if (dashed) {
    const float rank_offset = phase_rank > 0 ? static_cast<float>(phase_rank - 1) : 0;
    const float distance = phase_rank > 0 ? tuning::dash_distance : tuning::starter_dash_distance;
    view.player = add(view.player, scaled(last_direction, distance));
    present(PresentationEventKind::phase_dash, previous);
    view.dash_cooldown_seconds = phase_rank > 0 ? tuning::dash_cooldown - rank_offset * tuning::dash_cooldown_step : tuning::starter_dash_cooldown;
    const float dash_protection = phase_rank > 0 ? tuning::dash_invulnerability + rank_offset * tuning::dash_invulnerability_step : tuning::starter_dash_invulnerability;
    view.invulnerability_seconds = std::max(view.invulnerability_seconds, dash_protection);
    laser_clock = 0; // Counter-volley uses the existing automatic target/weapon rules.
  }
  view.player.x = std::clamp(view.player.x, -tuning::field_half_width, tuning::field_half_width);
  view.player.z = std::clamp(view.player.z, view.runner_distance - tuning::player_backward_allowance, view.runner_distance + tuning::player_forward_allowance);
  if (!dashed) {
    view.velocity = {(view.player.x - previous.x) / dt, (view.player.z - previous.z) / dt};
  }
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
