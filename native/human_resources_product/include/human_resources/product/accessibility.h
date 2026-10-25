#pragma once

#include <human_resources/accessibility/session.h>
#include <human_resources/product/presentation.h>

namespace human_resources::product {

// Rebuilt on the product thread from the exact visible hit regions. Stable
// identities include offer/purchase context, so native callbacks cannot choose
// a replacement upgrade or buy a different rank after the screen changes.
[[nodiscard]] std::vector<ui::AccessibilityNode> accessibility_snapshot(
    const View& view, const Interface& interface, Command focused = Command::none);

// Resolve only actions already validated by accessibility::Session::drain_actions.
// The current visible interface is checked again before product dispatch.
[[nodiscard]] Command accessibility_command(const accessibility::Action& action,
    const Interface& interface);

} // namespace human_resources::product
