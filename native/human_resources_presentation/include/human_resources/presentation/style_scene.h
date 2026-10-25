#pragma once
#include <array>
#include <human_resources/presentation/actor_animation.h>
#include <cstdint>
#include <string_view>
#include <vector>

namespace human_resources::presentation {
struct Vec3 final {
  float x{}, y{}, z{};
};
using Matrix = std::array<float, 16>;
enum class AttachmentSocket : std::uint8_t { head, weapon };
struct StyleInstance final {
  std::string_view mesh;
  Vec3 position;
  Vec3 scale{1, 1, 1};
  Vec3 rotation; // radians, XYZ; +Y up, +Z forward, metres
  std::array<float, 4> tint{1, 1, 1, 1};
  float texture_repeat{1};
  bool effect{};
  bool shield_rim{}; // Unit sphere: view-dependent alpha, front surface only.
  bool polished_hull{};   // Restrained enamel response; authored mint emission survives.
  bool damage_reactive{}; // Actual player hull/modules only; not drones or afterimages.
  bool casts_shadow{true};
  float texture_detail{1};
  std::uint64_t stable_id{};
  ActorAnimation animation{};
  std::uint64_t attachment_parent_id{};
  Matrix attachment_local{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  AttachmentSocket attachment_socket{AttachmentSocket::head};
  // Presentation-only launch correction decays over a short initial segment.
  // The authoritative projectile's position/direction remain the world model.
  std::uint64_t launch_parent_id{};
  std::uint64_t launch_id{};
  Vec3 launch_local, launch_origin;
  float launch_bridge{}, launch_elevation{};
};
struct SurfaceVertex final {
  Vec3 position;
  Vec3 normal{0, 1, 0};
  std::array<float, 2> uv;
};
struct StyleSurface final {
  std::string_view material;
  std::vector<SurfaceVertex> vertices; // triangle list; bounded by renderer upload capacity
  std::array<float, 4> tint{1, 1, 1, 1};
  float texture_detail{.24F};
};
struct ScreenQuad final {
  std::array<float, 4> rect; // normalized x,y,width,height
  std::array<float, 4> uv{0, 0, 1, 1};
  std::array<float, 4> color{1, 1, 1, 1};
};
struct StyleFrame final {
  // Available normalized output rectangle. The shared integer fit supplies the
  // actual destination; native UI and semantic input still use the full output.
  std::array<float, 4> world_destination{0, 0, 1, 1};
  std::vector<ScreenQuad> screen_quads;
  std::vector<StyleInstance> instances;
  std::vector<StyleSurface> surfaces;
  Matrix camera;
  Matrix light;
  float ring_angle{};
  float damage_flash{}; // Signed committed feedback: positive hull, negative shield.
  float active_delta_seconds{};
  unsigned location{};
  bool show_overlay{true};
};
// Authored visual choreography only. No captures, damage or progression rules.
[[nodiscard]] StyleFrame style_frame(float seconds, float aspect);
[[nodiscard]] Matrix model_matrix(const StyleInstance& instance);
void set_overhead_camera(StyleFrame& frame, float aspect, float width, Vec3 focus = {}, float setback = 27);
[[nodiscard]] Matrix multiply(const Matrix& a, const Matrix& b);
[[nodiscard]] bool bounds_visible(const Matrix& clip_from_model, Vec3 minimum, Vec3 maximum, float margin = 1);
} // namespace human_resources::presentation
