#include <human_resources/product/controls.h>
#include <algorithm>
#include <cmath>
namespace human_resources::product {
void Controls::clear() { *this = Controls{}; }
void Controls::remove_device(const input::DeviceId device) {
  for (auto& source : held_) if (source.occupied && source.device == device) source = {};
  if (contact_ && device_ == device) { contact_.reset(); flick_.cancel(); }
  if (flick_device_ == device) { dash_direction_ = {}; flick_device_ = 0; dash_ = false; }
}
void Controls::apply(const input::Action& a, bool movement_allowed, bool ui_claimed) {
  const auto id = static_cast<ControlAction>(a.id);
  const bool end = a.phase == input::ActionPhase::completed || a.phase == input::ActionPhase::cancelled;
  if (!movement_allowed) { clear(); return; }
  if (id == ControlAction::contact) {
    const bool owns = contact_ == a.contact && device_ == a.device;
    const auto accept_flick = [&](std::optional<input::Point> direction) {
      if (direction && !dash_) {
        dash_ = true;
        dash_direction_ = {direction->x, -direction->y};
        flick_device_ = a.device;
        flick_contact_ = a.contact;
      }
    };
    if (end) {
      if (owns) {
        if (a.phase == input::ActionPhase::completed) accept_flick(flick_.finish(a));
        flick_.cancel();
        contact_.reset();
      }
      // Cancellation can follow release in the same native event batch.
      if (a.phase == input::ActionPhase::cancelled && flick_device_ == a.device && flick_contact_ == a.contact) {
        dash_ = false; dash_direction_ = {}; flick_device_ = 0;
      }
    } else if (a.has_position && std::isfinite(a.position.x) && std::isfinite(a.position.y) && std::isfinite(a.value)) {
      if (a.phase == input::ActionPhase::started && !ui_claimed && !contact_ && a.device != input::invalid_device_id) {
        contact_ = a.contact; device_ = a.device;
        origin_ = {a.position.x, a.position.y}; position_ = origin_;
        flick_.start(a);
      } else if (owns) {
        const double dx = static_cast<double>(a.position.x) - origin_.x;
        const double dy = static_cast<double>(a.position.y) - origin_.y;
        const double scale = stick_radius / std::max(static_cast<double>(stick_radius), std::hypot(dx, dy));
        position_ = {origin_.x + static_cast<float>(dx * scale), origin_.y + static_cast<float>(dy * scale)};
        accept_flick(flick_.sample(a));
      }
    } else if (owns) flick_.cancel();
    return;
  }
  if (id == ControlAction::dash) {
    // A press and release can arrive in one frame; retain its rising edge.
    dash_ |= a.phase == input::ActionPhase::started;
    return;
  }
  if (id < ControlAction::left || id > ControlAction::axis_y) return;
  auto found = std::find_if(held_.begin(), held_.end(), [&](const auto& source) {
    return source.occupied && source.device == a.device && source.control == a.source_control &&
        source.kind == a.source_kind && source.action == id;
  });
  if (end || !std::isfinite(a.value) || a.value == 0) {
    if (found != held_.end()) *found = {};
    return;
  }
  if (found == held_.end()) found = std::find_if(held_.begin(), held_.end(), [](const auto& source) { return !source.occupied; });
  if (found == held_.end()) return;
  *found = {a.device, a.source_control, a.source_kind, id, std::clamp(a.value, -1.F, 1.F), true};
}
gameplay::Control Controls::sample() {
  std::array<float, 6> values{};
  for (const auto& source : held_) if (source.occupied) {
    auto& value = values[static_cast<unsigned>(source.action) - static_cast<unsigned>(ControlAction::left)];
    if (source.action <= ControlAction::down) value = std::max(value, source.value);
    else value = std::clamp(value + source.value, -1.F, 1.F);
  }
  gameplay::Control result{.movement = {values[1] - values[0] + values[4], values[2] - values[3] - values[5]},
      .dash = dash_, .dash_direction = dash_direction_};
  dash_ = false; dash_direction_ = {}; flick_device_ = 0;
  if (contact_) {
    const float x = (position_.x - origin_.x) / stick_radius;
    const float y = (position_.y - origin_.y) / stick_radius;
    const float magnitude = std::hypot(x,y);
    if (magnitude > stick_deadzone) {
      const float speed = std::min(1.F, (magnitude - stick_deadzone) / (1.F - stick_deadzone));
      result.movement.x += x / magnitude * speed;
      result.movement.z -= y / magnitude * speed;
    }
  }
  return result;
}
} // namespace human_resources::product
