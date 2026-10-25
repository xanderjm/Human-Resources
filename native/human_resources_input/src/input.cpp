#include <human_resources/input/input.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace human_resources::input {
namespace {

bool same_source(const Binding& first, const Binding& second) noexcept {
  return first.context == second.context && first.device_kind == second.device_kind && first.control == second.control;
}

ActionPhase phase_for(const float value) noexcept {
  if (value == 0.0F)
    return ActionPhase::completed;
  if (std::abs(value) == 1.0F)
    return ActionPhase::started;
  return ActionPhase::changed;
}

float normalized_value(const DeviceEvent& event, const Binding& binding) noexcept {
  if (!std::isfinite(event.value))
    return 0.0F;
  const float scaled = std::clamp(event.value * binding.scale, -1.0F, 1.0F);
  return std::abs(scaled) < binding.dead_zone ? 0.0F : scaled;
}

} // namespace

DeviceRegistryIssue DeviceRegistry::connect(const DeviceId id, const DeviceKind kind) {
  if (id == invalid_device_id)
    return DeviceRegistryIssue::invalid_device;
  if (find(id) != nullptr)
    return DeviceRegistryIssue::duplicate;
  devices_.push_back({.id = id, .kind = kind});
  return DeviceRegistryIssue::none;
}

DeviceRegistryIssue DeviceRegistry::disconnect(const DeviceId id) noexcept {
  const auto found = std::find_if(devices_.begin(), devices_.end(),
      [id](const DeviceRecord& record) { return record.id == id; });
  if (found == devices_.end())
    return DeviceRegistryIssue::not_found;
  devices_.erase(found);
  return DeviceRegistryIssue::none;
}

DeviceRegistryIssue DeviceRegistry::assign(const DeviceId id, const LocalUserId user) noexcept {
  if (user == unassigned_user_id)
    return DeviceRegistryIssue::invalid_user;
  const auto found = std::find_if(devices_.begin(), devices_.end(),
      [id](const DeviceRecord& record) { return record.id == id; });
  if (found == devices_.end())
    return DeviceRegistryIssue::not_found;
  found->user = user;
  return DeviceRegistryIssue::none;
}

const DeviceRecord* DeviceRegistry::find(const DeviceId id) const noexcept {
  const auto found = std::find_if(devices_.begin(), devices_.end(),
      [id](const DeviceRecord& record) { return record.id == id; });
  return found == devices_.end() ? nullptr : &*found;
}

BindingValidation validate_bindings(const std::span<const Binding> bindings) noexcept {
  for (std::size_t index = 0; index < bindings.size(); ++index) {
    const Binding& binding = bindings[index];
    if (binding.action == invalid_action_id)
      return {.issue = BindingIssue::invalid_action, .index = index};
    if (binding.context == invalid_context_id)
      return {.issue = BindingIssue::invalid_context, .index = index};
    if (!std::isfinite(binding.scale) || binding.scale == 0.0F)
      return {.issue = BindingIssue::invalid_scale, .index = index};
    if (!std::isfinite(binding.dead_zone) || binding.dead_zone < 0.0F || binding.dead_zone >= 1.0F)
      return {.issue = BindingIssue::invalid_dead_zone, .index = index};
    for (std::size_t earlier = 0; earlier < index; ++earlier) {
      if (same_source(bindings[earlier], binding))
        return {.issue = BindingIssue::duplicate, .index = index};
    }
  }
  return {};
}

ActionMap::ActionMap(std::vector<Binding> bindings)
    : bindings_(std::move(bindings)), validation_(validate_bindings(bindings_)) {}

std::optional<Action> ActionMap::translate(const DeviceEvent& event, const ContextId context) const noexcept {
  if (!validation_.valid() || event.device == invalid_device_id || context == invalid_context_id)
    return std::nullopt;
  const auto binding = std::find_if(bindings_.begin(), bindings_.end(), [&](const Binding& candidate) {
    return candidate.context == context && candidate.device_kind == event.kind && candidate.control == event.control;
  });
  if (binding == bindings_.end())
    return std::nullopt;
  const float value = normalized_value(event, *binding);
  return Action{
      .id = binding->action,
      .context = context,
      .device = event.device,
      .user = event.user,
      .phase = event.phase.value_or(phase_for(value)),
      .value = value,
      .position = event.position,
      .has_position = event.has_position,
      .source_control = event.control,
      .contact = event.contact,
      .timestamp_ns = event.timestamp_ns,
      .source_kind = event.kind,
  };
}

} // namespace human_resources::input
