#include <human_resources/assets/preview_mesh.h>

#include <bit>
#include <cmath>
#include <cstring>
#include <utility>
namespace human_resources::assets {
namespace {
std::uint32_t u32(std::span<const std::byte> b, std::size_t p) {
  std::uint32_t v = 0;
  for (unsigned i = 0; i < 4; ++i)
    v |= std::to_integer<std::uint32_t>(b[p + i]) << (i * 8);
  return v;
}
} // namespace
bool decode_preview_mesh(std::span<const std::byte> b, PreviewMesh& output) {
  output = {};
  if (b.size() < 20 || std::memcmp(b.data(), "HRM1", 4) != 0)
    return false;
  const auto nv = u32(b, 4), ni = u32(b, 8), w = u32(b, 12), h = u32(b, 16);
  if (nv == 0 || nv > 1000000 || ni == 0 || ni > 3000000 || ni % 3 != 0 || w == 0 || h == 0 || w > 2048 || h > 2048)
    return false;
  const std::size_t vertex_bytes = static_cast<std::size_t>(nv) * 56;
  const std::size_t index_bytes = static_cast<std::size_t>(ni) * 4;
  const std::size_t image_bytes = static_cast<std::size_t>(w) * h * 4;
  if (b.size() != 20 + vertex_bytes + index_bytes + image_bytes)
    return false;
  PreviewMesh mesh;
  mesh.width = w;
  mesh.height = h;
  mesh.vertices.resize(nv);
  mesh.indices.resize(ni);
  // Decode scalars explicitly: the file is little endian on every selected host.
  for (std::size_t n = 0; n < nv; ++n) {
    std::array<float, 14> fields{};
    for (std::size_t k = 0; k < 14; ++k) {
      fields[k] = std::bit_cast<float>(u32(b, 20 + n * 56 + k * 4));
      if (!std::isfinite(fields[k]))
        return false;
    }
    std::memcpy(&mesh.vertices[n], fields.data(), 56);
  }
  for (std::size_t n = 0; n < ni; ++n) {
    mesh.indices[n] = u32(b, 20 + vertex_bytes + n * 4);
    if (mesh.indices[n] >= nv)
      return false;
  }
  const auto pixels = b.subspan(20 + vertex_bytes + index_bytes);
  mesh.rgba.assign(pixels.begin(), pixels.end());
  output = std::move(mesh);
  return true;
}
} // namespace human_resources::assets
