#pragma once
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/presentation/style_scene.h>
namespace human_resources::product {
class SceneEffects;
struct ShipPose final {
  presentation::Vec3 position;
  float scale{1}, bank{}, pitch{};
};
// One rigid hull transform owns every real module and capture intake socket.
[[nodiscard]] presentation::Vec3 ship_socket(ShipPose, presentation::Vec3 local);
// Existing source modules assembled at fixed hull sockets. Render-only; never
// modifies modules, targeting, damage, capture or encounter clocks.
void append_ship_gear(presentation::StyleFrame&, const gameplay::Snapshot&, ShipPose, bool reduced_motion);
void append_gear_shots(presentation::StyleFrame&, const gameplay::Snapshot&, ShipPose, float runner_distance, bool reduced_motion);
void append_gear_effects(presentation::StyleFrame&, const gameplay::Snapshot&, const SceneEffects*, ShipPose, float runner_distance, bool reduced_motion);
} // namespace human_resources::product
