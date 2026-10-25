#pragma once

#include <human_resources/accessibility/session.h>
#include <human_resources/platform/sdl_platform.h>

#include <memory>

namespace human_resources::platform {

class NativeAccessibility {
public:
  virtual ~NativeAccessibility() = default;
  [[nodiscard]] virtual accessibility::Preferences preferences() const = 0;
  virtual void update(SDL_Window* window, const WindowMetrics& metrics) = 0;
};

// Lifetime belongs to the surface's composition root. The session keeps SDK
// callbacks outside product state and validates them when the product drains it.
[[nodiscard]] std::unique_ptr<NativeAccessibility> create_native_accessibility(
    std::shared_ptr<accessibility::Session> session);

} // namespace human_resources::platform
