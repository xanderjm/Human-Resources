#include <human_resources/lifecycle/lifecycle.h>

namespace human_resources::lifecycle {
namespace {

bool equal(const State& first, const State& second) noexcept {
  return first.presence == second.presence && first.focus == second.focus && first.memory == second.memory &&
         first.thermal == second.thermal && first.power == second.power;
}

} // namespace

Transition apply(State& state, const Event& event) noexcept {
  Transition transition{.before = state, .after = state};
  switch (event.kind) {
  case EventKind::entered_foreground:
  case EventKind::resumed:
    transition.after.presence = Presence::foreground;
    transition.reset_wall_clock = true;
    break;
  case EventKind::entered_background:
    transition.after.presence = Presence::background;
    transition.pause_product = true;
    transition.persist_bounded_state = true;
    break;
  case EventKind::suspending:
    transition.after.presence = Presence::suspended;
    transition.pause_product = true;
    transition.persist_bounded_state = true;
    break;
  case EventKind::focus_gained:
    transition.after.focus = Focus::focused;
    break;
  case EventKind::focus_lost:
    transition.after.focus = Focus::unfocused;
    break;
  case EventKind::memory_changed:
    transition.after.memory = event.memory;
    transition.release_disposable_caches = event.memory != MemoryPressure::normal;
    break;
  case EventKind::thermal_changed:
    transition.after.thermal = event.thermal;
    break;
  case EventKind::power_changed:
    transition.after.power = event.power;
    break;
  }
  transition.changed = !equal(transition.before, transition.after);
  if (!transition.changed) {
    transition.pause_product = false;
    transition.persist_bounded_state = false;
    transition.reset_wall_clock = false;
    transition.release_disposable_caches = false;
  }
  state = transition.after;
  return transition;
}

} // namespace human_resources::lifecycle
