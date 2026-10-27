#include <human_resources/product/scene_effects.h>
#include <human_resources/product/ship_appearance.h>

#include <algorithm>
#include <cmath>
#include <numbers>
namespace human_resources::product {
void SceneEffects::reset() {
  *this = SceneEffects{};
}
DamageFeedback SceneEffects::damage_feedback() const {
  DamageFeedback result;
  for (const auto& beat : beats()) {
    const bool hull = beat.kind == SceneBeatKind::hull_hit;
    if (!hull && beat.kind != SceneBeatKind::shield_hit)
      continue;
    if (result.age < 0 || beat.age < result.age || (beat.age == result.age && hull))
      result = {beat.age, hull, beat.damage_cause};
  }
  return result;
}
float SceneEffects::wheel_angle(std::uint32_t id) const {
  for (std::size_t i = 0; i < previous_count_; ++i)
    if (previous_[i].id == id)
      return wheel_angles_[i];
  return 0;
}
float SceneEffects::capture_response() const {
  float response = 0;
  for (const auto& beat : beats())
    if (beat.kind == SceneBeatKind::capture || beat.kind == SceneBeatKind::sweep_capture) {
      // Fast compression followed by a gentler return, shared across same-frame
      // captures so a dense volley cannot multiply the ship's displacement.
      const float envelope = (1 - std::exp(-beat.age * 70)) * std::exp(-beat.age * 12) * 1.6F;
      response = std::max(response, envelope * (beat.kind == SceneBeatKind::sweep_capture ? 1.3F : 1.F));
    }
  return std::clamp(response, 0.F, 1.F);
}
gameplay::Vec2 SceneEffects::dash_response() const {
  gameplay::Vec2 response{};
  for (const auto& beat : beats())
    if (beat.kind == SceneBeatKind::phase_dash && beat.age < .22F) {
      const float dx = beat.destination.x - beat.position.x, dz = beat.destination.z - beat.position.z;
      const float length = std::hypot(dx, dz);
      if (length > .05F) {
        const float weight = std::exp(-beat.age * 18);
        response = {dx / length * weight, dz / length * weight};
      }
    }
  return response;
}
void SceneEffects::emit(const gameplay::PresentationEvent& event, const gameplay::Snapshot& run) {
  const float age = static_cast<float>(run.stats.active_seconds - event.active_seconds);
  if (age < 0 || age > .55F)
    return;
  switch (event.kind) {
  case SceneBeatKind::capture:
    feedback_.capture = true;
    break;
  case SceneBeatKind::sweep_capture:
    feedback_.sweep_capture = true;
    break;
  case SceneBeatKind::harvest_overdrive:
    feedback_.harvest_overdrive = true;
    break;
  case SceneBeatKind::phase_dash:
    feedback_.phase_dash = true;
    break;
  case SceneBeatKind::laser_volley:
    feedback_.laser = true;
    return;
  case SceneBeatKind::warning_lock:
    feedback_.warning = true;
    return;
  case SceneBeatKind::shield_hit:
    feedback_.hit = feedback_.shield_hit = true;
    break;
  case SceneBeatKind::hull_hit:
    feedback_.hit = feedback_.hull_hit = true;
    break;
  default:
    break;
  }
  if (beat_count_ == beat_capacity) {
    const auto oldest = std::max_element(beats_.begin(), beats_.end(), [](const auto& a, const auto& b) { return a.age < b.age; });
    *oldest = {event.kind, event.position, event.actor_id, age, event.radius, run.player, event.damage_cause, event.kind == SceneBeatKind::phase_dash && run.dash_remaining_seconds > 0};
    return;
  }
  beats_[beat_count_++] = {event.kind, event.position, event.actor_id, age, event.radius, run.player, event.damage_cause, event.kind == SceneBeatKind::phase_dash && run.dash_remaining_seconds > 0};
}
void SceneEffects::update(const gameplay::Snapshot& run) {
  feedback_ = {};
  const double now = run.stats.active_seconds;
  if (!std::isfinite(now) || now < 0)
    return;
  if (initialized_ && now < seconds_)
    reset();
  // Immediate repairs can commit while the upgrade modal holds active time.
  if (initialized_ && now >= seconds_ && run.maximum_hull == previous_maximum_hull_ && run.hull > previous_hull_)
    repair_seconds_ = .65F;
  previous_hull_ = run.hull;
  previous_maximum_hull_ = run.maximum_hull;
  if (initialized_ && now == seconds_)
    return; // Pause, upgrade, background and protected saves freeze all effects.
  const float elapsed = initialized_ ? static_cast<float>(now - seconds_) : 0;
  const float blend = initialized_ ? -std::expm1(-std::min(elapsed, .25F) * 12.F) : 1.F;
  const gameplay::Vec2 target{run.velocity.x, run.velocity.z - run.forward_speed};
  if (!initialized_) { flight_velocity_ = target; flight_rate_ = {}; }
  else {
    // Exact underdamped spring: small overshoot, one soft return, no positional
    // lag. Closed-form integration stays coherent at 30/60/120Hz and on pause.
    const float dt = std::min(elapsed, .25F);
    constexpr float damping = 16.F, frequency = 16.F;
    const float decay = std::exp(-damping * dt), cosine = std::cos(frequency * dt), sine = std::sin(frequency * dt);
    const auto spring = [&](float& value, float& rate, float goal) {
      const float offset = value - goal, term = (rate + damping * offset) / frequency;
      value = goal + decay * (offset * cosine + term * sine);
      rate = decay * (rate * cosine - (damping * term + frequency * offset) * sine);
    };
    spring(flight_velocity_.x, flight_rate_.x, target.x);
    spring(flight_velocity_.z, flight_rate_.z, target.z);
  }
  repair_seconds_ = std::max(0.F, repair_seconds_ - elapsed);
  const float desired_speed = ship_appearance(run).rotor_speed;
  if (!initialized_)
    rotor_angle_ = std::fmod(static_cast<float>(now) * desired_speed, 2 * std::numbers::pi_v<float>);
  rotor_speed_ += (desired_speed - rotor_speed_) * blend;
  rotor_angle_ = std::fmod(rotor_angle_ + elapsed * rotor_speed_, 2 * std::numbers::pi_v<float>);
  for (std::size_t i = 0; i < beat_count_;) {
    beats_[i].age += elapsed;
    if (beats_[i].dash_travelling) {
      beats_[i].destination = run.player;
      beats_[i].dash_travelling = run.dash_remaining_seconds > 0;
    }
    if (beats_[i].age > .55F)
      beats_[i] = beats_[--beat_count_];
    else
      ++i;
  }
  // Ring slots may wrap, so consume in sequence order with bounded scratch.
  std::array<const gameplay::PresentationEvent*, gameplay::presentation_event_capacity> pending{};
  std::size_t count = 0;
  for (const auto& event : run.presentation_events)
    if (event.sequence > last_sequence_)
      pending[count++] = &event;
  std::sort(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(count), [](const auto* a, const auto* b) { return a->sequence < b->sequence; });
  for (std::size_t i = 0; i < count; ++i) {
    emit(*pending[i], run);
    last_sequence_ = pending[i]->sequence;
  }
  std::array<float, actor_capacity> next_angles{};
  const auto next_count = std::min(run.entities.size(), actor_capacity);
  for (std::size_t i = 0; i < next_count; ++i)
    if (run.entities[i].kind == gameplay::EntityKind::jeep)
      for (std::size_t j = 0; j < previous_count_; ++j)
        if (previous_[j].id == run.entities[i].id) {
          const auto a = previous_[j].position, b = run.entities[i].position;
          const float yaw = gameplay::vehicle_body_yaw(run.entities[i]);
          const float travel = (b.x - a.x) * std::sin(yaw) - (b.z - a.z) * std::cos(yaw);
          next_angles[i] = std::fmod(wheel_angles_[j] - std::clamp(travel, -5.F, 5.F) / .3F, 2 * std::numbers::pi_v<float>);
          break;
        }
  wheel_angles_ = next_angles;
  previous_count_ = next_count;
  std::copy_n(run.entities.begin(), previous_count_, previous_.begin());
  seconds_ = now;
  initialized_ = true;
}
} // namespace human_resources::product
