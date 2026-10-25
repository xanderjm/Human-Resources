#pragma once
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/presentation/style_scene.h>
#include <optional>
namespace human_resources::product {
struct VisibilityBounds final {
  presentation::Vec3 minimum, maximum;
};
struct ProjectedRegion final {
  float left{}, top{}, right{}, bottom{}, nearest{}, farthest{};
};
[[nodiscard]] std::optional<VisibilityBounds> visibility_mesh_bounds(std::string_view mesh);
[[nodiscard]] ProjectedRegion project_region(const presentation::Matrix&, const VisibilityBounds&);
// Scenery prefix only. Real actor/gear/projectile silhouettes and committed
// warning paths drive conservative, continuous spatial fading. No history,
// gameplay mutation, wall clock, inferred outcomes or renderer dependency.
void preserve_play_visibility(presentation::StyleFrame&, std::size_t scenery_count, const gameplay::Snapshot* = nullptr);
} // namespace human_resources::product
