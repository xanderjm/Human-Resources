#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace human_resources::assets {
struct PreviewVertex final {
  std::array<float, 3> position;
  std::array<float, 3> normal;
  std::array<float, 2> uv;
  std::array<float, 4> color;
  float emission;
  float rotor;
};
static_assert(sizeof(PreviewVertex) == 56);
struct PreviewMesh final {
  std::vector<PreviewVertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<std::byte> rgba;
  std::uint32_t width{}, height{};
};
// HRM1 is an offline development format. Rejects oversized/truncated/nonfinite
// data and invalid indices before allocation or upload; leaves output empty.
[[nodiscard]] bool decode_preview_mesh(std::span<const std::byte> bytes, PreviewMesh& output);
} // namespace human_resources::assets
