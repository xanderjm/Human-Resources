#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace human_resources::animation {
inline constexpr std::size_t max_nodes = 64, max_joints = 64;
inline constexpr std::size_t max_vertices = 32768, max_clips = 16, max_frames = 256;
inline constexpr std::size_t max_asset_bytes = 16 * 1024 * 1024;
using Vec3 = std::array<float, 3>;
using Quaternion = std::array<float, 4>; // glTF x,y,z,w, unit length.
using Matrix = std::array<float, 16>;    // Column-major, actor-local, Y-up/metres.
struct Transform final {
  Vec3 translation{};
  Quaternion rotation{0, 0, 0, 1};
  Vec3 scale{1, 1, 1};
};
struct Node final {
  std::string name;
  std::int32_t parent{-1}; // Topological order: parent precedes child.
  Transform rest;
};
struct Joint final {
  std::uint32_t node{};
  Matrix inverse_bind{};
};
struct SkinVertex final {
  Vec3 position{}, normal{};
  std::array<std::uint16_t, 4> joints{};
  std::array<float, 4> weights{};
};
struct Clip final {
  std::string name;
  float seconds{}, stride_m{};
  bool loop{};
  std::uint32_t frame_count{};
  std::vector<Transform> frames; // Uniform closed-key samples, frame-major/nodes.
};
enum class DecodeIssue { none,
  header,
  limit,
  truncated,
  node,
  joint,
  vertex,
  clip,
  trailing_data };
struct DecodeResult final {
  DecodeIssue issue{};
  std::size_t offset{};
  [[nodiscard]] bool valid() const {
    return issue == DecodeIssue::none;
  }
};
class Asset;
[[nodiscard]] DecodeResult decode(std::span<const std::byte> bytes, Asset& output);
class Asset final {
public:
  [[nodiscard]] std::span<const Node> nodes() const {
    return nodes_;
  }
  [[nodiscard]] std::span<const Joint> joints() const {
    return joints_;
  }
  [[nodiscard]] std::span<const SkinVertex> vertices() const {
    return vertices_;
  }
  [[nodiscard]] std::span<const Clip> clips() const {
    return clips_;
  }
  [[nodiscard]] std::size_t find_clip(std::string_view name) const;
  [[nodiscard]] std::size_t find_node(std::string_view name) const;

private:
  friend DecodeResult decode(std::span<const std::byte>, Asset&);
  std::vector<Node> nodes_;
  std::vector<Joint> joints_;
  std::vector<SkinVertex> vertices_;
  std::vector<Clip> clips_;
};
struct Pose final {
  std::size_t node_count{}, joint_count{};
  std::array<Transform, max_nodes> local{};
  std::array<Matrix, max_nodes> world{};
  std::array<Matrix, max_joints> palette{};
  std::array<std::array<float, 9>, max_joints> normal_palette{};
};
struct DeformedVertex final {
  Vec3 position{}, normal{};
};
// Output must have exactly asset.vertices().size() elements. No allocation/I/O.
[[nodiscard]] bool skin_vertices(const Asset& asset, const Pose& pose, std::span<DeformedVertex> output);
// Focused planted-infantry elevation. A fixed authored aim reference determines
// chest pitch toward actor-local target; applying it to the live clip preserves
// authored recoil, all lower-body transforms and rigid hand/weapon contacts.
// Requires the reviewed chest/socket_weapon/muzzle hierarchy. No allocation/I/O.
[[nodiscard]] bool aim_infantry_pose(const Asset& asset, const Pose& base,
    const Pose& aim_reference, Vec3 target, Pose& output, float weight = 1);
// Product capture transport owns world motion. Remove only the source bone
// named root's translation delta; preserve its rotation and every joint pose.
[[nodiscard]] bool normalize_capture_root(const Asset& asset, const Pose& base, Pose& output);
// Requires a loaded, immutable Asset that outlives this player and never moves
// while bound. Caller owns entity IDs, active time, actor placement and speed.
// Single-thread ownership; independent players may share the immutable Asset.
class Player final {
public:
  [[nodiscard]] bool bind(const Asset& asset, std::string_view initial_clip = "idle");
  // Snapshot crossfade preserves continuity even when a fade is interrupted.
  // Repeating the current clip preserves time unless restart is requested.
  [[nodiscard]] bool play(std::string_view clip, float fade_seconds = .18F, bool restart = false);
  // Active presentation delta only, [0,.25] seconds. Zero freezes all clocks;
  // invalid input rejects without mutation. Playback rate [0,256] affects only
  // clip time; crossfades keep their active-second duration. No allocation.
  [[nodiscard]] bool advance(float seconds, float playback_rate = 1);
  // Explicit spawn/replay positioning; ends any fade. Looping clips wrap.
  [[nodiscard]] bool seek(float seconds);
  [[nodiscard]] const Pose& pose() const {
    return pose_;
  }
  [[nodiscard]] const Matrix* node_transform(std::string_view name) const;
  [[nodiscard]] std::string_view clip_name() const;
  [[nodiscard]] float time() const {
    return time_;
  }
  [[nodiscard]] bool finished() const;

private:
  [[nodiscard]] bool evaluate();
  const Asset* asset_{};
  std::size_t clip_{};
  float time_{}, fade_time_{}, fade_seconds_{};
  Pose pose_{};
  std::array<Transform, max_nodes> fade_source_{};
};
} // namespace human_resources::animation
