#pragma once
#include <algorithm>
#include <human_resources/gameplay/gameplay.h>
#include <map>
#include <stdexcept>
#include <vector>

namespace hrqa {
namespace hr = human_resources;
struct CaptureSample final {
  hr::gameplay::EntityView actor;
  hr::gameplay::Vec2 player;
  float runner_distance{};
  double seconds{};
  bool beam{};
};
struct CaptureTrace final {
  std::vector<CaptureSample> samples;
  std::size_t cancelled_at{};
};
// Obtain an actual range-loss reset through public Game stepping. No fixtures,
// private state, invulnerability, progress ramp, save injection or rule overrides.
inline CaptureTrace actual_capture_cancel_trace() {
  hr::gameplay::Game game;
  game.start({.seed = 514});
  std::map<std::uint32_t, std::vector<CaptureSample>> pending;
  CaptureTrace result;
  std::uint32_t selected{}, escaping{};
  constexpr float step = 1.F / 60;
  for (unsigned frame = 0; frame < 10800; ++frame) {
    if (game.snapshot().phase == hr::gameplay::Phase::choosing_upgrade)
      (void)game.choose_upgrade(0);
    // New fleeing cohorts can complete every idle capture. Deliberately dash
    // away after a real deep lift, then keep steering away through its release.
    // This remains ordinary public input, not fixture state or a rule override.
    hr::gameplay::Control control{};
    for (const auto& actor : game.snapshot().entities) {
      if ((!escaping && actor.kind <= hr::gameplay::EntityKind::soldier &&
              actor.capture_progress > .9F && game.snapshot().dash_cooldown_seconds <= 0) ||
          actor.id == escaping) {
        const bool first_escape = !escaping;
        escaping = actor.id;
        control = {{game.snapshot().player.x - actor.position.x,
                       game.snapshot().player.z - actor.position.z},
            first_escape};
        break;
      }
    }
    game.advance(step, control);
    const auto& run = game.snapshot();
    if (run.phase == hr::gameplay::Phase::dead)
      break;
    for (const auto& actor : run.entities) {
      if (actor.kind > hr::gameplay::EntityKind::soldier)
        continue;
      if (selected && actor.id != selected)
        continue;
      if (actor.capture_progress <= 0 && !pending.contains(actor.id) && !selected)
        continue;
      CaptureSample sample{actor, run.player, run.runner_distance, run.stats.active_seconds,
          std::any_of(run.beams.begin(), run.beams.end(), [&](const auto& b) { return b.target_id == actor.id; })};
      if (selected) {
        result.samples.push_back(sample);
        if (result.samples.size() >= result.cancelled_at + 91)
          return result;
        continue;
      }
      auto& track = pending[actor.id];
      if (!track.empty() && track.back().actor.capture_progress > .9F && actor.capture_progress == 0) {
        result.cancelled_at = track.size();
        result.samples = std::move(track);
        result.samples.push_back(sample);
        selected = actor.id;
        break;
      }
      if (actor.capture_progress > 0)
        track.push_back(sample);
      else
        pending.erase(actor.id);
    }
    std::erase_if(pending, [&](const auto& entry) {
      return std::none_of(run.entities.begin(), run.entities.end(), [&](const auto& actor) { return actor.id == entry.first; });
    });
  }
  throw std::runtime_error("No bounded public-Game capture-loss trace found");
}
} // namespace hrqa
