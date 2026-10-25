#include "style_resources.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <shader_manifest.generated.h>
#include <vector>

namespace human_resources::renderer {
namespace {
std::vector<std::byte> read_file(const std::string& path, std::size_t limit) {
  SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
  if (io == nullptr) {
    SDL_Log("style asset open failed: %s: %s", path.c_str(), SDL_GetError());
    return {};
  }
  const Sint64 size = SDL_GetIOSize(io);
  if (size <= 0 || static_cast<Uint64>(size) > limit) {
    SDL_CloseIO(io);
    SDL_SetError("Style asset exceeds its byte budget: %s", path.c_str());
    return {};
  }
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  const bool valid = SDL_ReadIO(io, bytes.data(), bytes.size()) == bytes.size();
  SDL_CloseIO(io);
  if (!valid)
    bytes.clear();
  return bytes;
}
} // namespace
bool upload_style_mesh(SDL_GPUDevice* device, std::string_view path, StyleMesh& mesh) {
  const auto bytes = read_file(std::string(path), 80U * 1024U * 1024U);
  assets::PreviewMesh source;
  if (!assets::decode_preview_mesh(bytes, source)) {
    SDL_SetError("Invalid HRM1 mesh: %s", std::string(path).c_str());
    return false;
  }
  const auto& first = source.vertices.front().position;
  mesh.minimum = mesh.maximum = {first[0], first[1], first[2]};
  for (const auto& vertex : source.vertices) {
    mesh.minimum.x = std::min(mesh.minimum.x, vertex.position[0]);
    mesh.minimum.y = std::min(mesh.minimum.y, vertex.position[1]);
    mesh.minimum.z = std::min(mesh.minimum.z, vertex.position[2]);
    mesh.maximum.x = std::max(mesh.maximum.x, vertex.position[0]);
    mesh.maximum.y = std::max(mesh.maximum.y, vertex.position[1]);
    mesh.maximum.z = std::max(mesh.maximum.z, vertex.position[2]);
  }
  std::array<double, 3> total{};
  static const auto linear = [] {
    std::array<double, 256> values{};
    for (unsigned i = 0; i < values.size(); ++i) {
      const double channel = static_cast<double>(i) / 255.;
      values[i] = channel <= .04045 ? channel / 12.92 : std::pow((channel + .055) / 1.055, 2.4);
    }
    return values;
  }();
  for (std::size_t n = 0; n < source.rgba.size(); n += 4)
    for (unsigned c = 0; c < 3; ++c) {
      total[c] += linear[std::to_integer<unsigned>(source.rgba[n + c])];
    }
  for (unsigned c = 0; c < 3; ++c)
    mesh.average[c] = static_cast<float>(total[c] / static_cast<double>(source.rgba.size() / 4));
  const auto vb = static_cast<Uint32>(source.vertices.size() * sizeof(assets::PreviewVertex));
  const auto ib = static_cast<Uint32>(source.indices.size() * 4);
  const auto tb = static_cast<Uint32>(source.rgba.size());
  SDL_GPUBufferCreateInfo bi{};
  bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
  bi.size = vb;
  mesh.vertices = SDL_CreateGPUBuffer(device, &bi);
  bi.usage = SDL_GPU_BUFFERUSAGE_INDEX;
  bi.size = ib;
  mesh.indices = SDL_CreateGPUBuffer(device, &bi);
  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_2D;
  ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
  ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  ti.width = source.width;
  ti.height = source.height;
  ti.layer_count_or_depth = 1;
  ti.num_levels = 1;
  mesh.texture = SDL_CreateGPUTexture(device, &ti);
  mesh.count = static_cast<Uint32>(source.indices.size());
  if (!mesh.vertices || !mesh.indices || !mesh.texture)
    return false;
  SDL_GPUTransferBufferCreateInfo transfer_info{};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = vb + ib + tb;
  auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (!transfer)
    return false;
  auto* data = static_cast<std::byte*>(SDL_MapGPUTransferBuffer(device, transfer, false));
  if (!data) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  std::memcpy(data, source.vertices.data(), vb);
  std::memcpy(data + vb, source.indices.data(), ib);
  std::memcpy(data + vb + ib, source.rgba.data(), tb);
  SDL_UnmapGPUTransferBuffer(device, transfer);
  auto* command = SDL_AcquireGPUCommandBuffer(device);
  if (!command) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  auto* copy = SDL_BeginGPUCopyPass(command);
  if (!copy) {
    SDL_CancelGPUCommandBuffer(command);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  SDL_GPUTransferBufferLocation location{transfer, 0};
  SDL_GPUBufferRegion region{mesh.vertices, 0, vb};
  SDL_UploadToGPUBuffer(copy, &location, &region, false);
  location.offset = vb;
  region = {mesh.indices, 0, ib};
  SDL_UploadToGPUBuffer(copy, &location, &region, false);
  SDL_GPUTextureTransferInfo pixels{transfer, vb + ib, source.width, source.height};
  SDL_GPUTextureRegion image{};
  image.texture = mesh.texture;
  image.w = source.width;
  image.h = source.height;
  image.d = 1;
  SDL_UploadToGPUTexture(copy, &pixels, &image, false);
  SDL_EndGPUCopyPass(copy);
  const bool ok = SDL_SubmitGPUCommandBuffer(command);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  return ok;
}
void release_style_mesh(SDL_GPUDevice* device, StyleMesh& mesh) {
  if (mesh.vertices)
    SDL_ReleaseGPUBuffer(device, mesh.vertices);
  if (mesh.indices)
    SDL_ReleaseGPUBuffer(device, mesh.indices);
  if (mesh.texture)
    SDL_ReleaseGPUTexture(device, mesh.texture);
  mesh = {};
}
SDL_GPUShaderFormat style_shader_format() {
  const auto platform = generated::package_platform;
  if (platform == "macos" || platform == "ios")
    return SDL_GPU_SHADERFORMAT_METALLIB;
  return platform == "windows" ? SDL_GPU_SHADERFORMAT_DXIL : SDL_GPU_SHADERFORMAT_SPIRV;
}
SDL_GPUShader* style_shader(SDL_GPUDevice* device, bool overlay, bool vertex, bool shadow, bool product_ui) {
  const std::string name = product_ui ? "product_ui" : shadow ? "style_shadow" : overlay ? "style_overlay"
                                                             : "style_world";
  const std::string stage = vertex ? "vertex" : "fragment";
  const auto platform = generated::package_platform;
  const bool metal = platform == "macos" || platform == "ios";
  const bool dxil = platform == "windows";
  const std::string relative = metal ? "metal/" + std::string(platform) + "/" : dxil ? "dxil/"
                                                                                     : "spirv/";
  const char* base = SDL_GetBasePath();
  const auto suffix = std::filesystem::path(relative) / std::filesystem::path(name + "." + stage + (metal ? ".metallib" : dxil ? ".dxil" : ".spv"));
  auto path = std::string_view(SDL_GetPlatform()) == "Android" ? std::filesystem::path("shaders") / suffix : std::filesystem::path(base ? base : "") / "assets/shaders" / suffix;
  auto bytes = read_file(path.string(), 4U * 1024U * 1024U);
  if (bytes.empty()) {
    path = std::filesystem::path(base ? base : "") / "style-preview-shaders" / suffix;
    bytes = read_file(path.string(), 4U * 1024U * 1024U);
  }
  if (bytes.empty())
    return nullptr;
  const auto reflection = product_ui ? (vertex ? generated::product_ui_vertex : generated::product_ui_fragment) : shadow ? generated::style_shadow_fragment : overlay ? (vertex ? generated::style_overlay_vertex : generated::style_overlay_fragment)
                                                                              : (vertex ? generated::style_world_vertex : generated::style_world_fragment);
  SDL_GPUShaderCreateInfo info{};
  info.code = reinterpret_cast<const Uint8*>(bytes.data());
  info.code_size = bytes.size();
  info.entrypoint = vertex ? "vertex_main" : "fragment_main";
  info.format = metal ? SDL_GPU_SHADERFORMAT_METALLIB : dxil ? SDL_GPU_SHADERFORMAT_DXIL
                                                             : SDL_GPU_SHADERFORMAT_SPIRV;
  info.stage = vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
  info.num_samplers = reflection.samplers;
  info.num_uniform_buffers = reflection.uniform_buffers;
  info.num_storage_buffers = reflection.storage_buffers;
  info.num_storage_textures = reflection.storage_textures;
  auto* shader = SDL_CreateGPUShader(device, &info);
  if (!shader)
    SDL_Log("style shader failed: %s: %s", path.string().c_str(), SDL_GetError());
  return shader;
}
} // namespace human_resources::renderer
