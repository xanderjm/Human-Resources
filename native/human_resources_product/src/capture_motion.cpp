#include <human_resources/product/scene_effects.h>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace human_resources::product {
namespace {
float smooth(float low, float high, float value) {
  const float u = std::clamp((value - low) / (high - low), 0.F, 1.F);
  return u * u * (3 - 2 * u);
}
float angle(float from, float to, float weight) {
  return from + std::remainder(to - from, 2 * std::numbers::pi_v<float>) * weight;
}
CapturePose blend(const CapturePose& from, const CapturePose& to, float weight) {
  const auto vector = [weight](auto a, auto b) {
    return presentation::Vec3{a.x + (b.x - a.x) * weight, a.y + (b.y - a.y) * weight, a.z + (b.z - a.z) * weight};
  };
  return {vector(from.position, to.position),
      {angle(from.rotation.x, to.rotation.x, weight), angle(from.rotation.y, to.rotation.y, weight), angle(from.rotation.z, to.rotation.z, weight)},
      from.scale + (to.scale - from.scale) * weight};
}
} // namespace

void SceneEffects::begin_capture_frame(const gameplay::Snapshot& run) {
  const double now = run.stats.active_seconds;
  if (capture_initialized_ && now < capture_seconds_) captures_ = {};
  for (auto& history : captures_) {
    if (!history.id) continue;
    const auto found = std::find_if(run.entities.begin(), run.entities.end(), [&](const auto& actor) {
      return actor.id == history.id && actor.kind == history.kind && actor.infantry_role == history.role;
    });
    if (found == run.entities.end()) history = {};
  }
  capture_seconds_ = now;
  capture_initialized_ = true;
}

CapturePose SceneEffects::capture_pose(const gameplay::EntityView& actor, CapturePose captured,
    CapturePose ground, double seconds, float station) {
  auto found = std::find_if(captures_.begin(), captures_.end(), [&](const auto& history) { return history.id == actor.id; });
  if (found == captures_.end()) {
    found = std::find_if(captures_.begin(), captures_.end(), [](const auto& history) { return history.id == 0; });
    if (found == captures_.end()) return captured; // Legal Game snapshots never exceed actor_capacity.
    *found = {.id = actor.id, .kind = actor.kind, .role = actor.infantry_role, .last = captured,
        .seconds = seconds, .station = station, .capture_yaw = captured.rotation.y, .capturing = actor.capture_progress > 0};
    return captured;
  }
  auto& history = *found;
  // Render coordinates follow the runner. Carry history through that origin
  // change so an active release stays in the same gameplay world position.
  history.last.position.z += station - history.station;
  history.start.position.z += station - history.station;
  history.ground.position.z += station - history.station;
  history.station = station;
  if (seconds == history.seconds) return history.last;
  const bool capturing = actor.capture_progress > 0;
  if (capturing && actor.kind >= gameplay::EntityKind::jeep) {
    if (!history.capturing) history.capture_yaw=history.last.rotation.y;
    captured.rotation.y=history.capture_yaw;
  }
  if (history.capturing && !capturing) {
    history.start = history.last;
    history.ground = ground;
    history.transition_seconds = seconds;
    history.releasing = true;
    history.reacquiring = false;
  } else if (!history.capturing && capturing && history.releasing) {
    history.start = history.last;
    history.ground = ground;
    history.transition_seconds = seconds;
    history.releasing = false;
    history.reacquiring = true;
  }
  CapturePose resolved = captured;
  if (history.releasing) {
    const float age = static_cast<float>(seconds - history.transition_seconds);
    // Product owns the complete world return. Duration matches beam_release;
    // no authored root-motion curve or source lift height is duplicated here.
    const float remaining = 1 - smooth(0, 1, age / capture_release_seconds());
    CapturePose from = history.start;
    from.position.x += ground.position.x - history.ground.position.x;
    from.position.y += ground.position.y - history.ground.position.y;
    from.position.z += ground.position.z - history.ground.position.z;
    resolved = age == 0 ? from : blend(ground, from, remaining);
    if (age >= capture_release_seconds()) history.releasing = false;
  } else if (history.reacquiring) {
    const float weight = smooth(0, .12F, static_cast<float>(seconds - history.transition_seconds));
    resolved = blend(history.start, captured, weight);
    if (weight == 1) history.reacquiring = false;
  }
  history.last = resolved;
  history.seconds = seconds;
  history.capturing = capturing;
  return resolved;
}
} // namespace human_resources::product
