#include "pose_math.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <human_resources/animation/animation.h>
#include <utility>

namespace human_resources::animation {
namespace {
class Reader final {
public:
  explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}
  bool take(std::size_t n) {
    if (n > bytes_.size() - offset_) {
      okay_ = false;
      return false;
    }
    offset_ += n;
    return true;
  }
  std::uint32_t u32() {
    if (!take(4))
      return 0;
    std::uint32_t result = 0;
    for (unsigned k = 0; k < 4; ++k)
      result |= std::to_integer<std::uint32_t>(bytes_[offset_ - 4 + k]) << (8 * k);
    return result;
  }
  std::uint16_t u16() {
    if (!take(2))
      return 0;
    return static_cast<std::uint16_t>(std::to_integer<unsigned>(bytes_[offset_ - 2]) |
                                      (std::to_integer<unsigned>(bytes_[offset_ - 1]) << 8));
  }
  float scalar() {
    return std::bit_cast<float>(u32());
  }
  template <std::size_t N>
  std::array<float, N> floats() {
    std::array<float, N> result{};
    for (float& value : result)
      value = scalar();
    return result;
  }
  Transform transform() {
    return {floats<3>(), floats<4>(), floats<3>()};
  }
  bool name(std::string& output) {
    if (!take(32))
      return false;
    bool ended = false;
    for (std::size_t k = offset_ - 32; k < offset_; ++k) {
      const char c = static_cast<char>(std::to_integer<unsigned char>(bytes_[k]));
      if (c == 0)
        ended = true;
      else if (ended || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                            (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-'))
        return false;
      else
        output += c;
    }
    return ended && !output.empty();
  }
  [[nodiscard]] bool okay() const {
    return okay_;
  }
  [[nodiscard]] std::size_t offset() const {
    return offset_;
  }

private:
  std::span<const std::byte> bytes_;
  std::size_t offset_{};
  bool okay_{true};
};
bool same_pose(const Transform& a, const Transform& b) {
  float dot = 0;
  for (std::size_t k = 0; k < 4; ++k)
    dot += a.rotation[k] * b.rotation[k];
  if (std::abs(dot) < .99999F)
    return false;
  for (std::size_t k = 0; k < 3; ++k)
    if (std::abs(a.translation[k] - b.translation[k]) > .0001F || std::abs(a.scale[k] - b.scale[k]) > .0001F)
      return false;
  return true;
}
bool valid_vertex(const SkinVertex& vertex, std::size_t joints) {
  float total = 0, normal_length = 0;
  for (std::size_t k = 0; k < 3; ++k) {
    if (!std::isfinite(vertex.position[k]) || std::abs(vertex.position[k]) > 10000 || !std::isfinite(vertex.normal[k]))
      return false;
    normal_length += vertex.normal[k] * vertex.normal[k];
  }
  if (std::abs(normal_length - 1) > .002F)
    return false;
  for (std::size_t k = 0; k < 4; ++k) {
    if (vertex.joints[k] >= joints || !std::isfinite(vertex.weights[k]) || vertex.weights[k] < 0 || vertex.weights[k] > 1)
      return false;
    total += vertex.weights[k];
  }
  return std::abs(total - 1) < .0002F;
}
} // namespace

DecodeResult decode(std::span<const std::byte> bytes, Asset& output) {
  output = {};
  if (bytes.size() > max_asset_bytes)
    return {DecodeIssue::limit, 0};
  if (bytes.size() < 20 || std::memcmp(bytes.data(), "HRA1", 4) != 0)
    return {DecodeIssue::header, 0};
  Reader reader(bytes);
  (void)reader.take(4);
  const auto nn = reader.u32(), nj = reader.u32(), nv = reader.u32(), nc = reader.u32();
  if (nn == 0 || nn > max_nodes || nj == 0 || nj > max_joints || nv == 0 || nv > max_vertices || nc == 0 || nc > max_clips)
    return {DecodeIssue::limit, reader.offset()};
  // Reject impossible totals before allocating any container from file counts.
  const std::size_t minimum = 20 + static_cast<std::size_t>(nn) * 76 + static_cast<std::size_t>(nj) * 68 +
                              static_cast<std::size_t>(nv) * 48 + static_cast<std::size_t>(nc) * (48 + 2 * static_cast<std::size_t>(nn) * 40);
  if (bytes.size() < minimum)
    return {DecodeIssue::truncated, reader.offset()};
  Asset asset;
  asset.nodes_.resize(nn);
  for (std::size_t i = 0; i < nn; ++i) {
    auto& node = asset.nodes_[i];
    node.parent = std::bit_cast<std::int32_t>(reader.u32());
    if (!reader.name(node.name))
      return {DecodeIssue::node, reader.offset()};
    node.rest = reader.transform();
    if (!reader.okay())
      return {DecodeIssue::truncated, reader.offset()};
    if (node.parent < -1 || node.parent >= static_cast<std::int32_t>(i) || !detail::valid_transform(node.rest))
      return {DecodeIssue::node, reader.offset()};
    for (std::size_t j = 0; j < i; ++j)
      if (asset.nodes_[j].name == node.name)
        return {DecodeIssue::node, reader.offset()};
  }
  asset.joints_.resize(nj);
  for (std::size_t i = 0; i < nj; ++i) {
    auto& joint = asset.joints_[i];
    joint.node = reader.u32();
    joint.inverse_bind = reader.floats<16>();
    if (!reader.okay())
      return {DecodeIssue::truncated, reader.offset()};
    const auto& m = joint.inverse_bind;
    if (joint.node >= nn || !detail::finite_matrix(m) || std::abs(m[3]) > 1e-5F || std::abs(m[7]) > 1e-5F ||
        std::abs(m[11]) > 1e-5F || std::abs(m[15] - 1) > 1e-5F)
      return {DecodeIssue::joint, reader.offset()};
    for (std::size_t j = 0; j < i; ++j)
      if (asset.joints_[j].node == joint.node)
        return {DecodeIssue::joint, reader.offset()};
  }
  asset.vertices_.resize(nv);
  for (auto& vertex : asset.vertices_) {
    vertex.position = reader.floats<3>();
    vertex.normal = reader.floats<3>();
    for (auto& joint : vertex.joints)
      joint = reader.u16();
    vertex.weights = reader.floats<4>();
    if (!reader.okay())
      return {DecodeIssue::truncated, reader.offset()};
    if (!valid_vertex(vertex, nj))
      return {DecodeIssue::vertex, reader.offset()};
  }
  asset.clips_.resize(nc);
  for (std::size_t i = 0; i < nc; ++i) {
    auto& clip = asset.clips_[i];
    if (!reader.name(clip.name))
      return {DecodeIssue::clip, reader.offset()};
    clip.seconds = reader.scalar();
    clip.stride_m = reader.scalar();
    const auto loop = reader.u32();
    clip.loop = loop != 0;
    clip.frame_count = reader.u32();
    if (!reader.okay())
      return {DecodeIssue::truncated, reader.offset()};
    if (!std::isfinite(clip.seconds) || clip.seconds <= 0 || clip.seconds > 10 || !std::isfinite(clip.stride_m) ||
        clip.stride_m < 0 || clip.stride_m > 100 || loop > 1 || clip.frame_count < 2 || clip.frame_count > max_frames)
      return {DecodeIssue::clip, reader.offset()};
    for (std::size_t j = 0; j < i; ++j)
      if (asset.clips_[j].name == clip.name)
        return {DecodeIssue::clip, reader.offset()};
    const std::size_t count = static_cast<std::size_t>(clip.frame_count) * nn;
    if (count * 40 > bytes.size() - reader.offset())
      return {DecodeIssue::truncated, reader.offset()};
    clip.frames.resize(count);
    for (auto& frame : clip.frames) {
      frame = reader.transform();
      if (!detail::valid_transform(frame))
        return {DecodeIssue::clip, reader.offset()};
    }
    if (clip.loop)
      for (std::size_t n = 0; n < nn; ++n)
        if (!same_pose(clip.frames[n], clip.frames[count - nn + n]))
          return {DecodeIssue::clip, reader.offset()};
  }
  if (reader.offset() != bytes.size())
    return {DecodeIssue::trailing_data, reader.offset()};
  Pose bind;
  for (std::size_t n = 0; n < nn; ++n)
    bind.local[n] = asset.nodes_[n].rest;
  if (!detail::compose(asset, bind))
    return {DecodeIssue::joint, reader.offset()};
  output = std::move(asset);
  return {};
}
} // namespace human_resources::animation
