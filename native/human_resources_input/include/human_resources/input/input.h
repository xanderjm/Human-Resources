#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace human_resources::input {

using ActionId = std::uint32_t;
using ContextId = std::uint32_t;
using DeviceId = std::uint64_t;
using LocalUserId = std::uint32_t;

inline constexpr ActionId invalid_action_id = 0U;
inline constexpr ContextId invalid_context_id = 0U;
inline constexpr DeviceId invalid_device_id = 0U;
inline constexpr LocalUserId unassigned_user_id = 0U;

enum class DeviceKind : std::uint8_t {
  keyboard,
  pointer,
  touch,
  gamepad,
};

enum class ActionPhase : std::uint8_t {
  started,
  changed,
  completed,
  cancelled,
};

struct Point final {
  float x{};
  float y{};
};

struct DeviceEvent final {
  DeviceId device{invalid_device_id};
  LocalUserId user{unassigned_user_id};
  DeviceKind kind{DeviceKind::keyboard};
  std::int32_t control{};
  float value{};
  Point position{};
  bool has_position{};
  // Pointer contacts retain their full identity and explicit drag/cancel phase.
  std::uint64_t contact{};
  std::optional<ActionPhase> phase;
  // Monotonic event time in nanoseconds; zero means unavailable.
  std::uint64_t timestamp_ns{};
};

struct Binding final {
  ActionId action{invalid_action_id};
  ContextId context{invalid_context_id};
  DeviceKind device_kind{DeviceKind::keyboard};
  std::int32_t control{};
  float scale{1.0F};
  float dead_zone{};
};

struct Action final {
  ActionId id{invalid_action_id};
  ContextId context{invalid_context_id};
  DeviceId device{invalid_device_id};
  LocalUserId user{unassigned_user_id};
  ActionPhase phase{ActionPhase::changed};
  float value{};
  Point position{};
  bool has_position{};
  std::int32_t source_control{};
  std::uint64_t contact{};
  std::uint64_t timestamp_ns{};
  DeviceKind source_kind{DeviceKind::keyboard};
};

struct DeviceRecord final {
  DeviceId id{invalid_device_id};
  DeviceKind kind{DeviceKind::keyboard};
  LocalUserId user{unassigned_user_id};
};

enum class DeviceRegistryIssue : std::uint8_t {
  none,
  invalid_device,
  invalid_user,
  duplicate,
  not_found,
};

class DeviceRegistry final {
public:
  [[nodiscard]] DeviceRegistryIssue connect(DeviceId id, DeviceKind kind);
  [[nodiscard]] DeviceRegistryIssue disconnect(DeviceId id) noexcept;
  [[nodiscard]] DeviceRegistryIssue assign(DeviceId id, LocalUserId user) noexcept;
  [[nodiscard]] const DeviceRecord* find(DeviceId id) const noexcept;
  [[nodiscard]] std::span<const DeviceRecord> devices() const noexcept {
    return devices_;
  }

private:
  std::vector<DeviceRecord> devices_;
};

enum class BindingIssue : std::uint8_t {
  none,
  invalid_action,
  invalid_context,
  invalid_scale,
  invalid_dead_zone,
  duplicate,
};

struct BindingValidation final {
  BindingIssue issue{BindingIssue::none};
  std::size_t index{};

  [[nodiscard]] bool valid() const noexcept {
    return issue == BindingIssue::none;
  }
};

[[nodiscard]] BindingValidation validate_bindings(std::span<const Binding> bindings) noexcept;

class ActionMap final {
public:
  explicit ActionMap(std::vector<Binding> bindings);

  [[nodiscard]] const BindingValidation& validation() const noexcept {
    return validation_;
  }
  [[nodiscard]] std::optional<Action> translate(const DeviceEvent& event, ContextId context) const noexcept;

private:
  std::vector<Binding> bindings_;
  BindingValidation validation_;
};

} // namespace human_resources::input
