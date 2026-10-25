#include <human_resources/product/controls.h>
#include <algorithm>
#include <cmath>
namespace human_resources::product {
void Controls::clear() { *this = Controls{}; }
void Controls::remove_device(const input::DeviceId device) {
  for (auto& source : held_) if (source.occupied && source.device == device) source = {};
  if (contact_ && device_ == device) contact_.reset();
}
void Controls::apply(const input::Action& a, bool movement_allowed, bool ui_claimed) {
  const auto id = static_cast<ControlAction>(a.id);
  const bool end = a.phase == input::ActionPhase::completed || a.phase == input::ActionPhase::cancelled;
  if (!movement_allowed) { clear(); return; }
  if (id == ControlAction::contact) {
    if (end) {
      if (contact_ == a.contact && device_ == a.device) contact_.reset();
    } else if (a.has_position && std::isfinite(a.position.x) && std::isfinite(a.position.y)) {
      if (a.phase == input::ActionPhase::started && !ui_claimed && !contact_) {
        contact_ = a.contact; device_ = a.device;
        origin_ = {a.position.x, a.position.y}; position_ = origin_;
      } else if (contact_ == a.contact && device_ == a.device) position_ = {a.position.x, a.position.y};
    }
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
  gameplay::Control result{{values[1] - values[0] + values[4], values[2] - values[3] - values[5]}, dash_};
  dash_ = false;
  if (contact_) {
    float x = (position_.x - origin_.x) / 44.F, y = (position_.y - origin_.y) / 44.F;
    const float magnitude = std::hypot(x,y);
    if (magnitude > 1) { x /= magnitude; y /= magnitude; }
    if (magnitude > .1F) { result.movement.x += x; result.movement.z -= y; }
  }
  return result;
}
} // namespace human_resources::product
