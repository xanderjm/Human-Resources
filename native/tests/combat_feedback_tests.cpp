#include "test_support.h"
#include <human_resources/product/combat_feedback.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/product/visibility.h>
#include <algorithm>
#include <cmath>
#include <iostream>
namespace {
namespace hr = human_resources;
using test_support::require;
using test_support::require_near;
using Kind = hr::gameplay::EntityKind;
using Event = hr::gameplay::PresentationEventKind;
void exact_progress_and_engagement() {
  hr::gameplay::Snapshot run;
  run.stats.active_seconds = 10;
  run.entities = {{.id = 1, .kind = Kind::boss, .position = {-4, 12}, .hull = 120, .maximum_hull = 160},
      {.id = 2, .kind = Kind::tank, .position = {4, 2}, .hull = 100, .maximum_hull = 300},
      {.id = 3, .kind = Kind::jeep, .position = {-5, -4}, .hull = 20, .maximum_hull = 80},
      {.id = 4, .kind = Kind::soldier, .position = {3, 8}, .hull = 1, .maximum_hull = 10},
      {.id = 5, .kind = Kind::cow, .position = {1, 6}, .hull = 1, .maximum_hull = 20}};
  const hr::ui::Rect viewport{0, 0, 390, 844}, field{8, 80, 374, 650};
  auto values = hr::product::enemy_vitality(run, viewport, field, 14);
  require(values.size() == 1 && values[0].actor_id == 1, "only boss persists without actual recent engagement");
  require_near(values[0].fraction, .75F, 1e-6F, "boss progress is exact authoritative hull fraction");
  run.presentation_events[0] = {1, 9.9, Event::impact, 2, run.entities[1].position};
  run.beams.push_back({.target_id = 3, .target = run.entities[2].position, .vehicle = true});
  values = hr::product::enemy_vitality(run, viewport, field, 14);
  require(values.size() == 3 && values[1].actor_id == 3 && values[2].actor_id == 2,
      "actual beam target wins then recent impact; organics have no vitality clutter");
  require_near(values[2].fraction, 1.F / 3, 1e-6F, "vehicle meter uses real maximum hull");
  run.stats.active_seconds = 12.5;
  values = hr::product::enemy_vitality(run, viewport, field, 14);
  require(values.size() == 2 && values[1].actor_id == 3, "old impact expires in active time, beam target remains");
  run.entities[0].hull = 0;
  run.beams.clear();
  require(hr::product::enemy_vitality(run, viewport, field, 14).empty(), "dead/retired target cannot retain a progress label");
}
void attachment_boundaries_and_caps() {
  for (const auto viewport : {hr::ui::Rect{0, 0, 320, 568}, hr::ui::Rect{0, 0, 390, 844}, hr::ui::Rect{0, 0, 844, 390}})
    for (float text : {14.F, 28.F}) {
      const hr::ui::Rect field{8, 80, viewport.width - 16, viewport.height - 180};
      for (float x : {-7.5F, -4.F, 0.F, 4.F, 7.5F})
        for (float z : {-17.F, -5.F, 5.F, 16.5F}) {
          hr::gameplay::Snapshot run;
          run.entities = {{.id = 9, .kind = Kind::boss, .position = {x, z}, .hull = 33, .maximum_hull = 160}};
          const auto result = hr::product::enemy_vitality(run, viewport, field, text);
          for (const auto& bar : result) {
            require(bar.bounds.x >= field.x && bar.bounds.y >= field.y && bar.bounds.right() <= field.right() && bar.bounds.bottom() <= field.bottom(),
                "all labels remain inside the actual safe tactical field");
            const auto scene = hr::product::scene(run, viewport.width / viewport.height, true);
            float body_bottom = -1000, body_top = 1000;
            for (const auto& part : scene.instances)
              if (part.mesh == "tank-hull" || part.mesh == "tank-turret") {
                const auto region = hr::product::project_region(hr::presentation::multiply(scene.camera, hr::presentation::model_matrix(part)), *hr::product::visibility_mesh_bounds(part.mesh));
                body_bottom = std::max(body_bottom, (region.bottom + 1) * .5F * viewport.height);
                body_top = std::min(body_top, (region.top + 1) * .5F * viewport.height);
              }
            require(std::abs(bar.bounds.y - body_bottom - 4) < .01F || std::abs(bar.bounds.bottom() - body_top + 4) < .01F,
                "meter is attached to an actual source-body edge, never edge-clamped");
          }
        }
    }
  hr::gameplay::Snapshot run;
  run.stats.active_seconds = 1;
  for (unsigned i = 0; i < 128; ++i) {
    run.entities.push_back({.id = i + 1, .kind = Kind::tank, .position = {static_cast<float>(i % 8) - 4, static_cast<float>(i / 8) - 8}, .hull = 10, .maximum_hull = 300});
    if (i < run.presentation_events.size()) run.presentation_events[i] = {i + 1, 1, Event::impact, i + 1, {}};
  }
  const auto values = hr::product::enemy_vitality(run, {0, 0, 390, 844}, {8, 80, 374, 650}, 14);
  require(values.size() <= 2, "128-actor public fixture cannot add more than two vehicle meters");
  for (std::size_t i = 0; i < values.size(); ++i)
    for (std::size_t j = i + 1; j < values.size(); ++j) {
      const auto overlap = hr::ui::intersect(values[i].bounds, values[j].bounds);
      require(overlap.width <= 0 || overlap.height <= 0, "nearby labels do not stack over each other");
    }
}
void confirmed_hits_and_reduced_motion() {
  hr::gameplay::Snapshot run;
  run.stats.active_seconds = 3;
  run.entities.push_back({.id = 7, .kind = Kind::tank, .position = {5, 5}, .hull = 100, .maximum_hull = 300});
  require(hr::product::enemy_hit_strength(run, 7, false) == 0, "hull state alone cannot invent a hit flash");
  run.presentation_events[0] = {1, 3, Event::impact, 7, {3, 5}};
  require(hr::product::enemy_hit_strength(run, 7, false) == 1, "confirmed exact actor impact starts brief response");
  require(hr::product::enemy_hit_strength(run, 8, false) == 0, "impact cannot flash an unrelated actor");
  require(hr::product::enemy_hit_strength(run, 7, true) == 0, "reduced motion removes bright body flashing");
  const auto normal = hr::product::scene(run, 390.F / 844.F, false);
  const auto reduced = hr::product::scene(run, 390.F / 844.F, true);
  for (const auto& part : normal.instances)
    if (part.mesh == "tank-hull") require(part.tint[0] > 1, "actual enemy body consumes the committed hit response");
  for (const auto& part : reduced.instances)
    if (part.mesh == "tank-hull") require(part.tint[0] == 1, "reduced-motion enemy retains its authored material");
  const auto before = hr::product::enemy_vitality(run, {0, 0, 390, 844}, {8, 80, 374, 650}, 14);
  const auto paused = hr::product::enemy_vitality(run, {0, 0, 390, 844}, {8, 80, 374, 650}, 14);
  require(before.size() == 1 && paused.size() == before.size() && paused[0].fraction == before[0].fraction,
      "silent/reduced-motion and repeated paused snapshots preserve real damage progress");
  run.stats.active_seconds = 3.2;
  require(hr::product::enemy_hit_strength(run, 7, false) == 0, "body response ends after 160 active milliseconds");
}
void capture_feedback_replaces_ground_vitality() {
  // Synthetic valid presentation fixture: a damaged beam-owned vehicle has
  // started lifting. Its actual scene pose no longer matches a ground meter.
  for (const auto kind : {Kind::jeep, Kind::tank}) {
    hr::gameplay::Snapshot run;
    run.stats.active_seconds = 10;
    run.entities.push_back({.id = 7, .kind = kind, .position = {-5, -4}, .hull = 20, .maximum_hull = 80});
    run.presentation_events[0] = {1, 9.9, Event::impact, 7, run.entities[0].position};
    run.beams.push_back({.target_id = 7, .target = run.entities[0].position, .vehicle = true});
    const hr::ui::Rect viewport{0, 0, 390, 844}, field{8, 80, 374, 650};
    require(hr::product::enemy_vitality(run, viewport, field, 14).size() == 1,
        "grounded damaged vehicle begins with real engaged vitality");
    for (const float progress : {.01F, .2F, .6F, .99F}) {
      run.entities[0].capture_progress = progress;
      require(hr::product::enemy_vitality(run, viewport, field, 14).empty(),
          "capture progress owns lifted vehicle feedback; no stale ground-anchored vitality");
      if (progress == .6F) {
        const auto scene = hr::product::scene(run, viewport.width / viewport.height, false);
        const auto mesh = kind == Kind::jeep ? "jeep-chassis" : "tank-hull";
        const auto body = std::find_if(scene.instances.begin(), scene.instances.end(), [&](const auto& part) { return part.mesh == mesh; });
        require(body != scene.instances.end() && body->position.y > .1F,
            "synthetic capture fixture exercises the actual lifted vehicle source pose");
      }
    }
    run.entities[0].capture_progress = 0;
    run.beams.clear();
    require(hr::product::enemy_vitality(run, viewport, field, 14).size() == 1,
        "cancelled capture restores grounded vitality while actual recent impact remains");
    run.stats.active_seconds = 12.5;
    require(hr::product::enemy_vitality(run, viewport, field, 14).empty(),
        "cancelled vehicle vitality still obeys original engagement expiry");
  }
}
} // namespace
int main() {
  exact_progress_and_engagement();
  attachment_boundaries_and_caps();
  confirmed_hits_and_reduced_motion();
  capture_feedback_replaces_ground_vitality();
  std::cout << "combat feedback contracts passed\n";
}
