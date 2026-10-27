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
  controls.apply(touch(17, ActionPhase::changed, 168, 120), true, false);
  require(controls.dragging(), "first free contact owns floating stick");
  require_near(controls.sample().movement.x, 1, .001F, "forty-eight point drag gives full movement");
  controls.apply(touch(18, ActionPhase::started, 700, 340), true, true);
  controls.apply(touch(18, ActionPhase::cancelled, 700, 340), true, true);
  controls.apply(touch(17, ActionPhase::completed, 168, 120, 9), true, false);
  require(controls.dragging(), "UI finger or matching contact from another device cannot cancel movement");
  controls.apply(touch(17, ActionPhase::changed, 168, 120), true, true);
  require_near(controls.sample().movement.x, 1, .001F, "captured movement continues over HUD");
  controls.apply(touch(17, ActionPhase::cancelled, 168, 120), true, false);
  require(!controls.dragging(), "own cancellation releases movement");
  require_near(controls.sample().movement.x, 0, 0, "cancelled contact cannot drift");
  controls.apply(touch(2, ActionPhase::started, 120, 120), true, true);
  controls.apply(touch(2, ActionPhase::changed, 168, 120), true, false);
  require(!controls.dragging(), "contact begun on UI cannot turn into movement");
  controls.apply(touch(3, ActionPhase::started, 120, 120), true, false);
  controls.apply(touch(3, ActionPhase::changed, 168, 168), true, false);
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
void anchored_stick_precision_and_reversal() {
  for (float width : {320.F, 390.F, 768.F}) {
    for (const hr::ui::Point direction : {hr::ui::Point{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {.6F, .8F}, {-.6F, -.8F}}) {
      hr::product::Controls controls;
      const hr::ui::Point start{width * .5F, width * .8F};
      controls.apply(touch(1, ActionPhase::started, start.x, start.y), true, false);
      controls.apply(touch(1, ActionPhase::changed, start.x + direction.x * 200, start.y + direction.y * 200), true, false);
      require_near(controls.origin().x, start.x, 0, "Midnight Menu anchor stays at touchdown on long drags");
      require_near(controls.origin().y, start.y, 0, "anchor never slides vertically");
      const auto full = controls.sample();
      require_near(full.movement.x, direction.x, .001F, "radial full throw preserves direction");
      require_near(full.movement.z, -direction.y, .001F, "screen vertical maps to forward flight");
      require_near(std::hypot(controls.position().x-start.x, controls.position().y-start.y), 48, .001F, "visible knob stops on the same radial limit");
      controls.apply(touch(1, ActionPhase::changed, start.x, start.y), true, true);
      require_near(std::hypot(controls.sample().movement.x, controls.sample().movement.z), 0, 0, "return to touchdown is neutral even over HUD");
      controls.apply(touch(1, ActionPhase::changed, start.x-direction.x*48, start.y-direction.y*48), true, false);
      require_near(controls.sample().movement.x, -direction.x, .001F, "crossing the anchor reverses immediately");
      controls.clear();
      controls.apply(touch(1, ActionPhase::changed, start.x, start.y), true, false);
      require(!controls.dragging(), "cancelled finger must lift before recapturing");
    }
  }
  hr::product::Controls controls;
  controls.apply(touch(2, ActionPhase::started, 100, 300), true, false);
  controls.apply(touch(2, ActionPhase::changed, 107.2F, 300), true, false);
  require_near(controls.sample().movement.x, 0, .00001F, "15 percent deadzone ends without a speed jump");
  controls.apply(touch(2, ActionPhase::changed, 107.24F, 300), true, false);
  require(controls.sample().movement.x > 0 && controls.sample().movement.x < .002F, "small corrections leave deadzone continuously");
  controls.apply(touch(2, ActionPhase::changed, 127.6F, 300), true, false);
  require_near(controls.sample().movement.x, .5F, .001F, "remaining radial throw covers full analog range");
}
hr::input::Action timed(std::uint64_t ms, ActionPhase phase, float x, float y, std::uint64_t finger = 1) {
  auto action = touch(finger, phase, x, y); action.timestamp_ns = ms * 1'000'000; return action;
}
void flick_dash_direction_edges_and_rearm() {
  hr::product::Controls controls;
  controls.apply(timed(1000, ActionPhase::started, 100, 300), true, false);
  controls.apply(timed(1040, ActionPhase::changed, 148, 300), true, false);
  auto burst = controls.sample();
  require(burst.dash && burst.dash_direction.x == 1 && burst.dash_direction.z == 0, "held fast stroke dashes without requiring lift");
  require(controls.dragging() && !controls.sample().dash, "dash edge is consumed once while steering stays captured");
  controls.apply(timed(1060, ActionPhase::changed, 180, 300), true, false);
  require(!controls.sample().dash, "one fast stroke never repeats dash");
  controls.apply(timed(1200, ActionPhase::changed, 180, 300), true, false);
  controls.apply(timed(1240, ActionPhase::changed, 180, 252), true, false);
  burst = controls.sample();
  require(burst.dash && burst.dash_direction.z == 1, "held stick rearms after quiet then accepts an upward flick");
  require(burst.movement.x > 0 && burst.movement.z > 0 && burst.dash_direction.x == 0,
          "dash uses the flick stroke rather than old anchored-stick deflection");
  controls.apply(timed(1245, ActionPhase::cancelled, 180, 252), true, false);
  require(!controls.dragging() && !controls.sample().dash, "native cancellation drops stick and pending gesture");
  controls.apply(timed(2000, ActionPhase::started, 100, 300), true, false);
  controls.apply(timed(2040, ActionPhase::completed, 52, 300), true, false);
  burst = controls.sample();
  require(burst.dash && burst.dash_direction.x == -1 && !controls.dragging(), "release-only fast stroke retains its direction");
  hr::gameplay::Game game;
  game.start();
  game.advance(1.F/240, burst); // Deliberately no fixed tick yet.
  game.advance(1.F/60, {});
  require(game.snapshot().player.x < 0, "short flick direction survives to the first actual gameplay tick");
}
void flick_rejects_slow_jitter_bad_time_and_other_fingers() {
  for (unsigned scenario = 0; scenario < 7; ++scenario) {
    hr::product::Controls controls;
    controls.apply(timed(1000, ActionPhase::started, 100, 300), true, false);
    auto action = timed(1040, ActionPhase::changed, 148, 300);
    if (scenario == 0) action.timestamp_ns = 1300'000'000; // Slow drag.
    if (scenario == 1) action.position.x = 106; // Lift jitter.
    if (scenario == 2) action.timestamp_ns = 0;
    if (scenario == 3) action.timestamp_ns = 999'000'000;
    if (scenario == 4) action.timestamp_ns = 1000'000'000;
    if (scenario == 5) action.contact = 2;
    if (scenario == 6) action.position.x = std::numeric_limits<float>::quiet_NaN();
    controls.apply(action, true, false);
    require(!controls.sample().dash, "slow/jitter/invalid/foreign sample never invents a dash");
  }
  hr::product::Controls controls;
  controls.apply(timed(1000, ActionPhase::started, 100, 300), true, true);
  controls.apply(timed(1040, ActionPhase::changed, 148, 300), true, false);
  require(!controls.dragging() && !controls.sample().dash, "HUD-started contact never turns into a flick");
  controls.apply(timed(2000, ActionPhase::started, 100, 300), true, false);
  controls.apply(timed(2040, ActionPhase::changed, 148, 300), true, false);
  controls.remove_device(1);
  require(!controls.dragging() && !controls.sample().dash, "unplug cancels unconsumed flick");
}
void thumb_reversal_changes_actual_flight() {
  hr::product::Controls controls;
  hr::gameplay::Game game;
  game.start({});
  controls.apply(touch(1, ActionPhase::started, 100, 300), true, false);
  controls.apply(touch(1, ActionPhase::changed, 148, 300), true, false);
  for (unsigned frame = 0; frame < 15; ++frame) game.advance(1.F / 60.F, controls.sample());
  require(game.snapshot().velocity.x > 0, "stick moves the real saucer right");
  controls.apply(touch(1, ActionPhase::changed, 52, 300), true, false);
  for (unsigned frame = 0; frame < 8; ++frame) game.advance(1.F / 60.F, controls.sample());
  require(game.snapshot().velocity.x < 0, "crossing anchor reverses actual flight within 134ms");
  controls.apply(touch(1, ActionPhase::completed, 52, 300), true, false);
  const float before = game.snapshot().player.x;
  for (unsigned frame = 0; frame < 24; ++frame) game.advance(1.F / 60.F, controls.sample());
  require_near(game.snapshot().velocity.x, 0, .001F, "release brakes actual lateral flight");
  require(std::abs(game.snapshot().player.x-before) < .3F, "release coasts less than a third of a metre");
}
void flowing_dash_distance_pause_continuation_and_bounds() {
  for (unsigned hz : {30U, 60U, 120U}) {
    hr::gameplay::Game burst, ordinary;
    burst.start({.seed=514}); ordinary.start({.seed=514});
    float previous = 0, previous_step = 1;
    bool paused_checked = false;
    for (unsigned frame = 0; frame < hz / 2; ++frame) {
      burst.advance(1.F/static_cast<float>(hz), {{}, frame == 0, {1, 0}});
      ordinary.advance(1.F/static_cast<float>(hz));
      const float extra = burst.snapshot().player.x - ordinary.snapshot().player.x;
      if (extra > 0 && previous == 0) require(extra < 1, "dash starts now but never teleports its full distance");
      if (hz <= 60 && frame > 0 && frame < hz / 10) {
        require(extra > previous && extra - previous < previous_step, "burst decelerates toward its catch");
      }
      previous_step = extra - previous; previous = extra;
      require(extra <= 2.00001F, "unobstructed starter dash never overshoots two metres");
      const auto bytes = burst.save_state();
      hr::gameplay::Game cold;
      require(!bytes.empty() && cold.restore_state(bytes) == hr::gameplay::SaveStateIssue::none && cold.save_state() == bytes,
          "every real dash phase cold restores byte exactly");
      if (!paused_checked && extra > 0) {
        paused_checked = true;
        burst.pause(); const auto paused = burst.save_state();
        burst.advance(10, {{-1,0}, true});
        require(burst.save_state() == paused, "pause freezes burst distance, protection and cooldown");
        burst.resume();
      }
      // Compare actual future at every point, including during the burst.
      hr::gameplay::Game live;
      require(live.restore_state(bytes) == hr::gameplay::SaveStateIssue::none, "second continuation restores");
      for (unsigned step=0;step<12;++step) {
        live.advance(1.F/60, {{0,.2F}, false}); cold.advance(1.F/60, {{0,.2F}, false});
        require(live.save_state()==cold.save_state(), "saved burst direction and remaining travel produce an exact future");
      }
    }
    require_near(previous, 2, .00001F, "30/60/120Hz retain exact two-metre burst travel");
    require(burst.snapshot().dash_cooldown_seconds > 3.4F, "burst keeps the existing four-second cooldown");
  }
  hr::gameplay::Game game; game.start();
  for (unsigned i=0;i<70;++i) game.advance(1.F/60, {{1,0}, false});
  game.advance(1.F/60, {{1,0}, true});
  for (unsigned i=0;i<20;++i) game.advance(1.F/60, {{1,0}, false});
  require(game.snapshot().player.x <= 7.5F, "burst remains inside the real field edge");
  game.advance(1.F/60, {{-1,0}, false});
  require(game.snapshot().velocity.x < 0, "edge absorbs outward inertia so inward steering responds on the next tick");
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
  anchored_stick_precision_and_reversal();
  flick_dash_direction_edges_and_rearm();
  flick_rejects_slow_jitter_bad_time_and_other_fingers();
  thumb_reversal_changes_actual_flight();
  keyboard_sources_and_opposites();
  flowing_dash_distance_pause_continuation_and_bounds();
  dash_axes_lifecycle_and_unplug();
}
