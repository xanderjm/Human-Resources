#include <human_resources/product/scene_effects.h>

#include <algorithm>
#include <cmath>
#include <numbers>
namespace human_resources::product {
void SceneEffects::reset() {
  *this = SceneEffects{};
}
float SceneEffects::wheel_angle(std::uint32_t id) const {
  for (std::size_t i = 0; i < previous_count_; ++i)
    if (previous_[i].id == id)
      return wheel_angles_[i];
  return 0;
}
void SceneEffects::emit(const gameplay::PresentationEvent& event, double now) {
  const float age = static_cast<float>(now - event.active_seconds);
  if (age < 0 || age > .55F)
    return;
  switch (event.kind) {
  case SceneBeatKind::capture:
    feedback_.capture = true;
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
    *oldest = {event.kind, event.position, event.actor_id, age, event.radius};
    return;
  }
  beats_[beat_count_++] = {event.kind, event.position, event.actor_id, age, event.radius};
}
void SceneEffects::update(const gameplay::Snapshot& run) {
  feedback_ = {};
  const double now = run.stats.active_seconds;
  if (!std::isfinite(now) || now < 0)
    return;
  if (initialized_ && now < seconds_)
    reset();
  if (initialized_ && now == seconds_)
    return; // Pause, upgrade, background and protected saves freeze all effects.
  const float elapsed = initialized_ ? static_cast<float>(now - seconds_) : 0;
  for (std::size_t i = 0; i < beat_count_;) {
    beats_[i].age += elapsed;
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
    emit(*pending[i], now);
    last_sequence_ = pending[i]->sequence;
  }
  std::array<float, actor_capacity> next_angles{};
  const auto next_count = std::min(run.entities.size(), actor_capacity);
  for (std::size_t i = 0; i < next_count; ++i)
    if (run.entities[i].kind == gameplay::EntityKind::jeep)
      for (std::size_t j = 0; j < previous_count_; ++j)
        if (previous_[j].id == run.entities[i].id) {
          const auto a = previous_[j].position, b = run.entities[i].position;
          next_angles[i] = std::fmod(wheel_angles_[j] - std::min(5.F, std::hypot(b.x - a.x, b.z - a.z)) / .3F, 2 * std::numbers::pi_v<float>);
          break;
        }
  wheel_angles_ = next_angles;
  previous_count_ = next_count;
  std::copy_n(run.entities.begin(), previous_count_, previous_.begin());
  seconds_ = now;
  initialized_ = true;
}
} // namespace human_resources::product
