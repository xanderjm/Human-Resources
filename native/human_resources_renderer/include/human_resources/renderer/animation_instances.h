#pragma once
#include <cstdint>
#include <human_resources/animation/animation.h>
#include <human_resources/assets/preview_mesh.h>
#include <human_resources/presentation/actor_animation.h>
#include <human_resources/presentation/style_scene.h>
#include <memory>
#include <span>
#include <string_view>

namespace human_resources::renderer {
using AnimationIntent = presentation::ActorAnimation;
struct AnimatedActor final {
  std::span<const assets::PreviewVertex> vertices;
  animation::Vec3 minimum{}, maximum{};
  animation::Matrix head_delta{}; // posed socket * inverse(rest socket)
  animation::Matrix weapon_delta{};
  animation::Matrix launch_weapon_delta{};
  std::uint64_t launch_sequence{};
  std::string_view clip;
  float clip_time{};
  std::uint64_t revision{};
  bool has_head{};
  bool has_weapon{};
};
// Explicit boundary: animation uses column-major glTF matrices; presentation
// model/camera matrices and shader upload contract use row-major storage.
[[nodiscard]] presentation::Matrix head_attachment_matrix(const AnimatedActor& actor);
[[nodiscard]] presentation::Matrix weapon_attachment_matrix(const AnimatedActor& actor);
// Renderer-owned presentation state; contains no SDL or gameplay types.
// Asset loads and actor spawns allocate; steady-state update does not.
class AnimationInstances final {
public:
  static constexpr std::size_t actor_capacity = 128, asset_capacity = 8;
  AnimationInstances();
  ~AnimationInstances();
  AnimationInstances(const AnimationInstances&) = delete;
  AnimationInstances& operator=(const AnimationInstances&) = delete;
  [[nodiscard]] bool load(std::string_view id, std::span<const std::byte> hra,
      std::span<const assets::PreviewVertex> companion_vertices);
  [[nodiscard]] bool contains(std::string_view id) const;
  void begin_frame();
  // Zero delta freezes an existing player, including clip transitions.
  [[nodiscard]] const AnimatedActor* update(std::uint64_t stable_id, std::string_view asset,
      const AnimationIntent& intent, float active_delta_seconds);
  void end_frame();    // discard actors absent from this frame
  void clear_actors(); // new run/restore; prepared assets stay loaded
  [[nodiscard]] std::size_t size() const;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace human_resources::renderer
