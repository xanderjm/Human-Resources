#pragma once
#include <human_resources/lifecycle/lifecycle.h>

namespace human_resources::client {
// Focus loss and OS suspension are distinct boundaries. A focus interruption
// before a run exists must never consume that run's later background flush.
[[nodiscard]] constexpr bool interruption_checkpoint_due(const lifecycle::Transition& transition) noexcept {
  return transition.persist_bounded_state ||
         (transition.before.focus == lifecycle::Focus::focused && transition.after.focus == lifecycle::Focus::unfocused);
}
} // namespace human_resources::client
