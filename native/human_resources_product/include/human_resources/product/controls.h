#pragma once
#include <human_resources/input/input.h>
#include <human_resources/product/flick_gesture.h>
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/ui/geometry.h>
#include <optional>
#include <array>
namespace human_resources::product {
enum class ControlAction : input::ActionId { contact = 1, left, right, up, down, axis_x, axis_y, accept, pause, dash, pick0, pick1, pick2 };
// Product drag policy: a floating stick claims one contact outside UI. Additional
// fingers may press HUD controls without stealing or ending the movement drag.
// Midnight Menu policy: touchdown stays anchored, with a 48pt radial throw and
// continuous 15% deadzone. A fast captured stroke requests a directional dash.
class Controls final {
public:
  static constexpr float stick_radius = 48.F, stick_deadzone = .15F;
  void apply(const input::Action& action, bool movement_allowed, bool ui_claimed);
  void clear();
  // Device unplugging cancels only that device, preserving a held keyboard or drag.
  void remove_device(input::DeviceId device);
  [[nodiscard]] gameplay::Control sample();
  [[nodiscard]] bool dragging() const { return contact_.has_value(); }
  [[nodiscard]] ui::Point origin() const { return origin_; }
  [[nodiscard]] ui::Point position() const { return position_; }
private:
  std::optional<std::uint64_t> contact_;
  input::DeviceId device_{};
  ui::Point origin_, position_;
  struct HeldSource final {
    input::DeviceId device{};
    std::int32_t control{};
    input::DeviceKind kind{};
    ControlAction action{};
    float value{};
    bool occupied{};
  };
  // Eight keyboard movement bindings plus axes on sixteen connected controllers.
  // Fixed storage keeps event processing bounded and allocation-free.
  std::array<HeldSource, 64> held_{};
  bool dash_{};
  FlickGesture flick_;
  gameplay::Vec2 dash_direction_{};
  input::DeviceId flick_device_{};
  std::uint64_t flick_contact_{};
};
} // namespace human_resources::product
