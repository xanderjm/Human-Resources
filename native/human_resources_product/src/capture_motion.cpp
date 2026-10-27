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

unsigned SceneEffects::capture_port(std::uint32_t actor_id) const {
  const auto found=std::find_if(intake_bindings_.begin(),intake_bindings_.end(),[&](const auto& b){return b.id==actor_id&&actor_id!=0;});
  return found==intake_bindings_.end()?0:found->port;
}
void SceneEffects::bind_capture_ports(const gameplay::Snapshot& run) {
  const unsigned count=std::clamp(run.beam_capacity,1U,6U);
  std::array<bool,6> occupied{};
  const auto live=[&](std::uint32_t id){return id!=0&&std::any_of(run.beams.begin(),run.beams.end(),[&](const auto& b){return b.target_id==id;});};
  for(auto& b:intake_bindings_) {
    if(b.id&&run.stats.active_seconds-b.last_seen>.65&&!live(b.id)) b={};
    if(live(b.id)&&b.port<count) {occupied[b.port]=true;b.last_seen=run.stats.active_seconds;}
  }
  for(const auto& beam:run.beams) {
    if(!beam.target_id)continue;
    const auto existing=std::find_if(intake_bindings_.begin(),intake_bindings_.end(),[&](const auto& b){return b.id==beam.target_id;});
    if(existing!=intake_bindings_.end()&&existing->port<count)continue;
    auto free=std::find_if(intake_bindings_.begin(),intake_bindings_.end(),[](const auto& b){return b.id==0;});
    if(free==intake_bindings_.end()) {
      free=std::min_element(intake_bindings_.begin(),intake_bindings_.end(),[&](const auto& a,const auto& b){return (live(a.id)?run.stats.active_seconds+1:a.last_seen)<(live(b.id)?run.stats.active_seconds+1:b.last_seen);});
    }
    const auto port=std::find(occupied.begin(),occupied.begin()+count,false);
    const auto index=port==occupied.begin()+count?0U:static_cast<unsigned>(port-occupied.begin());
    *free={beam.target_id,index,run.stats.active_seconds};occupied[index]=true;
  }
}
void SceneEffects::begin_capture_frame(const gameplay::Snapshot& run) {
  const double now = run.stats.active_seconds;
  if (capture_initialized_ && now < capture_seconds_) {captures_ = {};intake_bindings_ = {};}
  bind_capture_ports(run);
  const float elapsed = capture_initialized_ ? std::clamp(static_cast<float>(now - capture_seconds_), 0.F, .25F) : 0;
  constexpr float intake_seconds = .34F;
  completed_count_ = 0;
  for (auto& history : captures_) {
    if (!history.id) continue;
    const auto found = std::find_if(run.entities.begin(), run.entities.end(), [&](const auto& actor) {
      return actor.id == history.id && actor.kind == history.kind && actor.infantry_role == history.role;
    });
    if (found != run.entities.end()) {
      if (history.completed) {
        history = {}; // Even a malformed reused ID cannot inherit a finisher.
        continue;
      }
      history.actor = *found;
      history.display_progress = found->kind <= gameplay::EntityKind::soldier ? capture_lift_progress(found->capture_progress) : found->capture_progress;
      continue;
    }
    const bool confirmed = std::any_of(beats().begin(), beats().end(), [&](const auto& beat) {
      return beat.kind == SceneBeatKind::capture && beat.actor_id == history.id;
    });
    const bool reused = std::any_of(run.entities.begin(), run.entities.end(), [&](const auto& actor) {
      return actor.id == history.id;
    });
    if (reused || history.actor.kind > gameplay::EntityKind::soldier || (!history.completed && !confirmed)) {
      history = {};
      continue;
    }
    if (!history.completed) {
      history.start = history.last;
      history.finish_progress = history.display_progress;
      history.releasing = history.reacquiring = false;
    }
    history.completed = true;
    history.display_progress = std::min(1.F, history.display_progress + (1 - history.finish_progress) * elapsed / intake_seconds);
    if (history.display_progress >= 1) {
      history = {};
      continue;
    }
    auto actor = history.actor;
    actor.capture_progress = history.display_progress;
    actor.warning_seconds = actor.fire_recovery_seconds = 0;
    actor.velocity = {};
    completed_captures_[completed_count_++] = actor;
  }
  capture_seconds_ = now;
  capture_initialized_ = true;
}
float SceneEffects::capture_progress(const gameplay::EntityView& actor) const {
  if (actor.capture_progress <= 0)
    return 0;
  const auto found = std::find_if(captures_.begin(), captures_.end(), [&](const auto& history) {
    return history.id == actor.id && history.kind == actor.kind && history.role == actor.infantry_role;
  });
  return found == captures_.end() ? capture_lift_progress(actor.capture_progress) : found->display_progress;
}

CapturePose SceneEffects::capture_pose(const gameplay::EntityView& actor, CapturePose captured,
    CapturePose ground, double seconds, float station, CaptureMotion motion) {
  auto found = std::find_if(captures_.begin(), captures_.end(), [&](const auto& history) { return history.id == actor.id; });
  if (found == captures_.end()) {
    found = std::find_if(captures_.begin(), captures_.end(), [](const auto& history) { return history.id == 0; });
    if (found == captures_.end()) return captured; // Legal Game snapshots never exceed actor_capacity.
    *found = {.id = actor.id, .kind = actor.kind, .role = actor.infantry_role, .last = captured, .seconds = seconds, .station = station, .capture_yaw = captured.rotation.y, .capturing = actor.capture_progress > 0, .actor = actor, .display_progress = actor.kind <= gameplay::EntityKind::soldier ? capture_lift_progress(actor.capture_progress) : actor.capture_progress};
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
  if (history.completed) {
    const float u = std::clamp((history.display_progress - history.finish_progress) /
                                   std::max(.001F, 1 - history.finish_progress),
        0.F, 1.F);
    const float v = 1 - u;
    // Keep the full source specimen readable beside the hull before the final
    // intake zip. The first point is exactly its last visible canonical pose.
    // Reduced motion retains a straight finish without orbit or body tumble.
    const bool reduced = motion == CaptureMotion::reduced;
    auto resolved = blend(history.start, captured, smooth(0, 1, u));
    if (!reduced) {
      const float side = history.id & 1U ? 1.F : -1.F;
      const presentation::Vec3 control{history.start.position.x + side * 2.2F,
          std::min(captured.position.y, history.start.position.y + .8F),
          std::max(history.start.position.z, captured.position.z) + 2.5F};
      resolved.position = {v * v * history.start.position.x + 2 * v * u * control.x + u * u * captured.position.x,
          v * v * history.start.position.y + 2 * v * u * control.y + u * u * captured.position.y,
          v * v * history.start.position.z + 2 * v * u * control.z + u * u * captured.position.z};
      resolved.rotation = {history.start.rotation.x + u * 1.25F,
          history.start.rotation.y + side * u * 1.4F, history.start.rotation.z + side * u * .65F};
    } else
      resolved.rotation = history.start.rotation;
    resolved.scale = history.start.scale + (captured.scale - history.start.scale) * smooth(.5F, 1, u);
    history.last = resolved;
    history.seconds = seconds;
    history.capturing = true;
    return resolved;
  }
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
