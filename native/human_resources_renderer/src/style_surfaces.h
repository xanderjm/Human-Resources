#pragma once
#include <SDL3/SDL_gpu.h>
#include <human_resources/assets/preview_mesh.h>
#include <human_resources/presentation/style_scene.h>
#include <span>
namespace human_resources::renderer {
struct SurfaceRange final {
  Uint32 first{}, count{};
  std::string_view material;
  std::array<float, 4> tint;
  float detail{};
};
// One bounded, cycling upload owner for streamed road/ground geometry.
struct StyleSurfaceStream final {
  static constexpr Uint32 vertex_capacity = 65536;
  SDL_GPUBuffer* buffer{};
  SDL_GPUTransferBuffer* transfer{};
  std::vector<assets::PreviewVertex> packed;
  std::vector<SurfaceRange> ranges;
  bool initialize(SDL_GPUDevice* device);
  bool stage(SDL_GPUDevice* device, SDL_GPUCommandBuffer* command, std::span<const presentation::StyleSurface> surfaces);
  void release(SDL_GPUDevice* device);
};
} // namespace human_resources::renderer
