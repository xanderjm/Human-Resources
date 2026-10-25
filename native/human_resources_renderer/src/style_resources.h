#pragma once
#include <SDL3/SDL_gpu.h>
#include <array>
#include <human_resources/assets/preview_mesh.h>
#include <human_resources/presentation/style_scene.h>
#include <string_view>
namespace human_resources::renderer {
struct StyleMesh final {
  SDL_GPUBuffer* vertices{};
  SDL_GPUBuffer* indices{};
  SDL_GPUTexture* texture{};
  Uint32 count{};
  std::array<float, 3> average{1, 1, 1};
  presentation::Vec3 minimum, maximum;
};
[[nodiscard]] bool upload_style_mesh(SDL_GPUDevice* device, std::string_view path, StyleMesh& mesh);
void release_style_mesh(SDL_GPUDevice* device, StyleMesh& mesh);
[[nodiscard]] SDL_GPUShaderFormat style_shader_format();
[[nodiscard]] SDL_GPUShader* style_shader(SDL_GPUDevice* device, bool overlay, bool vertex, bool shadow = false, bool product_ui = false);
} // namespace human_resources::renderer
