#include <human_resources/product/combat_feedback.h>
#include <human_resources/product/visibility.h>
#include "vehicle_assembly.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <product_glyphs.generated.h>

namespace human_resources::product {
namespace {
ui::Rect pixels(ProjectedRegion region, ui::Rect viewport) {
  return {viewport.x + (region.left + 1) * .5F * viewport.width,
      viewport.y + (region.top + 1) * .5F * viewport.height,
      (region.right - region.left) * .5F * viewport.width,
      (region.bottom - region.top) * .5F * viewport.height};
}
bool overlaps(ui::Rect a, ui::Rect b) {
  const auto intersection = ui::intersect(a, b);
  return intersection.width > 0 && intersection.height > 0;
}
bool inside(ui::Rect a, ui::Rect b) {
  return a.finite() && a.x >= b.x && a.y >= b.y && a.right() <= b.right() && a.bottom() <= b.bottom();
}
float impact_age(const gameplay::Snapshot& run, std::uint32_t actor_id) {
  float age = std::numeric_limits<float>::infinity();
  for (const auto& event : run.presentation_events)
    if (event.sequence && event.actor_id == actor_id && event.kind == gameplay::PresentationEventKind::impact) {
      const auto elapsed = static_cast<float>(run.stats.active_seconds - event.active_seconds);
      if (elapsed >= 0) age = std::min(age, elapsed);
    }
  return age;
}
ui::Rect silhouette(const gameplay::EntityView& actor, const gameplay::Snapshot& run,
    const presentation::Matrix& camera, ui::Rect viewport) {
  presentation::StyleFrame assembled;
  assembled.instances.reserve(9);
  const float scale = actor.kind == gameplay::EntityKind::boss ? 1.6F : 1.F;
  const presentation::StyleInstance root{.position = {actor.position.x, 0, run.runner_distance - actor.position.z + 2.3F},
      .scale = {scale, scale, scale}, .rotation = {0, gameplay::vehicle_body_yaw(actor), 0}};
  if (actor.kind == gameplay::EntityKind::jeep)
    detail::append_jeep(assembled, root, actor, actor.warning_seconds > 0 ? actor.warning_aim : run.player, 0, -1);
  else {
    const auto* boss = run.boss_combat && run.boss_combat->actor_id == actor.id ? &*run.boss_combat : nullptr;
    detail::append_tank(assembled, root, actor, actor.warning_seconds > 0 ? actor.warning_aim : run.player, -1,
        boss);
    // Vitality placement is shared by both motion preferences. Bound both
    // current physical poses so its label never covers the snapped variant.
    if (boss && boss->plan)
      detail::append_tank(assembled, root, actor, actor.warning_seconds > 0 ? actor.warning_aim : run.player, -1,
          boss, true);
  }
  const float inf = std::numeric_limits<float>::infinity();
  ProjectedRegion total{inf, inf, -inf, -inf, inf, -inf};
  for (const auto& part : assembled.instances)
    if (const auto bounds = visibility_mesh_bounds(part.mesh)) {
      const auto region = project_region(presentation::multiply(camera, presentation::model_matrix(part)), *bounds);
      total.left = std::min(total.left, region.left);
      total.top = std::min(total.top, region.top);
      total.right = std::max(total.right, region.right);
      total.bottom = std::max(total.bottom, region.bottom);
    }
  return pixels(total, viewport);
}
} // namespace
void set_gameplay_camera(presentation::StyleFrame& frame, float aspect) {
  frame.camera = gameplay_framing(aspect).camera;
}
std::vector<EnemyVitality> enemy_vitality(const gameplay::Snapshot& run,
    ui::Rect viewport, ui::Rect field, float boss_text_size) {
  return enemy_vitality(run, gameplay_framing(viewport), field, boss_text_size);
}
std::vector<EnemyVitality> enemy_vitality(const gameplay::Snapshot& run,
    const GameplayFraming& framing, ui::Rect field, float boss_text_size) {
  const auto viewport = framing.stage;
  std::vector<EnemyVitality> result;
  result.reserve(3);
  if (!viewport.finite() || viewport.width <= 0 || viewport.height <= 0 || !field.finite()) return result;
  presentation::StyleFrame projection;
  projection.camera = framing.camera;
  const presentation::StyleInstance ship{.position = {run.player.x, 2.5F, run.runner_distance - run.player.z + 2.3F},
      .scale = {1.12F / .75F, 1.12F / .75F, 1.12F / .75F}};
  auto ship_region = pixels(project_region(presentation::multiply(projection.camera, presentation::model_matrix(ship)),
      *visibility_mesh_bounds("ufo-modular")), viewport);
  // Covers the bounded bank/pitch and sockets without moving health feedback
  // away from its owner when the saucer flies over a target.
  ship_region = {ship_region.x - 10, ship_region.y - 10, ship_region.width + 20, ship_region.height + 20};
  struct Candidate { const gameplay::EntityView* actor{}; bool beamed{}; float impact_age{}; };
  std::array<Candidate, 128> candidates{};
  std::size_t count = 0;
  for (const auto& actor : run.entities) {
    const bool boss = actor.kind == gameplay::EntityKind::boss;
    const bool vehicle = actor.kind == gameplay::EntityKind::jeep || actor.kind == gameplay::EntityKind::tank;
    if ((!boss && !vehicle) || !std::isfinite(actor.hull) || !std::isfinite(actor.maximum_hull) ||
        actor.maximum_hull <= 0 || actor.hull <= 0 || actor.capture_progress > 0 || (!boss && actor.hull >= actor.maximum_hull)) continue;
    // Capture owns the feedback once lift starts. These meters are attached to
    // the grounded source silhouette, not the animated capture pose.
    const bool beamed = std::any_of(run.beams.begin(), run.beams.end(), [&](const auto& beam) { return beam.target_id == actor.id; });
    const float age = impact_age(run, actor.id);
    if (!boss && !beamed && age > 2.5F) continue;
    if (count < candidates.size()) candidates[count++] = {&actor, beamed, age};
  }
  const auto distance = [&](const auto* actor) { return std::hypot(actor->position.x - run.player.x, actor->position.z - run.player.z); };
  std::sort(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(count), [&](const auto& a, const auto& b) {
    if ((a.actor->kind == gameplay::EntityKind::boss) != (b.actor->kind == gameplay::EntityKind::boss)) return a.actor->kind == gameplay::EntityKind::boss;
    if (a.beamed != b.beamed) return a.beamed;
    if (a.impact_age != b.impact_age) return a.impact_age < b.impact_age;
    if (distance(a.actor) != distance(b.actor)) return distance(a.actor) < distance(b.actor);
    return a.actor->id < b.actor->id;
  });
  unsigned vehicles = 0;
  bool has_boss = false;
  for (std::size_t i = 0; i < count; ++i) {
    const auto& actor = *candidates[i].actor;
    const bool boss = actor.kind == gameplay::EntityKind::boss;
    if ((boss && has_boss) || (!boss && vehicles == 2)) continue;
    const auto body = silhouette(actor, run, projection.camera, viewport);
    const bool stacked = boss && std::max(112.F, boss_text_size * 6.6F) > field.width;
    const auto measure = [&](std::string_view text) {
      float width = 0;
      for (char ch : text) width += glyph_advances[static_cast<unsigned>(ch - 32)] * boss_text_size / 48.F;
      return width;
    };
    const float width = boss ? (stacked ? std::max(measure("BOSS"), measure("100%")) + 8 : std::max(112.F, boss_text_size * 6.6F)) : 42.F;
    const float height = boss ? boss_text_size * (stacked ? 2.F : 1.F) + 18 : 8.F;
    const float x = body.x + (body.width - width) * .5F;
    const ui::Rect above{x, body.y - height - 4, width, height}, below{x, body.bottom() + 4, width, height};
    // Boss identification normally sits above its large silhouette; entering
    // under the HUD can use the opposite edge. Both positions touch this body.
    for (const auto bounds : boss ? std::array{above, below} : std::array{below, above}) {
      if (!inside(bounds, field) || overlaps(bounds, ship_region)) continue;
      bool crowded = false;
      for (const auto& existing : result)
        crowded |= overlaps(bounds, {existing.bounds.x - 4, existing.bounds.y - 4, existing.bounds.width + 8, existing.bounds.height + 8});
      if (crowded) continue;
      result.push_back({actor.id, boss, std::clamp(actor.hull / actor.maximum_hull, 0.F, 1.F), bounds, stacked});
      has_boss |= boss;
      vehicles += !boss;
      break;
    }
  }
  return result;
}
float enemy_hit_strength(const gameplay::Snapshot& run, std::uint32_t actor_id, bool reduced_motion) {
  if (reduced_motion) return 0;
  float response = 0;
  for (const auto& event : run.presentation_events)
    if (event.sequence && event.actor_id == actor_id && event.kind == gameplay::PresentationEventKind::impact) {
      const auto age = static_cast<float>(run.stats.active_seconds - event.active_seconds);
      if (age >= 0 && age < .16F) response = std::max(response, 1 - age / .16F);
    }
  return response;
}
} // namespace human_resources::product
