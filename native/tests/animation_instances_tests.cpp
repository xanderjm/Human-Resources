#include "test_support.h"
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <human_resources/renderer/animation_instances.h>
#include <iostream>
#include <limits>
#include <vector>
using test_support::require;
namespace {
using Bytes = std::vector<std::byte>;
void u32(Bytes& b, std::uint32_t v) {
  for (unsigned k = 0; k < 4; ++k)
    b.push_back(static_cast<std::byte>((v >> (8 * k)) & 255));
}
void scalar(Bytes& b, float v) {
  u32(b, std::bit_cast<std::uint32_t>(v));
}
void name(Bytes& b, std::string_view n) {
  for (std::size_t k = 0; k < 32; ++k)
    b.push_back(k < n.size() ? static_cast<std::byte>(n[k]) : std::byte{});
}
void transform(Bytes& b, float x = 0, float y = 0) {
  for (float v : {x, y, 0.F, 0.F, 0.F, 0.F, 1.F, 1.F, 1.F, 1.F})
    scalar(b, v);
}
Bytes fixture() {
  Bytes b;
  for (char c : std::string_view("HRA1"))
    b.push_back(static_cast<std::byte>(c));
  for (auto n : {2U, 1U, 1U, 7U})
    u32(b, n);
  u32(b, 0xffffffffU);
  name(b, "root");
  transform(b);
  u32(b, 0);
  name(b, "socket_head");
  transform(b, 0, 2);
  u32(b, 0);
  for (std::size_t k = 0; k < 16; ++k)
    scalar(b, k % 5 == 0 ? 1.F : 0.F);
  for (float v : {1.F, 0.F, 0.F, 0.F, 1.F, 0.F})
    scalar(b, v);
  for (unsigned k = 0; k < 8; ++k)
    b.push_back(std::byte{});
  for (float v : {1.F, 0.F, 0.F, 0.F})
    scalar(b, v);
  for (std::string_view clip : {"idle", "walk", "run", "startle", "beam_lift", "beam_struggle", "beam_release"}) {
    const bool loop = clip == "idle" || clip == "walk" || clip == "run" || clip == "beam_struggle";
    name(b, clip);
    scalar(b, 1);
    scalar(b, clip == "walk" || clip == "run" ? 1.F : 0.F);
    u32(b, loop ? 1 : 0);
    u32(b, 3);
    for (unsigned k = 0; k < 3; ++k) {
      const float x = k == 1 ? .1F : 0.F;
      transform(b, x);
      transform(b, 0, 2 + x);
    }
  }
  return b;
}
Bytes read(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  require(static_cast<bool>(f), "read cooked pair");
  const auto size = f.tellg();
  Bytes b(static_cast<std::size_t>(size));
  f.seekg(0);
  f.read(reinterpret_cast<char*>(b.data()), size);
  return b;
}
} // namespace
int main(int argc, char** argv) {
  using namespace human_resources;
  renderer::AnimationInstances cache;
  assets::PreviewVertex vertex{{1, 0, 0}, {0, 1, 0}, {.3F, .7F}, {.1F, .2F, .3F, 1}, .4F, .5F};
  const auto bytes = fixture();
  require(cache.load("fixture", bytes, std::span(&vertex, 1)), "valid companion accepted");
  require(!cache.load("fixture", bytes, std::span(&vertex, 1)), "duplicate asset rejected");
  renderer::AnimationIntent escape{.speed = 1.4F, .enabled = true, .fleeing = true};
  cache.begin_frame();
  const auto* fleeing = cache.update(918, "fixture", escape, .1F);
  require(fleeing && fleeing->clip == "run", "civilian escape intent uses the authored run without a military warning");
  cache.end_frame();
  const float escape_time = fleeing->clip_time;
  cache.begin_frame();
  fleeing = cache.update(918, "fixture", escape, .1F);
  test_support::require_near(fleeing->clip_time - escape_time, .14F, 1e-5F, "escape run phase matches actual travel speed and authored stride");
  cache.end_frame();
  renderer::AnimationIntent intent{.speed = 1, .enabled = true};
  cache.begin_frame();
  const auto* a = cache.update(17, "fixture", intent, .01F);
  require(a && a->has_head && a->clip == "walk", "walking actor and head pose");
  test_support::require_near(a->head_delta[12], a->vertices[0].position[0] - 1, 1e-5F, "socket follows posed actor translation");
  test_support::require_near(a->head_delta[13], a->head_delta[12], 1e-5F, "socket rest height removed before attachment");
  const auto attached = renderer::head_attachment_matrix(*a);
  test_support::require_near(attached[3], a->head_delta[12], 1e-6F, "renderer attachment translation uses row-major storage");
  test_support::require_near(attached[7], a->head_delta[13], 1e-6F, "animated hat vertical translation converted at renderer boundary");
  require(attached[12] == 0 && attached[13] == 0 && attached[15] == 1, "attachment affine final row stays valid");
  renderer::AnimatedActor rotated_head;
  rotated_head.head_delta = {0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 3, 4, 5, 1};
  const auto rotated_attachment = renderer::head_attachment_matrix(rotated_head);
  // +90 degree Y rotation followed by translation [3,4,5] maps [1,2,3]
  // to [6,6,4]. This must survive the glTF->presentation matrix boundary.
  const std::array<float, 4> hat_point{1, 2, 3, 1};
  const std::array<float, 4> expected_point{6, 6, 4, 1};
  for (std::size_t row = 0; row < 4; ++row) {
    float transformed = 0;
    for (std::size_t column = 0; column < 4; ++column)
      transformed += rotated_attachment[row * 4 + column] * hat_point[column];
    test_support::require_near(transformed, expected_point[row], 1e-6F, "rotated/translated headwear stays in actor space");
  }
  require(a->vertices[0].uv == vertex.uv && a->vertices[0].color == vertex.color && a->vertices[0].emission == vertex.emission && a->vertices[0].rotor == vertex.rotor, "skinning retains non-pose fields");
  const auto first_time = a->clip_time;
  const auto first_revision = a->revision;
  const auto* first_storage = a->vertices.data();
  require(!cache.update(17, "fixture", intent, .01F), "duplicate ID cannot advance twice");
  cache.end_frame();
  cache.begin_frame();
  intent.capturing = true;
  a = cache.update(17, "fixture", intent, 0);
  require(a && a->clip == "walk" && a->clip_time == first_time && a->revision == first_revision, "pause freezes transitions and pose");
  cache.end_frame();
  cache.begin_frame();
  a = cache.update(17, "fixture", intent, .01F);
  require(a && a->clip == "beam_lift" && a->vertices.data() == first_storage, "capture starts and scratch retained");
  cache.end_frame();
  for (unsigned k = 0; k < 5; ++k) {
    cache.begin_frame();
    a = cache.update(17, "fixture", intent, .25F);
    require(a, "capture advances");
    cache.end_frame();
  }
  require(a->clip == "beam_struggle", "lift transitions into struggle");
  intent.capturing = false;
  cache.begin_frame();
  a = cache.update(17, "fixture", intent, .01F);
  require(a && a->clip == "beam_release", "cancel releases");
  cache.end_frame();
  for (unsigned k = 0; k < 5; ++k) {
    cache.begin_frame();
    a = cache.update(17, "fixture", intent, .25F);
    require(a, "release advances");
    cache.end_frame();
  }
  require(a->clip == "walk", "release returns to locomotion");
  intent.warning = true;
  cache.begin_frame();
  a = cache.update(17, "fixture", intent, .01F);
  require(a && a->clip == "startle", "warning edge startles");
  cache.end_frame();
  for (unsigned k = 0; k < 5; ++k) {
    cache.begin_frame();
    a = cache.update(17, "fixture", intent, .25F);
    require(a, "warning advances");
    cache.end_frame();
  }
  require(a->clip == "run", "startle returns to escape run");
  cache.begin_frame();
  require(!cache.update(17, "fixture", intent, -1), "negative delta rejected");
  require(!cache.update(17, "fixture", intent, std::numeric_limits<float>::quiet_NaN()), "nonfinite delta rejected");
  cache.end_frame();
  require(cache.size() == 0, "absent actors evicted");
  cache.begin_frame();
  for (std::uint64_t id = 1; id <= renderer::AnimationInstances::actor_capacity; ++id)
    require(cache.update(id, "fixture", intent, 0), "actor budget accepted");
  require(!cache.update(129, "fixture", intent, 0), "actor budget bounded");
  cache.end_frame();
  require(cache.load("replacement", bytes, std::span(&vertex, 1)), "replacement source loads");
  cache.begin_frame();
  require(cache.update(17, "replacement", intent, 0), "retry can reuse ID for new source");
  require(!cache.update(17, "fixture", intent, 0), "duplicate ID cannot rebind inside same frame");
  cache.end_frame();
  intent = {.capturing = true, .enabled = true};
  cache.begin_frame();
  require(cache.update(17, "fixture", intent, .1F), "same-model capture can begin before restart");
  cache.end_frame();
  cache.clear_actors();
  require(cache.size() == 0 && cache.contains("fixture") && cache.contains("replacement"), "run boundary clears actors while retaining prepared assets");
  intent = {.enabled = true};
  cache.begin_frame();
  a = cache.update(17, "fixture", intent, 0);
  require(a && a->clip == "idle", "same ID and model after restart cannot inherit old capture/startle clip");
  cache.end_frame();
  intent = {.capturing = true, .enabled = true, .product_capture_transport = true, .capture_progress = .05F};
  cache.begin_frame();
  a = cache.update(901, "fixture", intent, 0);
  require(a && a->clip == "beam_lift" && a->clip_time > .07F, "cold capture starts at its actual authored lift progress");
  cache.end_frame();
  for (float progress : {.25F, .45F, .60F, .70F, .9F}) {
    intent.capture_progress = progress;
    cache.begin_frame();
    a = cache.update(901, "fixture", intent, 1.F / 60);
    require(a, "fast actual capture progress advances the complete authored pose");
    if (progress == .60F)
      require(a->clip == "beam_lift" && a->clip_time > .9F, "fast sweep shows the full lift before intake despite under50ms elapsed");
    if (progress == .9F)
      require(a->clip == "beam_struggle" && a->clip_time > .7F, "remaining actual intake progress shows authored struggle");
    cache.end_frame();
  }
  const auto capture_time = a->clip_time;
  intent.capture_progress = .99F;
  cache.begin_frame();
  a = cache.update(901, "fixture", intent, 0);
  require(a && a->clip_time == capture_time, "paused progress update cannot advance or seek an existing capture pose");
  cache.end_frame();
  intent.capturing = false;
  intent.capture_progress = 0;
  cache.begin_frame();
  a = cache.update(901, "fixture", intent, .1F);
  require(a && a->clip == "beam_release" && a->clip_time < .11F, "capture cancel retains normal active-time release clock");
  cache.end_frame();
  intent.capture_progress = std::numeric_limits<float>::quiet_NaN();
  cache.begin_frame();
  require(!cache.update(901, "fixture", intent, .1F), "invalid normalized capture timing rejects before posing");
  cache.end_frame();
  if (argc > 1) {
    renderer::AnimationInstances real;
    constexpr std::array<std::string_view, 6> names{"cow", "pig", "sheep", "farmer-body", "resident-body", "soldier-body"};
    std::size_t vertices = 0;
    for (auto id : names) {
      const auto base = std::filesystem::path(argv[1]) / id;
      const auto hrm = read(base.string() + ".hrm");
      assets::PreviewMesh mesh;
      require(assets::decode_preview_mesh(hrm, mesh), "actual HRM decodes");
      const auto hra = read(base.string() + ".hra");
      require(real.load(id, hra, mesh.vertices), "actual HRA companion agrees");
      vertices += mesh.vertices.size();
    }
    renderer::AnimationIntent walk{.speed = .5F, .enabled = true};
    real.begin_frame();
    for (std::uint64_t id = 1; id <= 64; ++id)
      require(real.update(id, names[static_cast<std::size_t>(id % 6)], walk, 0), "actual actor spawn");
    real.end_frame();
    const auto start = std::chrono::steady_clock::now();
    for (unsigned frame = 0; frame < 60; ++frame) {
      real.begin_frame();
      for (std::uint64_t id = 1; id <= 64; ++id)
        require(real.update(id, names[static_cast<std::size_t>(id % 6)], walk, 1.F / 60.F), "actual actor update");
      real.end_frame();
    }
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 60.;
    std::cout << "64 actors CPU pose+skin ms/frame=" << ms << " six-asset vertices=" << vertices << '\n';
  }
  std::cout << "animation instance contracts passed\n";
}
