#include <human_resources/lifecycle/lifecycle.h>

#include "test_support.h"
#include "../human_resources_client/src/interruption_policy.h"

namespace {

using test_support::require;

void suspension_requests_bounded_persistence() {
  using namespace human_resources::lifecycle;
  State state;
  const auto transition = apply(state, {.kind = EventKind::suspending});
  require(state.presence == Presence::suspended, "suspend updates presence");
  require(transition.pause_product, "suspend requests product pause");
  require(transition.persist_bounded_state, "suspend requests bounded persistence");
}

void resume_resets_wall_clock_accumulation() {
  using namespace human_resources::lifecycle;
  State state{.presence = Presence::suspended, .focus = Focus::unfocused};
  const auto transition = apply(state, {.kind = EventKind::resumed});
  require(state.presence == Presence::foreground, "resume returns to foreground");
  require(transition.reset_wall_clock, "resume prevents catch-up from suspended wall time");
}

void memory_pressure_only_releases_disposable_caches() {
  using namespace human_resources::lifecycle;
  State state;
  const auto transition = apply(state, {
                                           .kind = EventKind::memory_changed,
                                           .memory = MemoryPressure::critical,
                                       });
  require(state.memory == MemoryPressure::critical, "memory state is retained");
  require(transition.release_disposable_caches, "critical pressure requests disposable cache release");
  require(!transition.pause_product && !transition.persist_bounded_state,
      "memory pressure does not invent pause or persistence policy");
}

void thermal_and_power_policy_is_retained_without_inventing_gameplay() {
  using namespace human_resources::lifecycle;
  State state;
  const auto thermal = apply(state, {.kind = EventKind::thermal_changed, .thermal = ThermalPressure::serious});
  require(thermal.changed && state.thermal == ThermalPressure::serious, "thermal pressure is retained");
  require(!thermal.pause_product && !thermal.persist_bounded_state,
      "thermal pressure does not invent product policy");
  const auto power = apply(state, {.kind = EventKind::power_changed, .power = PowerMode::constrained});
  require(power.changed && state.power == PowerMode::constrained, "constrained power state is retained");
}

void focus_before_start_cannot_consume_real_background_flush() {
  using namespace human_resources::lifecycle;
  State state;
  bool run_exists = false;
  unsigned writes = 0;
  unsigned current_checkpoint = 0, persisted_checkpoint = 0;
  const auto deliver = [&](EventKind kind) {
    const auto transition = apply(state, {.kind = kind});
    if (human_resources::client::interruption_checkpoint_due(transition) && run_exists) {
      persisted_checkpoint = current_checkpoint;
      ++writes;
    }
  };
  // Android's fullscreen tutorial may remove focus before Start. The run then
  // begins while lifecycle presence is still foreground, with no focus-gained
  // event before the later real onPause/onStop sequence.
  deliver(EventKind::focus_lost);
  require(writes == 0 && state.presence == Presence::foreground, "pre-Start focus loss has no run to flush and is not OS suspension");
  run_exists = true;
  current_checkpoint = 10016;
  deliver(EventKind::entered_background);
  require(writes == 1 && persisted_checkpoint == 10016, "real OS background saves current run despite earlier pre-Start focus loss");
  deliver(EventKind::entered_background); // SDL WILL then DID.
  deliver(EventKind::focus_lost);
  require(writes == 1, "duplicate WILL/DID and focus events do not duplicate the same flush");
  deliver(EventKind::entered_foreground);
  deliver(EventKind::focus_gained);
  current_checkpoint = 14000;
  deliver(EventKind::focus_lost);
  require(writes == 2 && persisted_checkpoint == 14000, "a later genuine focus interruption still flushes current state");
  deliver(EventKind::entered_background);
  require(writes == 3, "actual OS suspension remains its own durable boundary after focus loss");
  deliver(EventKind::entered_background);
  require(writes == 3, "second OS notification remains deduplicated");
  deliver(EventKind::entered_foreground);
  deliver(EventKind::suspending); // Termination may arrive without focus loss.
  require(writes == 4, "genuine termination suspension requests a final bounded checkpoint");
  deliver(EventKind::suspending);
  require(writes == 4, "duplicate termination notification is idempotent");
}

} // namespace

int main() {
  focus_before_start_cannot_consume_real_background_flush();
  suspension_requests_bounded_persistence();
  resume_resets_wall_clock_accumulation();
  memory_pressure_only_releases_disposable_caches();
  thermal_and_power_policy_is_retained_without_inventing_gameplay();
  return 0;
}
