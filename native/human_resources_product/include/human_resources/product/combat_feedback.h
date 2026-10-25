#pragma once
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/presentation/style_scene.h>
#include <human_resources/ui/geometry.h>
#include <vector>

namespace human_resources::product {
// Shared by world geometry and actor-attached screen feedback. No HUD camera
// approximation: changes to the play camera must move both together.
void set_gameplay_camera(presentation::StyleFrame&, float aspect);
struct EnemyVitality final {
  std::uint32_t actor_id{};
  bool boss{};
  float fraction{};
  ui::Rect bounds;
};
// One boss and at most two damaged vehicles. Labels stay attached to the actual
// assembled silhouette; crowded/off-field labels are omitted, never clamped to
// an unrelated screen edge. All dimensions are viewport points.
[[nodiscard]] std::vector<EnemyVitality> enemy_vitality(const gameplay::Snapshot&,
    ui::Rect viewport, ui::Rect field, float boss_text_size);
[[nodiscard]] float enemy_hit_strength(const gameplay::Snapshot&, std::uint32_t actor_id,
    bool reduced_motion);
} // namespace human_resources::product
