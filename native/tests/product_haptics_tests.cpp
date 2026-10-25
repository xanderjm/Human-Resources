#include "test_support.h"
#include <human_resources/audio/product_haptics.h>

using human_resources::feedback::HapticCue;
using human_resources::feedback::HapticGate;
using test_support::require;

int main() {
  HapticGate gate;
  require(!gate.accept(HapticCue::capture, 0), "no feedback before active play");
  gate.set_policy(true, true);
  require(gate.accept(HapticCue::capture, 0), "first capture at zero timestamp plays");
  require(!gate.accept(HapticCue::capture, 0), "same-frame duplicate suppressed");
  require(!gate.accept(HapticCue::capture, 299), "capture rate bounded");
  require(gate.accept(HapticCue::capture, 300), "capture rate resumes at exact boundary");
  require(gate.accept(HapticCue::hull_hit, 300), "urgent hull damage preempts capture");
  require(!gate.accept(HapticCue::shield_hit, 301), "weaker event cannot replace hull impact");
  require(gate.accept(HapticCue::death, 301), "death preempts incidental feedback");
  require(!gate.accept(HapticCue::death, 1300), "death cannot repeat before cooldown");
  require(gate.accept(HapticCue::death, 1301), "death cooldown exact boundary");
  require(!gate.accept(HapticCue::capture, 1200), "clock rollback is rejected");
  gate.set_policy(false, true);
  require(!gate.accept(HapticCue::upgrade, 2000), "saved disabled preference suppresses every cue");
  gate.set_policy(true, false);
  require(!gate.accept(HapticCue::upgrade, 3000), "background and pause discard cue");
  gate.set_policy(true, true);
  require(gate.accept(HapticCue::upgrade, 3000), "discarded cue was not scheduled or consumed");
  require(!gate.accept(HapticCue::count, 4000), "invalid cue rejected");
  require(!gate.accept(static_cast<HapticCue>(255), 4000), "untrusted enum rejected");
  unsigned pulses = 0;
  HapticGate dense;
  dense.set_policy(true, true);
  for (std::uint64_t ms = 0; ms < 60000; ++ms)
    if (dense.accept(HapticCue::capture, ms)) ++pulses;
  require(pulses == 200, "dense capture event stream stays bounded over a full minute");
  return 0;
}
