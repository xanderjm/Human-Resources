#pragma once
#include "run_state.h"

namespace human_resources::gameplay::detail {
struct EncounterWindow final {
  float soldier_multiplier{}, heavy_multiplier{};
  unsigned warning_cap{}, formation{};
  bool recovery{}, boss_ingress{};
};
[[nodiscard]] EncounterWindow encounter_window(double seconds);
} // namespace human_resources::gameplay::detail
