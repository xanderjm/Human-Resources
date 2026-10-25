#include <human_resources/input/input.h>

#include "test_support.h"

#include <cmath>
#include <vector>

namespace {

using test_support::require;

void validation_rejects_ambiguous_bindings() {
  using namespace human_resources::input;
  const std::vector bindings{
      Binding{.action = 1U, .context = 1U, .device_kind = DeviceKind::keyboard, .control = 32},
      Binding{.action = 2U, .context = 1U, .device_kind = DeviceKind::keyboard, .control = 32},
  };
  const auto result = validate_bindings(bindings);
  require(result.issue == BindingIssue::duplicate && result.index == 1U, "duplicate source binding is rejected");
}

void translation_is_semantic_and_contextual() {
  using namespace human_resources::input;
  ActionMap actions({
      {.action = 10U, .context = 1U, .device_kind = DeviceKind::gamepad, .control = 4, .dead_zone = 0.2F},
      {.action = 20U, .context = 2U, .device_kind = DeviceKind::gamepad, .control = 4},
  });
  require(actions.validation().valid(), "valid action map accepted");
  const DeviceEvent source{
      .device = 7U,
      .user = 2U,
      .kind = DeviceKind::gamepad,
      .control = 4,
      .value = 0.75F,
  };
  const auto gameplay = actions.translate(source, 1U);
  const auto ui = actions.translate(source, 2U);
  require(gameplay.has_value() && gameplay->id == 10U, "gameplay context maps to gameplay action");
  require(ui.has_value() && ui->id == 20U, "UI context maps the same control independently");
  const auto gameplay_action = gameplay.value_or(Action{});
  require(gameplay_action.device == 7U && gameplay_action.user == 2U && gameplay_action.source_kind == DeviceKind::gamepad,
      "device, local user and input modality are retained");
}

void axes_are_bounded_and_dead_zoned() {
  using namespace human_resources::input;
  ActionMap actions({{
      .action = 3U,
      .context = 1U,
      .device_kind = DeviceKind::gamepad,
      .control = 9,
      .scale = -2.0F,
      .dead_zone = 0.25F,
  }});
  DeviceEvent source{.device = 1U, .kind = DeviceKind::gamepad, .control = 9, .value = 0.1F};
  const auto dead = actions.translate(source, 1U);
  require(dead.has_value() && dead->value == 0.0F, "scaled value inside dead zone resolves to zero");
  source.value = 0.75F;
  const auto bounded = actions.translate(source, 1U);
  require(bounded.has_value() && std::abs(bounded->value + 1.0F) < 0.0001F, "axis is scaled and bounded");
}

void devices_hot_plug_and_assign_local_users() {
  using namespace human_resources::input;
  DeviceRegistry devices;
  require(devices.connect(41U, DeviceKind::gamepad) == DeviceRegistryIssue::none,
      "gamepad connection is recorded");
  require(devices.connect(41U, DeviceKind::gamepad) == DeviceRegistryIssue::duplicate,
      "duplicate hot-plug event is rejected");
  require(devices.assign(41U, 2U) == DeviceRegistryIssue::none, "gamepad is assigned to a local user");
  require(devices.find(41U) != nullptr && devices.find(41U)->user == 2U,
      "local user assignment is queryable");
  require(devices.disconnect(41U) == DeviceRegistryIssue::none && devices.devices().empty(),
      "disconnect removes the device");
  require(devices.disconnect(41U) == DeviceRegistryIssue::not_found,
      "duplicate disconnect is safely rejected");
}

void ui_navigation_is_device_independent() {
  using namespace human_resources::input;
  ActionMap actions({
      Binding{.action = 90U, .context = 7U, .device_kind = DeviceKind::keyboard, .control = 40},
      Binding{.action = 90U, .context = 7U, .device_kind = DeviceKind::gamepad, .control = 0},
  });
  const auto keyboard = actions.translate(
      {.device = 1U, .kind = DeviceKind::keyboard, .control = 40, .value = 1.0F}, 7U);
  const auto gamepad = actions.translate(
      {.device = 2U, .kind = DeviceKind::gamepad, .control = 0, .value = 1.0F}, 7U);
  require(keyboard.has_value() && gamepad.has_value() && keyboard->id == gamepad->id,
      "keyboard and gamepad produce the same semantic UI action");
}

void pointer_identity_and_explicit_phase_survive_translation() {
  using namespace human_resources::input;
  ActionMap actions({{.action = 5U, .context = 1U, .device_kind = DeviceKind::touch, .control = 1}});
  const auto drag = actions.translate({.device = 9U, .kind = DeviceKind::touch, .control = 1, .value = 1.0F, .position = {40.0F, 80.0F}, .has_position = true, .contact = 0x100000002LL, .phase = ActionPhase::changed, .timestamp_ns = 9'123'456'789U}, 1U);
  require(drag && drag->phase == ActionPhase::changed && drag->contact == 0x100000002LL && drag->source_control == 1,
      "drag phase and full contact/source identity survive semantic mapping");
  require(drag && drag->timestamp_ns == 9'123'456'789U, "original event time survives queued semantic translation");
  require(drag && drag->source_kind == DeviceKind::touch, "touch modality survives semantic translation");
  const auto cancelled = actions.translate({.device = 9U, .kind = DeviceKind::touch, .control = 1, .contact = 0x100000002LL, .phase = ActionPhase::cancelled}, 1U);
  require(cancelled && cancelled->phase == ActionPhase::cancelled, "cancellation does not become ordinary release");
}

} // namespace

int main() {
  validation_rejects_ambiguous_bindings();
  translation_is_semantic_and_contextual();
  axes_are_bounded_and_dead_zoned();
  devices_hot_plug_and_assign_local_users();
  ui_navigation_is_device_independent();
  pointer_identity_and_explicit_phase_survive_translation();
  return 0;
}
