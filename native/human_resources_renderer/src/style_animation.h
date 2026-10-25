#pragma once
#include "style_resources.h"
#include <human_resources/renderer/animation_instances.h>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
namespace human_resources::renderer {
// GPU ownership stays here, while the testable actor cache has no SDL.
class StyleAnimationStream final {
public:
  // Startup-only bounded pair I/O/decode. Frames only query prepared assets.
  [[nodiscard]] bool preload(std::span<const std::string_view> ids, std::string_view content_root);
  [[nodiscard]] bool contains(std::string_view id) const;
  [[nodiscard]] bool stage(SDL_GPUDevice* device, SDL_GPUCommandBuffer* command,
      const presentation::StyleFrame& frame);
  [[nodiscard]] const StyleMesh* mesh(std::uint64_t id, const StyleMesh& base);
  [[nodiscard]] const presentation::Matrix* head_delta(std::uint64_t id) const;
  [[nodiscard]] const presentation::Matrix* weapon_delta(std::uint64_t id) const;
  [[nodiscard]] const presentation::Vec3* projectile_launch(std::uint64_t id) const;
  void commit(); // call only after the staged command was submitted
  void release(SDL_GPUDevice* device);

private:
  [[nodiscard]] bool load(std::string_view id, std::string_view content_root);
  struct Stream final {
    SDL_GPUBuffer* buffer{};
    SDL_GPUTransferBuffer* transfer{};
    StyleMesh mesh;
    presentation::Matrix head{};
    presentation::Matrix weapon{};
    std::string asset;
    Uint32 byte_capacity{};
    const AnimatedActor* actor{};
    std::uint64_t revision{};
    std::uint64_t pending_revision{};
    bool seen{};
  };
  AnimationInstances instances_;
  std::map<std::uint64_t, Stream> streams_;
  struct Launch final { std::uint64_t id{}; presentation::Vec3 position; bool seen{}; };
  std::array<Launch,96> launches_{};
};
} // namespace human_resources::renderer
