#pragma once

#include <cstdint>

namespace human_resources::lifecycle {

enum class Presence : std::uint8_t {
  foreground,
  background,
  suspended,
};

enum class Focus : std::uint8_t {
  focused,
  unfocused,
};

enum class MemoryPressure : std::uint8_t {
  normal,
  warning,
  critical,
};

enum class ThermalPressure : std::uint8_t {
  nominal,
  elevated,
  serious,
  critical,
};

enum class PowerMode : std::uint8_t {
  normal,
  constrained,
};

enum class EventKind : std::uint8_t {
  entered_foreground,
  entered_background,
  suspending,
  resumed,
  focus_gained,
  focus_lost,
  memory_changed,
  thermal_changed,
  power_changed,
};

struct Event final {
  EventKind kind{EventKind::entered_foreground};
  MemoryPressure memory{MemoryPressure::normal};
  ThermalPressure thermal{ThermalPressure::nominal};
  PowerMode power{PowerMode::normal};
};

struct State final {
  Presence presence{Presence::foreground};
  Focus focus{Focus::focused};
  MemoryPressure memory{MemoryPressure::normal};
  ThermalPressure thermal{ThermalPressure::nominal};
  PowerMode power{PowerMode::normal};
};

struct Transition final {
  State before;
  State after;
  bool changed{};
  bool pause_product{};
  bool persist_bounded_state{};
  bool reset_wall_clock{};
  bool release_disposable_caches{};
};

[[nodiscard]] Transition apply(State& state, const Event& event) noexcept;

} // namespace human_resources::lifecycle
