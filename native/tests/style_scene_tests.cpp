#include "test_support.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <human_resources/assets/preview_mesh.h>
#include <human_resources/presentation/style_scene.h>
#include <human_resources/renderer/preview_viewport.h>
#include <human_resources/renderer/capture_extent.h>
#include <iostream>
#include <limits>
#include <vector>
using test_support::require;
namespace {
void put(std::vector<std::byte>& b, std::size_t p, std::uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b[p + i] = static_cast<std::byte>((v >> (i * 8)) & 255);
}
std::vector<std::byte> fixture() {
  std::vector<std::byte> b(20 + 56 + 12 + 4);
  std::memcpy(b.data(), "HRM1", 4);
  put(b, 4, 1);
  put(b, 8, 3);
  put(b, 12, 1);
  put(b, 16, 1);
  return b;
}
void mesh_validation() {
  using human_resources::assets::decode_preview_mesh;
  using human_resources::assets::PreviewMesh;
  auto bytes = fixture();
  PreviewMesh m;
  require(decode_preview_mesh(bytes, m) && m.vertices.size() == 1 && m.rgba.size() == 4, "valid mesh decoded");
  bytes.pop_back();
  require(!decode_preview_mesh(bytes, m) && m.vertices.empty(), "truncated image rejected without stale output");
  bytes = fixture();
  put(bytes, 4, 1000001);
  require(!decode_preview_mesh(bytes, m), "allocation budget enforced");
  bytes = fixture();
  put(bytes, 20 + 56, 1);
  require(!decode_preview_mesh(bytes, m), "invalid index rejected");
  bytes = fixture();
  put(bytes, 20, std::bit_cast<std::uint32_t>(std::numeric_limits<float>::quiet_NaN()));
  require(!decode_preview_mesh(bytes, m), "nonfinite vertex rejected");
  bytes = fixture();
  bytes.push_back(std::byte{});
  require(!decode_preview_mesh(bytes, m), "trailing data rejected");
}
void film_contract() {
  using namespace human_resources::presentation;
  for (float aspect : {9.F / 16, 16.F / 9})
    for (unsigned n = 0; n < 900; ++n) {
      auto f = style_frame(static_cast<float>(n) / 30, aspect);
      require(f.location == n / 300, "location changes at authored cut");
      require(f.instances.size() > 50 && f.instances.size() < 1000, "bounded complete scene");
      unsigned craft = 0;
      bool cow = false;
      for (const auto& i : f.instances) {
        craft += i.mesh == "ufo-modular" ? 1U : 0U;
        cow = cow || i.mesh == "cow";
        for (float v : model_matrix(i))
          require(std::isfinite(v), "finite transform");
      }
      require(craft == 1 && cow, "assembled saucer and existing cow present");
      for (float v : f.camera)
        require(std::isfinite(v), "finite camera");
    }
  StyleInstance i{.mesh = "cow", .position = {2, 3, 4}};
  const auto m = model_matrix(i);
  require(m[0] == 1 && m[5] == 1 && m[10] == 1 && m[15] == 1, "identity rotation retains affine diagonal");
  require(m[3] == 2 && m[7] == 3 && m[11] == 4 && m[12] == 0 && m[13] == 0 && m[14] == 0, "translation preserves homogeneous coordinate");
  const auto f = style_frame(std::numeric_limits<float>::quiet_NaN(), 0);
  require(f.location == 0, "invalid preview clock sanitized");
}
void viewport_and_visibility() {
  using namespace human_resources::presentation;
  using human_resources::renderer::fit_preview_extent;
  const auto phone = fit_preview_extent(780, 1688, 1280, 1280);
  require(phone.width == 591 && phone.height == 1280, "phone backing pixels fit render budget without changing shape");
  const auto landscape = fit_preview_extent(1688, 780, 1280, 1280);
  require(landscape.width == phone.height && landscape.height == phone.width, "rotation swaps render dimensions");
  require(fit_preview_extent(600, 600, 1280, 1280).width == 600, "small viewport is not needlessly upscaled");
  const auto letterbox = fit_preview_extent(390, 844, 1200, 800, true);
  require(letterbox.width == 370 && letterbox.height == 800, "wide window fits phone image instead of stretching it");
  require(fit_preview_extent(0, 844, 1280, 1280).width == 0, "zero drawable is not divided by");
  const auto identity = model_matrix({});
  require(bounds_visible(identity, {-.5F, -.5F, .1F}, {.5F, .5F, .9F}), "visible bounds retained");
  require(!bounds_visible(identity, {2, -.5F, .1F}, {3, .5F, .9F}), "fully offscreen geometry culled");
  require(bounds_visible(identity, {.9F, -.5F, .1F}, {2, .5F, .9F}), "partly visible geometry retained");
  require(!bounds_visible(identity, {-.5F, -.5F, -2}, {.5F, .5F, -1}), "geometry behind near plane culled");
  require(bounds_visible(identity, {1.2F, -.5F, .1F}, {1.4F, .5F, .9F}, 1.6F), "nearby offscreen shadow casters retained");
}
void capture_contract() {
  using human_resources::renderer::CaptureExtent;
  using human_resources::renderer::parse_capture_extent;
  require(CaptureExtent{}.empty() && !CaptureExtent{}.valid(), "empty extent means legacy world-sized capture, never an explicit zero allocation");
  for (const auto text : {"1080x1920", "1206x2622", "2064x2752", "4096x2048", "1x1"})
    require(parse_capture_extent(text).has_value(), "bounded exact output pixels accepted independently of world cap/alignment");
  for (const auto text : {"", "0x0", "0x1280", "720x0", "4097x1", "4096x2049", "999999999999999999x1", "-1x2", "+1x2", "1X2", "1x2x3", "1x2 ", " 1x2", "1", "x1", "1x"})
    require(!parse_capture_extent(text), "malformed, overflowing or excessive capture extent rejected before allocation");
  require(CaptureExtent{1206, 2622}.matches_aspect(402, 874), "phone exact pixels match logical window without stretching");
  require(CaptureExtent{1080, 1920}.matches_aspect(360, 640), "store portrait matches logical layout");
  require(CaptureExtent{2064, 2752}.matches_aspect(516, 688), "tablet capture uses generic bounded contract");
  require(!CaptureExtent{1080, 1920}.matches_aspect(390, 844), "different portrait layout cannot silently stretch into requested screenshot");
  require(!CaptureExtent{1206, 2622}.matches_aspect(0, 874), "unavailable window is not an aspect match");
  require(CaptureExtent{1207, 2622}.matches_aspect(402, 874) && !CaptureExtent{1208, 2622}.matches_aspect(402, 874), "aspect rounding tolerance is exactly one physical pixel");
}
} // namespace
int main() {
  mesh_validation();
  film_contract();
  viewport_and_visibility();
  capture_contract();
  std::cout << "style scene and mesh contracts passed\n";
}
