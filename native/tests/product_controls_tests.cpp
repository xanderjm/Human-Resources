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
void floating_anchor_reversals_and_precision() {
  hr::product::Controls controls;
  controls.apply(touch(1, ActionPhase::started, 100, 300), true, false);
  controls.apply(touch(1, ActionPhase::changed, 300, 300), true, false);
  require_near(controls.sample().movement.x, 1, .001F, "long sweep retains full speed");
  require_near(controls.origin().x, 256, .001F, "floating anchor follows beyond full throw");
  controls.apply(touch(1, ActionPhase::changed, 256, 300), true, false);
  require_near(controls.sample().movement.x, 0, .001F, "reversal reaches neutral after one throw, not entire sweep");
  controls.apply(touch(1, ActionPhase::changed, 212, 300), true, false);
  require_near(controls.sample().movement.x, -1, .001F, "reversal reaches opposite full speed after two throws");
  controls.apply(touch(1, ActionPhase::changed, 212, 100), true, false);
  require_near(std::hypot(controls.position().x - controls.origin().x,
                         controls.position().y - controls.origin().y), 44, .001F,
               "cornering keeps radial throw bounded");
  require_near(std::hypot(controls.sample().movement.x, controls.sample().movement.z), 1, .001F,
               "cornering retains normalized full-speed direction");
  controls.clear();
  controls.apply(touch(2, ActionPhase::started, 40, 40), true, false);
  controls.apply(touch(2, ActionPhase::changed, 44.4F, 40), true, false);
  require_near(controls.sample().movement.x, 0, .00001F, "deadzone edge has no speed jump");
  controls.apply(touch(2, ActionPhase::changed, 44.44F, 40), true, false);
  require(controls.sample().movement.x > 0 && controls.sample().movement.x < .002F,
          "fine movement starts continuously above the deadzone");
  controls.apply(touch(2, ActionPhase::changed, 64.2F, 40), true, false);
  require_near(controls.sample().movement.x, .5F, .001F, "remaining throw covers full analog speed range");
  controls.apply(touch(2, ActionPhase::cancelled, 64.2F, 40), true, false);
  controls.apply(touch(3, ActionPhase::started, 300, 700), true, false);
  require_near(controls.origin().x, 300, 0, "new contact starts at its own touchdown");
  require_near(controls.sample().movement.x, 0, 0, "new contact cannot inherit previous movement");
}
void floating_anchor_direction_changes_and_ownership() {
  // Semantic positions are logical points, independent of a high-DPI render
  // scale. Exercise phone/tablet touchdown positions with the same 44pt throw.
  for (const float width : {320.F, 390.F, 768.F}) {
    for (const hr::ui::Point direction : {hr::ui::Point{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                                         {.6F, .8F}, {-.6F, .8F}, {.6F, -.8F}, {-.6F, -.8F}}) {
      hr::product::Controls controls;
      const hr::ui::Point start{width * .5F, width * .8F};
      const hr::ui::Point end{start.x + direction.x * 200, start.y + direction.y * 200};
      controls.apply(touch(1, ActionPhase::started, start.x, start.y), true, false);
      controls.apply(touch(1, ActionPhase::changed, end.x, end.y), true, false);
      const auto forward = controls.sample();
      require_near(forward.movement.x, direction.x, .001F, "long sweep preserves requested horizontal direction");
      require_near(forward.movement.z, -direction.y, .001F, "long sweep preserves requested vertical direction");
      const auto anchor = controls.origin();
      controls.apply(touch(2, ActionPhase::started, width - 10, 20), true, true);
      controls.apply(touch(2, ActionPhase::changed, 10, 20), true, true);
      controls.apply(touch(2, ActionPhase::completed, 10, 20), true, true);
      require_near(controls.origin().x, anchor.x, 0, "HUD contact cannot move the sliding anchor");
      require_near(controls.origin().y, anchor.y, 0, "HUD contact cannot move the sliding anchor vertically");
      controls.apply(touch(1, ActionPhase::changed, end.x - direction.x * 88,
                           end.y - direction.y * 88), true, true);
      const auto reversed = controls.sample();
      require_near(reversed.movement.x, -direction.x, .001F, "full horizontal reversal is bounded over HUD");
      require_near(reversed.movement.z, direction.y, .001F, "full vertical reversal is bounded over HUD");
      controls.apply(touch(1, ActionPhase::changed, end.x, end.y), false, false);
      require(!controls.dragging(), "menu transition clears the sliding stick");
      controls.apply(touch(1, ActionPhase::changed, end.x, end.y), true, false);
      require_near(std::hypot(controls.sample().movement.x, controls.sample().movement.z), 0, 0,
                   "held contact after menu cannot silently reclaim flight");
    }
  }
}
void thumb_reversal_changes_actual_flight() {
  hr::product::Controls controls;
  hr::gameplay::Game game;
  game.start({});
  controls.apply(touch(1, ActionPhase::started, 100, 300), true, false);
  controls.apply(touch(1, ActionPhase::changed, 300, 300), true, false);
  for (unsigned frame = 0; frame < 24; ++frame) game.advance(1.F / 60.F, controls.sample());
  require(game.snapshot().velocity.x > 0, "long sweep moves the real saucer right");
  controls.apply(touch(1, ActionPhase::changed, 212, 300), true, false);
  for (unsigned frame = 0; frame < 30; ++frame) game.advance(1.F / 60.F, controls.sample());
  require(game.snapshot().velocity.x < 0, "bounded thumb reversal changes real saucer flight direction");
  controls.apply(touch(1, ActionPhase::completed, 212, 300), true, false);
  for (unsigned frame = 0; frame < 24; ++frame) game.advance(1.F / 60.F, controls.sample());
  require_near(game.snapshot().velocity.x, 0, .001F, "release brakes actual lateral flight");
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
  floating_anchor_reversals_and_precision();
  floating_anchor_direction_changes_and_ownership();
  thumb_reversal_changes_actual_flight();
  keyboard_sources_and_opposites();
  dash_axes_lifecycle_and_unplug();
}
