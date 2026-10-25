#pragma once

#include <SDL3/SDL.h>

#include <human_resources/assets/assets.h>
#include <human_resources/diagnostics/diagnostics.h>
#include <human_resources/input/input.h>
#include <human_resources/lifecycle/lifecycle.h>
#include <human_resources/storage/storage.h>
#include <human_resources/ui/geometry.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace human_resources::platform {

// SDL numbers buttons and axes from zero; bindings need disjoint source IDs.
inline constexpr std::int32_t gamepad_axis_control_base = 0x100;
// Matches translated gamepad actions; use for product cancellation on unplug.
[[nodiscard]] human_resources::input::DeviceId gamepad_device_id(SDL_JoystickID instance_id) noexcept;

struct WindowMetrics final {
  human_resources::ui::Rect viewport{};
  human_resources::ui::Insets safe_area{};
  float display_scale{1.0F};
  float window_units_per_point{1.0F};
  bool valid{};
};

[[nodiscard]] WindowMetrics normalize_window_metrics(human_resources::ui::Size window_size,
    human_resources::ui::Insets window_safe_area, float pixel_density, float display_scale) noexcept;
[[nodiscard]] human_resources::ui::Rect window_coordinates(
    human_resources::ui::Rect rect, const WindowMetrics& metrics) noexcept;
[[nodiscard]] WindowMetrics query_window_metrics(SDL_Window* window) noexcept;
[[nodiscard]] std::optional<human_resources::ui::Point> pointer_position(const SDL_Event& event,
    const WindowMetrics& metrics) noexcept;
[[nodiscard]] std::optional<human_resources::input::DeviceEvent> translate_input_event(
    const SDL_Event& event, const WindowMetrics& metrics) noexcept;
[[nodiscard]] std::optional<human_resources::lifecycle::Event> translate_lifecycle_event(
    const SDL_Event& event) noexcept;

class SdlInputDevices final {
public:
  SdlInputDevices() = default;
  ~SdlInputDevices();
  SdlInputDevices(const SdlInputDevices&) = delete;
  SdlInputDevices& operator=(const SdlInputDevices&) = delete;

  void discover() noexcept;
  void handle_device_event(const SDL_Event& event) noexcept;

private:
  void open(SDL_JoystickID instance_id) noexcept;
  void close(SDL_JoystickID instance_id) noexcept;
  std::array<SDL_Gamepad*, 16> gamepads_{};
  std::size_t gamepad_count_{};
};

class SdlStorageBackend final : public human_resources::storage::Backend {
public:
  SdlStorageBackend(std::string_view organization, std::string_view application);
  explicit SdlStorageBackend(std::string preference_root);

  [[nodiscard]] bool available() const noexcept {
    return !preference_root_.empty();
  }
  [[nodiscard]] human_resources::storage::ReadResult read(human_resources::storage::Scope scope,
      const human_resources::storage::RelativePath& path, std::size_t maximum_bytes) override;
  human_resources::storage::WriteResult write_atomically(human_resources::storage::Scope scope,
      const human_resources::storage::RelativePath& path, std::span<const std::byte> bytes) override;

private:
  std::string preference_root_;
};

class SdlAssetSource final : public human_resources::assets::Source {
public:
  SdlAssetSource();
  explicit SdlAssetSource(std::string root);

  [[nodiscard]] bool available() const noexcept {
    return !root_.empty();
  }
  [[nodiscard]] human_resources::assets::LoadResult load(
      const human_resources::assets::Record& record) override;

private:
  std::string root_;
};

class SdlLogSink final : public human_resources::diagnostics::Sink {
public:
  void write(const human_resources::diagnostics::Record& record) noexcept override;
};

} // namespace human_resources::platform
