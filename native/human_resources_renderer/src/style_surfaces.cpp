#include "style_surfaces.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <cstring>
namespace human_resources::renderer {
bool StyleSurfaceStream::initialize(SDL_GPUDevice* device) {
  SDL_GPUBufferCreateInfo info{};
  info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
  info.size = vertex_capacity * sizeof(assets::PreviewVertex);
  buffer = SDL_CreateGPUBuffer(device, &info);
  SDL_GPUTransferBufferCreateInfo upload{};
  upload.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  upload.size = info.size;
  transfer = SDL_CreateGPUTransferBuffer(device, &upload);
  packed.reserve(vertex_capacity);
  ranges.reserve(32);
  return buffer && transfer;
}
bool StyleSurfaceStream::stage(SDL_GPUDevice* device, SDL_GPUCommandBuffer* command, std::span<const presentation::StyleSurface> surfaces) {
  packed.clear();
  ranges.clear();
  if (surfaces.size() > 32) {
    SDL_SetError("Scenery surface material budget exceeded");
    return false;
  }
  for (const auto& surface : surfaces) {
    if (surface.vertices.size() % 3 != 0 || surface.vertices.size() > vertex_capacity - packed.size()) {
      SDL_SetError("Scenery surface vertex budget exceeded");
      return false;
    }
    ranges.push_back({static_cast<Uint32>(packed.size()), static_cast<Uint32>(surface.vertices.size()), surface.material, surface.tint, surface.texture_detail});
    for (const auto& v : surface.vertices) {
      const assets::PreviewVertex p{{v.position.x, v.position.y, v.position.z}, {v.normal.x, v.normal.y, v.normal.z}, v.uv, {1, 1, 1, 1}, 0, 0};
      for (float f : std::array{v.position.x, v.position.y, v.position.z, v.uv[0], v.uv[1]})
        if (!std::isfinite(f)) {
          SDL_SetError("Nonfinite scenery surface");
          return false;
        }
      packed.push_back(p);
    }
  }
  if (packed.empty())
    return true;
  auto* mapped = SDL_MapGPUTransferBuffer(device, transfer, true);
  if (!mapped)
    return false;
  const auto bytes = static_cast<Uint32>(packed.size() * sizeof(assets::PreviewVertex));
  std::memcpy(mapped, packed.data(), bytes);
  SDL_UnmapGPUTransferBuffer(device, transfer);
  auto* copy = SDL_BeginGPUCopyPass(command);
  if (!copy)
    return false;
  const SDL_GPUTransferBufferLocation from{transfer, 0};
  const SDL_GPUBufferRegion to{buffer, 0, bytes};
  SDL_UploadToGPUBuffer(copy, &from, &to, true);
  SDL_EndGPUCopyPass(copy);
  return true;
}
void StyleSurfaceStream::release(SDL_GPUDevice* device) {
  if (buffer)
    SDL_ReleaseGPUBuffer(device, buffer);
  if (transfer)
    SDL_ReleaseGPUTransferBuffer(device, transfer);
  buffer = nullptr;
  transfer = nullptr;
}
} // namespace human_resources::renderer
