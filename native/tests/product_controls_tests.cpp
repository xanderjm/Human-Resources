#include <human_resources/product/controls.h>
#include "test_support.h"

#include <limits>

namespace {
namespace hr = human_resources;
using hr::input::ActionPhase;
using hr::product::ControlAction;
using test_support::require;
using test_support::require_near;

hr::input::Action touch(std::uint64_t contact, ActionPhase phase, float x, float y, hr::input::DeviceId device = 1) {
  hr::input::Action a;
  a.id = static_cast<hr::input::ActionId>(ControlAction::contact);
  a.device = device; a.contact = contact; a.phase = phase; a.value = 1;
  a.position = {x, y}; a.has_position = true; a.source_kind = hr::input::DeviceKind::touch;
  return a;
}
hr::input::Action key(ControlAction id, int control, ActionPhase phase, hr::input::DeviceId device = 2, float value = 1) {
  hr::input::Action a;
  a.id = static_cast<hr::input::ActionId>(id); a.device = device;
  a.source_control = control; a.phase = phase; a.value = value;
  return a;
}
void contact_capture_and_cancellation() {
  hr::product::Controls controls;
  controls.apply(touch(17, ActionPhase::started, 120, 120), true, false);
  controls.apply(touch(17, ActionPhase::changed, 164, 120), true, false);
  require(controls.dragging(), "first free contact owns floating stick");
  require_near(controls.sample().movement.x, 1, .001F, "forty-four point drag gives full movement");
  controls.apply(touch(18, ActionPhase::started, 700, 340), true, true);
  controls.apply(touch(18, ActionPhase::cancelled, 700, 340), true, true);
  controls.apply(touch(17, ActionPhase::completed, 164, 120, 9), true, false);
  require(controls.dragging(), "UI finger or matching contact from another device cannot cancel movement");
  controls.apply(touch(17, ActionPhase::changed, 164, 120), true, true);
  require_near(controls.sample().movement.x, 1, .001F, "captured movement continues over HUD");
  controls.apply(touch(17, ActionPhase::cancelled, 164, 120), true, false);
  require(!controls.dragging(), "own cancellation releases movement");
  require_near(controls.sample().movement.x, 0, 0, "cancelled contact cannot drift");
  controls.apply(touch(2, ActionPhase::started, 120, 120), true, true);
  controls.apply(touch(2, ActionPhase::changed, 164, 120), true, false);
  require(!controls.dragging(), "contact begun on UI cannot turn into movement");
  controls.apply(touch(3, ActionPhase::started, 120, 120), true, false);
  controls.apply(touch(3, ActionPhase::changed, 164, 164), true, false);
  require_near(std::hypot(controls.sample().movement.x, controls.sample().movement.z), 1, .001F, "diagonal drag is normalized");
}
void keyboard_sources_and_opposites() {
  hr::product::Controls controls;
  controls.apply(key(ControlAction::up, 26, ActionPhase::started), true, false);
  controls.apply(key(ControlAction::up, 82, ActionPhase::started), true, false);
  controls.apply(key(ControlAction::up, 26, ActionPhase::completed), true, false);
  require_near(controls.sample().movement.z, 1, 0, "releasing W leaves held Up active");
  controls.apply(key(ControlAction::down, 22, ActionPhase::started), true, false);
  require_near(controls.sample().movement.z, 0, 0, "held opposites cancel");
  controls.apply(key(ControlAction::down, 22, ActionPhase::cancelled), true, false);
  require_near(controls.sample().movement.z, 1, 0, "cancelling opposite resumes other held direction");
  controls.apply(key(ControlAction::up, 82, ActionPhase::cancelled), true, false);
  require_near(controls.sample().movement.z, 0, 0, "final source cancellation stops movement");
  controls.apply(key(ControlAction::left, 4, ActionPhase::started), true, false);
  controls.apply(key(ControlAction::left, 80, ActionPhase::started), true, false);
  controls.apply(key(ControlAction::left, 4, ActionPhase::completed), true, false);
  require_near(controls.sample().movement.x, -1, 0, "releasing A leaves held Left active");
}
void dash_axes_lifecycle_and_unplug() {
  hr::product::Controls controls;
  controls.apply(key(ControlAction::dash, 44, ActionPhase::started), true, false);
  controls.apply(key(ControlAction::dash, 44, ActionPhase::completed), true, false);
  require(controls.sample().dash, "short press retains dash rising edge within one frame");
  require(!controls.sample().dash, "dash edge is consumed exactly once");
  auto axis = key(ControlAction::axis_y, 257, ActionPhase::changed, 8, -.75F);
  axis.source_kind = hr::input::DeviceKind::gamepad;
  controls.apply(axis, true, false);
  controls.apply(key(ControlAction::right, 7, ActionPhase::started), true, false);
  require_near(controls.sample().movement.z, .75F, 0, "controller negative Y maps to forward");
  controls.remove_device(8);
  require_near(controls.sample().movement.z, 0, 0, "unplug cancels removed device axis");
  require_near(controls.sample().movement.x, 1, 0, "unplug preserves held keyboard");
  controls.apply(touch(4, ActionPhase::started, 50, 50, 10), true, false);
  controls.remove_device(2);
  require(controls.dragging(), "keyboard removal preserves touch capture");
  controls.remove_device(10);
  require(!controls.dragging(), "touch device removal releases capture");
  controls.apply(key(ControlAction::left, 4, ActionPhase::started), false, false);
  require_near(controls.sample().movement.x, 0, 0, "paused input cannot latch movement");
  axis.value = std::numeric_limits<float>::quiet_NaN();
  controls.apply(axis, true, false);
  require(std::isfinite(controls.sample().movement.z), "invalid axis cannot contaminate movement");
  controls.clear();
  require(!controls.dragging() && !controls.sample().dash, "lifecycle reset releases all state");
}
} // namespace
int main() {
  contact_capture_and_cancellation();
  keyboard_sources_and_opposites();
  dash_axes_lifecycle_and_unplug();
}
