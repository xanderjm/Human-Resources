#pragma once
#include "capture_cancel_trace.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <human_resources/gameplay/tuning.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/renderer/style_renderer.h>
#include <iostream>

namespace hrqa {
inline void infantry_transition_proof(hr::renderer::StyleRenderer& renderer, hr::progression::AccountStore& store, const char* path) {
  const auto trace = actual_capture_cancel_trace();
  const auto& first = trace.samples.front();
  const auto& before = trace.samples[trace.cancelled_at - 1];
  const auto& cancelled = trace.samples[trace.cancelled_at];
  std::cout << "SCRIPTED NATIVE TRANSITIONS: three role bodies replay one real capture-loss trace; this is not an earned encounter\n"
            << "actual-trace seed=514 actor=" << first.actor.id << " source_kind=" << static_cast<unsigned>(first.actor.kind)
            << " samples=" << trace.samples.size() << " cancel_index=" << trace.cancelled_at
            << " before_seconds=" << before.seconds << " before_progress=" << before.actor.capture_progress
            << " cancel_seconds=" << cancelled.seconds << " cancel_progress=" << cancelled.actor.capture_progress << '\n';
  auto* stream = SDL_IOFromFile(path, "wb");
  if (!stream)
    throw std::runtime_error(SDL_GetError());
  hr::product::SceneEffects effects;
  constexpr float dt = 1.F / 60, start_station = 33.6F, capture_start = 5.7F;
  constexpr unsigned frames = 720, capture_frame = 342;
  const float initial_z = first.actor.position.z - first.runner_distance;
  const float walk_speed = (5 - initial_z) / (capture_start - 2.8F);
  bool ok = true;
  for (unsigned frame = 0; frame < frames; ++frame) {
    const float t = static_cast<float>(frame) * dt;
    hr::gameplay::Snapshot run;
    run.phase = hr::gameplay::Phase::active;
    run.runner_distance = start_station;
    run.player = {0, start_station + first.player.z - first.runner_distance};
    run.hull = run.maximum_hull = 100;
    run.shield = run.maximum_shield = 20;
    run.beam_capacity = 3;
    run.modules[static_cast<unsigned>(hr::gameplay::Module::beam_splitter)] = 2;
    run.stats.active_seconds = 100 + static_cast<double>(frame) / 60;
    run.stats.level = 1;
    run.next_level_xp = 100;
    const CaptureSample* sample = nullptr;
    float after_trace = 0;
    if (frame >= capture_frame) {
      const auto index = static_cast<std::size_t>(frame - capture_frame);
      sample = &trace.samples[std::min(index, trace.samples.size() - 1)];
      after_trace = index >= trace.samples.size() ? static_cast<float>(index - trace.samples.size() + 1) * dt : 0;
      run.runner_distance += sample->runner_distance - first.runner_distance + after_trace * hr::gameplay::tuning::forward_scroll_metres_per_second;
      run.player = {sample->player.x - first.player.x, start_station + sample->player.z - first.runner_distance + after_trace * hr::gameplay::tuning::forward_scroll_metres_per_second};
    }
    for (unsigned role = 1; role <= 3; ++role) {
      const auto typed = static_cast<hr::gameplay::InfantryRole>(role);
      const auto attack = hr::gameplay::infantry_attack(typed);
      const float x = (static_cast<float>(role) - 2) * 3;
      hr::gameplay::EntityView actor;
      actor.id = 100 + role;
      actor.kind = hr::gameplay::EntityKind::soldier;
      actor.infantry_role = typed;
      actor.hull = actor.maximum_hull = 100;
      actor.position = {x, start_station + 5 + (t < 1 ? (1 - t) * 1.6F : 0)};
      actor.velocity = t < 1 ? hr::gameplay::Vec2{0, -1.6F} : hr::gameplay::Vec2{};
      actor.warning_aim = run.player;
      const float fire_at = 1 + attack.warning_seconds;
      if (t >= 1 && t < fire_at)
        actor.warning_seconds = fire_at - t;
      if (t >= fire_at && t < fire_at + attack.recovery_seconds)
        actor.fire_recovery_seconds = fire_at + attack.recovery_seconds - t;
      if (t >= 2.8F && !sample) {
        actor.position.z = start_station + 5 - walk_speed * (t - 2.8F);
        actor.velocity = {0, -walk_speed};
      }
      if (sample) {
        actor.position = {x + sample->actor.position.x - first.actor.position.x + sample->actor.velocity.x * after_trace,
            start_station + sample->actor.position.z - first.runner_distance + sample->actor.velocity.z * after_trace};
        actor.velocity = sample->actor.velocity;
        actor.capture_progress = sample->actor.capture_progress;
        actor.warning_seconds = actor.fire_recovery_seconds = 0;
        if (sample->beam)
          run.beams.push_back({actor.id, actor.position, actor.capture_progress, false});
      }
      run.entities.push_back(actor);
      if (t >= fire_at) {
        const hr::gameplay::Vec2 origin{x, start_station + 5};
        const auto muzzle = hr::gameplay::infantry_shot(typed, origin, {0, start_station + first.player.z - first.runner_distance});
        if (!muzzle.valid)
          throw std::runtime_error("Scripted source-derived shot is invalid");
        run.presentation_events[role - 1] = {role, 100 + static_cast<double>(fire_at), hr::gameplay::PresentationEventKind::enemy_shot, actor.id, muzzle.origin, 0};
        const float age = t - fire_at;
        const float flight = std::hypot(muzzle.origin.x, muzzle.origin.z - (start_station + first.player.z - first.runner_distance)) / attack.projectile_speed;
        if (age <= flight + .25F)
          for (unsigned pellet = 0; pellet < attack.pellets; ++pellet) {
            const float angle = (static_cast<float>(pellet) - static_cast<float>(attack.pellets - 1) * .5F) * attack.spread_radians;
            const hr::gameplay::Vec2 velocity{attack.projectile_speed * (muzzle.direction.x * std::cos(angle) - muzzle.direction.z * std::sin(angle)), attack.projectile_speed * (muzzle.direction.x * std::sin(angle) + muzzle.direction.z * std::cos(angle))};
            run.projectiles.push_back({.id = role * 10 + pellet + 1, .position = {muzzle.origin.x + velocity.x * age, muzzle.origin.z + velocity.z * age}, .velocity = velocity, .radius = .23F, .damage = 6 * attack.damage_fraction, .source_id = actor.id, .infantry_role = typed, .launch_position = muzzle.origin, .age_seconds = age, .launch_height = muzzle.height, .rise_per_second = (2.5F - muzzle.height) / flight,
                .kind=typed==hr::gameplay::InfantryRole::rocketeer ? hr::gameplay::EnemyProjectileKind::infantry_rocket : hr::gameplay::EnemyProjectileKind::infantry_round,
                .launch_direction=muzzle.direction});
          }
      }
    }
    effects.update(run);
    auto scene = hr::product::scene(run, 588.F / 1280.F, false, false, &effects);
    scene.active_delta_seconds = dt;
    hr::product::View view;
    view.screen = hr::product::Screen::run;
    view.run = &run;
    view.store = &store;
    view.viewport = {0, 0, 390, 844};
    scene.screen_quads = hr::product::interface(view).quads;
    if (frame + 1 == capture_frame + trace.cancelled_at || frame == capture_frame + trace.cancelled_at || frame == capture_frame + trace.cancelled_at + 84)
      for (const auto& body : scene.instances)
        if (body.stable_id >= 101 && body.stable_id <= 103)
          std::cout << "cancel-pose frame=" << frame << " actor=" << body.stable_id << " station=" << run.runner_distance << " progress=" << (sample ? sample->actor.capture_progress : 0)
                    << " root=" << body.position.x << ',' << body.position.y << ',' << body.position.z
                    << " scale=" << body.scale.x << ',' << body.scale.y << ',' << body.scale.z
                    << " rotation=" << body.rotation.x << ',' << body.rotation.y << ',' << body.rotation.z
                    << " weapon_parent=" << body.stable_id << " head=skinned_same_actor\n";
    if (!renderer.render(scene, stream, false)) {
      ok = false;
      break;
    }
  }
  SDL_CloseIO(stream);
  if (!ok || !renderer.wait_idle())
    throw std::runtime_error(SDL_GetError());
  std::cout << "scripted-native-transition frames=" << frames << " fps=60 capture_start_frame=" << capture_frame
            << " cancellation_frame=" << capture_frame + trace.cancelled_at << " source_progress_before=" << before.actor.capture_progress << " source_progress_after=" << cancelled.actor.capture_progress << '\n';
}
} // namespace hrqa
