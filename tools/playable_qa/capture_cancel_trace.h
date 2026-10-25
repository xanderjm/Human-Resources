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
  std::uint32_t selected{};
  constexpr float step = 1.F / 60;
  for (unsigned frame = 0; frame < 10800; ++frame) {
    if (game.snapshot().phase == hr::gameplay::Phase::choosing_upgrade)
      (void)game.choose_upgrade(0);
    game.advance(step);
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
      if (!track.empty() && track.back().actor.capture_progress >= .35F && actor.capture_progress == 0) {
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
