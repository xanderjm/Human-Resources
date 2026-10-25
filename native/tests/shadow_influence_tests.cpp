#include "shadow_influence.h"
#include "test_support.h"

namespace {
using namespace human_resources::renderer::shadow_detail;
using test_support::require;
const Matrix camera{.1F, 0, 0, 0, 0, 0, .1F, 0, 0, .01F, 0, 0, 0, 0, 0, 1};
const Matrix light{.025F, -.015F, 0, 0, 0, .015F, .025F, 0, 0, -.01F, 0, 0, 0, 0, 0, 1};
Matrix identity() {
  return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
void contributing_shadows_stay() {
  const Influence influence(camera, light, -.05, 2048);
  require(influence.valid, "finite affine light has a downward influence volume");
  auto model = identity();
  model[3] = 18;
  require(influence.rejects(model, {0, 0, 0}, {1, 1, 1}), "distant low caster cannot reach any visible receiver");
  model[3] = 12;
  require(!influence.rejects(model, {0, 0, 0}, {1, 5, 1}), "offscreen tall caster's visible shadow remains");
  model[3] = -12;
  require(influence.rejects(model, {0, 0, 0}, {1, 5, 1}), "caster opposite light direction cannot shadow the view");
  model[3] = 0;
  require(!influence.rejects(model, {-1, 0, -1}, {1, 2, 1}), "visible geometry remains conservatively included");
  model[3] = 10.005F;
  require(!influence.rejects(model, {0, 0, 0}, {.001F, .001F, .001F}), "linear PCF support crossing the viewport edge remains");
  model[3] = 18;
  model[0] = -1;
  require(influence.rejects(model, {0, 0, 0}, {1, 1, 1}), "negative scale uses transformed corners");
}
void receiver_floor_and_unsupported_contexts_are_conservative() {
  auto model = identity();
  model[7] = -3;
  require(receiver_floor(model, {0, 0, 0}, {1, 1, 1}) < -3, "receiver floor pads transform roundoff outward");
  auto perspective = camera;
  perspective[14] = 1;
  const Influence unsupported(perspective, light, 0, 2048);
  require(!unsupported.valid && !unsupported.rejects(model, {0, 0, 0}, {1, 1, 1}), "unsupported projection keeps casters");
  auto upward = light;
  upward[9] = .01F;
  require(!Influence(camera, upward, 0, 2048).valid, "upward light has no assumed receiver floor");
  require(!Influence(camera, light, std::numeric_limits<double>::quiet_NaN(), 2048).valid, "unknown floor keeps casters");
  auto singular = light;
  singular[0] = singular[1] = singular[2] = 0;
  require(!Influence(camera, singular, 0, 2048).valid, "singular light keeps casters");
  require(!Influence(camera, light, 0, 0).valid, "unknown shadow extent keeps casters");
  const Influence floor_above_caster(camera, light, 5, 2048);
  require(!floor_above_caster.rejects(model, {0, 0, 0}, {1, 1, 1}), "caster below declared receiver floor keeps uncertain geometry");
}
void actual_world_projection_uses_actual_light() {
  human_resources::presentation::StyleFrame frame;
  human_resources::presentation::set_overhead_camera(frame, 588.F / 1280, 17.F / .75F, {0, 0, -.15F}, 27);
  const Influence influence(frame.camera, frame.light, -.05, 2048);
  require(influence.valid, "actual production orthographic light is supported");
  auto tree = identity();
  tree[3] = 14.7F;
  tree[11] = -5;
  tree[0] = tree[5] = tree[10] = 1.7F;
  require(!influence.rejects(tree, {-1.44F, 0, -1.52F}, {1.44F, 3.2F, 1.52F}), "actual right edge tall tree shadow reaches the view");
  tree[3] = -14.7F;
  require(influence.rejects(tree, {-1.44F, 0, -1.52F}, {1.44F, 3.2F, 1.52F}), "actual left edge tree points its shadow away");
}
} // namespace
int main() {
  contributing_shadows_stay();
  receiver_floor_and_unsupported_contexts_are_conservative();
  actual_world_projection_uses_actual_light();
}
