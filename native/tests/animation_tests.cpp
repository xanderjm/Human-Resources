#include "test_support.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <human_resources/animation/animation.h>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace human_resources::animation;
using test_support::require;
using test_support::require_near;
using Bytes = std::vector<std::byte>;
void u32(Bytes& b, std::uint32_t value) {
  for (unsigned k = 0; k < 4; ++k)
    b.push_back(static_cast<std::byte>((value >> (8 * k)) & 255));
}
void scalar(Bytes& b, float value) {
  u32(b, std::bit_cast<std::uint32_t>(value));
}
void name(Bytes& b, std::string_view value) {
  for (std::size_t k = 0; k < 32; ++k)
    b.push_back(k < value.size() ? static_cast<std::byte>(value[k]) : std::byte{});
}
void transform(Bytes& b, const Transform& t) {
  for (float v : t.translation)
    scalar(b, v);
  for (float v : t.rotation)
    scalar(b, v);
  for (float v : t.scale)
    scalar(b, v);
}
void put(Bytes& b, std::size_t offset, std::uint32_t value) {
  for (unsigned k = 0; k < 4; ++k)
    b[offset + k] = static_cast<std::byte>((value >> (8 * k)) & 255);
}
Bytes fixture() {
  Bytes b;
  for (char c : std::string_view("HRA1"))
    b.push_back(static_cast<std::byte>(c));
  for (auto n : {2U, 1U, 1U, 4U})
    u32(b, n);
  u32(b, 0xffffffffU);
  name(b, "root");
  transform(b, {});
  Transform socket;
  socket.translation[1] = 2;
  u32(b, 0);
  name(b, "socket_head");
  transform(b, socket);
  u32(b, 0);
  for (std::size_t k = 0; k < 16; ++k)
    scalar(b, k % 5 == 0 ? 1.F : 0.F);
  for (float v : {1.F, 0.F, 0.F, 1.F, 0.F, 0.F})
    scalar(b, v);
  b.resize(b.size() + 8); // Four u16 joint indices, all root.
  for (float v : {1.F, 0.F, 0.F, 0.F})
    scalar(b, v);
  for (auto clip : {"idle", "turn", "walk", "antipodal"}) {
    const std::string_view clip_name(clip);
    const bool walk = clip_name == "walk";
    name(b, clip);
    scalar(b, 1);
    scalar(b, walk ? 2.F : 0.F);
    u32(b, clip_name == "turn" ? 0U : 1U);
    u32(b, walk ? 3U : 2U);
    for (unsigned frame = 0; frame < (walk ? 3U : 2U); ++frame) {
      Transform root;
      if (walk && frame == 1)
        root.translation[0] = 1;
      if (clip_name == "turn" && frame == 1)
        root.rotation = {0, 0, 1, 0};
      if (clip_name == "antipodal" && frame == 1)
        root.rotation = {0, 0, 0, -1};
      transform(b, root);
      transform(b, socket);
    }
  }
  return b;
}
Asset asset() {
  Asset result;
  require(decode(fixture(), result).valid(), "valid fixture decodes");
  return result;
}
void malformed_files_fail_closed() {
  const auto good = fixture();
  for (const std::size_t count : {0U, 3U, 19U, 20U, 95U, 171U, 239U, 287U}) {
    auto output = asset();
    require(!decode(std::span(good).first(count), output).valid(), "truncated animation rejected");
    require(output.nodes().empty(), "failed decode leaves empty asset");
  }
  auto reject = [&](std::size_t offset, std::uint32_t value) {
    auto b = good;
    put(b, offset, value);
    Asset output;
    require(!decode(b, output).valid(), "corrupt animation field rejected");
  };
  reject(4, 65);                      // Node bound before allocation.
  reject(12, 32769);                  // Vertex bound.
  reject(20, 1);                      // Forward parent/cycle.
  reject(68, 0x7fc00000U);            // NaN quaternion.
  reject(84, 0);                      // Zero scale.
  reject(172, 2);                     // Out-of-range joint node.
  reject(176, 0);                     // Singular inverse bind matrix.
  reject(240 + 24, 2);                // Out-of-range vertex joint.
  reject(240 + 32, 0);                // Non-normalized influences.
  reject(288 + 32, 0);                // Zero duration.
  reject(288 + 44, 257);              // Frame bound.
  reject(288 + 48 + 12, 0x7fc00000U); // Nonfinite animation quaternion.
  auto extra = good;
  extra.push_back(std::byte{});
  Asset output;
  require(!decode(extra, output).valid(), "unlisted trailing bytes rejected");
}
void smooth_pose_and_socket_contract() {
  auto source = asset();
  Player p;
  require(p.bind(source), "bind idle");
  require(p.play("turn", 0), "select turn");
  require(p.seek(.5F), "seek exact subframe");
  std::array<DeformedVertex, 1> vertices;
  require(skin_vertices(source, p.pose(), vertices), "skin sample");
  require_near(vertices[0].position[0], 0, 1e-5F, "90 degree midpoint uses quaternion slerp");
  require_near(vertices[0].position[1], 1, 1e-5F, "joint palette rotates vertex");
  require_near(vertices[0].normal[1], 1, 1e-5F, "normal follows inverse transpose");
  const auto* socket = p.node_transform("socket_head");
  require(socket != nullptr, "head socket found");
  require_near((*socket)[12], -2, 1e-5F, "posed socket traverses parent rotation");
  require(p.node_transform("missing") == nullptr, "absent socket explicit");
  require(p.play("antipodal", 0), "select antipodal equivalent quaternions");
  require(p.seek(.5F), "seek antipodal midpoint");
  require(skin_vertices(source, p.pose(), vertices), "antipodal sample skins");
  require_near(vertices[0].position[0], 1, 1e-5F, "shortest arc avoids full spin");
  require(!skin_vertices(source, p.pose(), {}), "wrong mesh span rejected");
}
void clocks_loops_pause_and_fades() {
  auto source = asset();
  Player p;
  require(p.bind(source, "walk"), "bind walk");
  require(p.seek(1.25F), "loop time wraps");
  require_near(p.time(), .25F, 1e-6F, "wrapped phase");
  require(p.play("walk"), "repeated state accepted");
  require_near(p.time(), .25F, 1e-6F, "repeated state preserves phase");
  const auto before = p.pose().world;
  for (float dt : {-1.F, .3F, std::numeric_limits<float>::quiet_NaN()})
    require(!p.advance(dt), "invalid delta rejected");
  require(p.advance(0), "paused delta accepted");
  require(p.pose().world == before && p.time() == .25F, "pause/invalid delta preserve pose and clock");
  require(!p.play("missing") && p.clip_name() == "walk", "unknown clip preserves playback");
  require(p.play("turn", .2F), "begin fade");
  require(p.pose().world == before, "fade starts at current visible pose");
  require(p.advance(.1F), "fade advances");
  const auto mid = p.pose().world;
  require(p.play("idle", .2F), "interrupt fade");
  require(p.pose().world == mid, "interrupted fade preserves visible pose");
  require(p.advance(.2F), "fade reaches target");
  require_near(p.pose().world[0][12], 0, 1e-6F, "fade reaches idle translation");
  require(p.play("turn", 0), "one-shot begins");
  for (int i = 0; i < 8; ++i)
    require(p.advance(.25F), "one-shot advances");
  require(p.finished() && p.time() == 1, "one-shot clamps to endpoint");
  require(p.play("walk", 0), "loop begins");
  for (int i = 0; i < 5; ++i)
    require(p.advance(.25F), "loop advances");
  require_near(p.time(), .25F, 1e-6F, "loop crosses boundary continuously");
  require(!p.finished(), "loops never finish");
  require(p.seek(0) && p.advance(.25F, 2), "scaled clock advances in one bounded update");
  require_near(p.time(), .5F, 1e-6F, "playback rate scales clip phase");
  const auto scaled = p.pose().world;
  require(!p.advance(.1F, 257) && !p.advance(.1F, -257), "invalid playback rates rejected");
  require(p.pose().world == scaled, "invalid rate does not mutate pose");
  require(p.advance(.25F, -2), "looping gait can run backwards");
  require_near(p.time(), 0, 1e-6F, "reverse gait consumes the matching stride phase");
  require(p.advance(.1F, -1), "reverse gait wraps through zero");
  require_near(p.time(), .9F, 1e-6F, "reverse loop wraps to the last part of its stride");
  require(p.advance(0, -1) && p.time() == .9F, "paused reverse gait retains its phase");
  require(p.play("turn", 0) && !p.advance(.1F, -1) && p.time() == 0,
      "one-shot combat and capture clips cannot run backwards");
  require(p.bind(source) && p.play("walk", .2F) && p.advance(.1F, 4), "rate-scaled clip crossfades");
  require_near(p.pose().world[0][12], .4F, 1e-5F, "crossfade duration uses active time, not clip rate");
}
Bytes read(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  require(file.good(), "test artifact opens");
  const auto size = file.tellg();
  require(size > 0 && size <= static_cast<std::streamoff>(max_asset_bytes), "test artifact bounded");
  Bytes bytes(static_cast<std::size_t>(size));
  file.seekg(0);
  file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  require(file.good(), "test artifact read complete");
  return bytes;
}
class Reference final {
public:
  explicit Reference(Bytes bytes) : bytes_(std::move(bytes)) {}
  std::uint32_t integer() {
    require(at_ + 4 <= bytes_.size(), "reference bounded");
    std::uint32_t result{};
    for (unsigned k = 0; k < 4; ++k)
      result |= std::to_integer<std::uint32_t>(bytes_[at_++]) << (8 * k);
    return result;
  }
  float number() {
    return std::bit_cast<float>(integer());
  }
  std::string text() {
    std::string result;
    for (int i = 0; i < 32; ++i) {
      require(at_ < bytes_.size(), "reference name bounded");
      const char c = static_cast<char>(bytes_[at_++]);
      if (c)
        result += c;
    }
    return result;
  }
  bool ended() const {
    return at_ == bytes_.size();
  }

private:
  Bytes bytes_;
  std::size_t at_{};
};
void parity_and_profile(const std::filesystem::path& assets, const std::filesystem::path& references) {
  for (const auto* id : {"cow", "pig", "sheep", "farmer-body", "resident-body", "soldier-body"}) {
    Asset source;
    const auto result = decode(read(assets / (std::string(id) + ".hra")), source);
    if (!result.valid())
      std::cerr << id << " decode issue " << static_cast<int>(result.issue) << " offset " << result.offset << '\n';
    require(result.valid(), "real cooked asset decodes");
    Reference ref(read(references / (std::string(id) + ".hrt")));
    require(ref.integer() == 0x31545248U, "reference HRT1 header");
    const auto samples = ref.integer(), count = ref.integer();
    const auto socket = ref.text();
    std::vector<std::uint32_t> selected(count);
    for (auto& index : selected) {
      index = ref.integer();
      require(index < source.vertices().size(), "reference vertex in bounds");
    }
    Player player;
    require(player.bind(source), "real asset binds");
    std::vector<DeformedVertex> output(source.vertices().size());
    float max_position_error = 0, max_normal_error = 0, max_socket_error = 0;
    for (std::uint32_t sample = 0; sample < samples; ++sample) {
      const auto clip = ref.integer();
      const float time = ref.number();
      require(clip < source.clips().size(), "reference clip exists");
      require(player.play(source.clips()[clip].name, 0), "reference clip selects");
      require(player.seek(time), "reference subframe seeks");
      require(skin_vertices(source, player.pose(), output), "native geometry deforms");
      for (const auto index : selected) {
        for (float actual : output[index].position)
          max_position_error = std::max(max_position_error, std::abs(actual - ref.number()));
        for (float actual : output[index].normal)
          max_normal_error = std::max(max_normal_error, std::abs(actual - ref.number()));
      }
      const auto* m = player.node_transform(socket);
      require(m != nullptr, "reference head/socket exists");
      for (float actual : *m)
        max_socket_error = std::max(max_socket_error, std::abs(actual - ref.number()));
    }
    require(ref.ended(), "all source reference samples consumed");
    std::cout << "PARITY " << id << " samples=" << samples << " position=" << max_position_error << " normal=" << max_normal_error << " socket=" << max_socket_error << '\n';
    require(max_position_error < .0001F && max_normal_error < .0004F && max_socket_error < .0001F, "native keys and half-frames match source GLB");
    require(player.play("walk", 0), "profile walk");
    const auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < 200; ++iteration) {
      require(player.advance(1.F / 60), "profile pose advances");
      require(skin_vertices(source, player.pose(), output), "profile skin deforms");
    }
    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 200;
    std::cout << "PROFILE " << id << " vertices=" << output.size() << " pose_and_skin_us=" << us << '\n';
  }
}
} // namespace
int main(int argc, char** argv) {
  malformed_files_fail_closed();
  smooth_pose_and_socket_contract();
  clocks_loops_pause_and_fades();
  if (argc == 3)
    parity_and_profile(argv[1], argv[2]);
  std::cout << "Animation contracts passed.\n";
}
