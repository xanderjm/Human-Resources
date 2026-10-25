#include <human_resources/platform/sdl_platform.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace human_resources::platform {
namespace {

constexpr human_resources::input::DeviceId keyboard_device_base = 0x100000000ULL;
constexpr human_resources::input::DeviceId pointer_device_base = 0x200000000ULL;
constexpr human_resources::input::DeviceId touch_device_base = 0x300000000ULL;
constexpr human_resources::input::DeviceId gamepad_device_base = 0x400000000ULL;
std::atomic_uint64_t next_temporary_file{1U};

SDL_LogPriority priority(const human_resources::diagnostics::Severity severity) noexcept {
  using human_resources::diagnostics::Severity;
  switch (severity) {
  case Severity::trace:
    return SDL_LOG_PRIORITY_TRACE;
  case Severity::debug:
    return SDL_LOG_PRIORITY_DEBUG;
  case Severity::info:
    return SDL_LOG_PRIORITY_INFO;
  case Severity::warning:
    return SDL_LOG_PRIORITY_WARN;
  case Severity::error:
    return SDL_LOG_PRIORITY_ERROR;
  case Severity::critical:
    return SDL_LOG_PRIORITY_CRITICAL;
  }
  return SDL_LOG_PRIORITY_INFO;
}

int category(const human_resources::diagnostics::Subsystem subsystem) noexcept {
  using human_resources::diagnostics::Subsystem;
  switch (subsystem) {
  case Subsystem::renderer:
    return SDL_LOG_CATEGORY_GPU;
  case Subsystem::audio:
    return SDL_LOG_CATEGORY_AUDIO;
  case Subsystem::platform:
  case Subsystem::network:
    return SDL_LOG_CATEGORY_SYSTEM;
  case Subsystem::application:
  case Subsystem::authority:
  case Subsystem::protocol:
  case Subsystem::replay:
  case Subsystem::ui:
  case Subsystem::asset:
    return SDL_LOG_CATEGORY_APPLICATION;
  }
  return SDL_LOG_CATEGORY_APPLICATION;
}

std::size_t append(std::array<char, 768>& buffer, const std::size_t offset, const char* format, const char* first,
    const char* second) noexcept {
  if (offset >= buffer.size())
    return offset;
  const int written = std::snprintf(buffer.data() + offset, buffer.size() - offset, format, first, second);
  if (written <= 0)
    return offset;
  const std::size_t count = static_cast<std::size_t>(written);
  return std::min(buffer.size() - 1U, offset + count);
}

human_resources::input::DeviceId with_kind(
    const human_resources::input::DeviceId base, const std::uint64_t instance) noexcept {
  return base | (instance & 0xffffffffULL);
}

std::string joined_path(const std::string_view first, const std::string_view second) {
  std::string result(first);
  if (!result.empty() && result.back() != '/' && result.back() != '\\')
    result.push_back('/');
  result.append(second);
  return result;
}

std::string_view scope_directory(const human_resources::storage::Scope scope) noexcept {
  using human_resources::storage::Scope;
  switch (scope) {
  case Scope::saves:
    return "saves";
  case Scope::settings:
    return "settings";
  case Scope::cache:
    return "cache";
  case Scope::temporary:
    return "temporary";
  }
  return {};
}

} // namespace

human_resources::input::DeviceId gamepad_device_id(const SDL_JoystickID instance_id) noexcept {
  return with_kind(gamepad_device_base, static_cast<std::uint64_t>(instance_id));
}

std::optional<human_resources::ui::Point> pointer_position(const SDL_Event& event,
    const WindowMetrics& metrics) noexcept {
  if (!metrics.valid)
    return std::nullopt;
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
    return human_resources::ui::Point{event.button.x / metrics.window_units_per_point, event.button.y / metrics.window_units_per_point};
  }
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    return human_resources::ui::Point{event.motion.x / metrics.window_units_per_point, event.motion.y / metrics.window_units_per_point};
  }
  if (event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_UP ||
      event.type == SDL_EVENT_FINGER_MOTION) {
    return human_resources::ui::Point{
        event.tfinger.x * metrics.viewport.width,
        event.tfinger.y * metrics.viewport.height,
    };
  }
  return std::nullopt;
}

std::optional<human_resources::input::DeviceEvent> translate_input_event(
    const SDL_Event& event, const WindowMetrics& metrics) noexcept {
  using human_resources::input::ActionPhase;
  using human_resources::input::DeviceEvent;
  using human_resources::input::DeviceKind;
  if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
    if (event.key.repeat)
      return std::nullopt;
    return DeviceEvent{
        .device = with_kind(keyboard_device_base, event.key.which),
        .kind = DeviceKind::keyboard,
        .control = static_cast<std::int32_t>(event.key.scancode),
        .value = event.key.down ? 1.0F : 0.0F,
        .timestamp_ns = event.common.timestamp,
    };
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
    if (event.button.which == SDL_TOUCH_MOUSEID)
      return std::nullopt;
    return DeviceEvent{
        .device = with_kind(pointer_device_base, event.button.which),
        .kind = DeviceKind::pointer,
        .control = static_cast<std::int32_t>(event.button.button),
        .value = event.button.down ? 1.0F : 0.0F,
        .position = {.x = event.button.x / metrics.window_units_per_point,
            .y = event.button.y / metrics.window_units_per_point},
        .has_position = metrics.valid,
        .timestamp_ns = event.common.timestamp,
    };
  }
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    if (event.motion.which == SDL_TOUCH_MOUSEID)
      return std::nullopt;
    return DeviceEvent{
        .device = with_kind(pointer_device_base, event.motion.which),
        .kind = DeviceKind::pointer,
        .control = static_cast<std::int32_t>(SDL_BUTTON_LEFT),
        .value = (event.motion.state & SDL_BUTTON_LMASK) != 0U ? 1.0F : 0.0F,
        .position = {.x = event.motion.x / metrics.window_units_per_point,
            .y = event.motion.y / metrics.window_units_per_point},
        .has_position = metrics.valid,
        .phase = ActionPhase::changed,
        .timestamp_ns = event.common.timestamp,
    };
  }
  if (event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_UP ||
      event.type == SDL_EVENT_FINGER_MOTION || event.type == SDL_EVENT_FINGER_CANCELED) {
    if (event.tfinger.touchID == SDL_MOUSE_TOUCHID)
      return std::nullopt;
    return DeviceEvent{
        .device = with_kind(touch_device_base, event.tfinger.touchID),
        .kind = DeviceKind::touch,
        .control = 1,
        .value = event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_MOTION ? 1.0F : 0.0F,
        .position = {
            .x = event.tfinger.x * metrics.viewport.width,
            .y = event.tfinger.y * metrics.viewport.height,
        },
        .has_position = metrics.valid,
        .contact = event.tfinger.fingerID,
        .phase = event.type == SDL_EVENT_FINGER_DOWN ? ActionPhase::started : event.type == SDL_EVENT_FINGER_MOTION ? ActionPhase::changed
                                                                          : event.type == SDL_EVENT_FINGER_CANCELED ? ActionPhase::cancelled
                                                                                                                    : ActionPhase::completed,
        .timestamp_ns = event.common.timestamp,
    };
  }
  if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
    return DeviceEvent{
        .device = gamepad_device_id(event.gbutton.which),
        .kind = DeviceKind::gamepad,
        .control = static_cast<std::int32_t>(event.gbutton.button),
        .value = event.gbutton.down ? 1.0F : 0.0F,
        .timestamp_ns = event.common.timestamp,
    };
  }
  if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
    constexpr float positive_axis_maximum = static_cast<float>(std::numeric_limits<Sint16>::max());
    constexpr float negative_axis_maximum = -static_cast<float>(std::numeric_limits<Sint16>::min());
    const float value = event.gaxis.value >= 0 ? static_cast<float>(event.gaxis.value) / positive_axis_maximum
                                               : static_cast<float>(event.gaxis.value) / negative_axis_maximum;
    return DeviceEvent{
        .device = gamepad_device_id(event.gaxis.which),
        .kind = DeviceKind::gamepad,
        .control = gamepad_axis_control_base + static_cast<std::int32_t>(event.gaxis.axis),
        .value = value,
        .timestamp_ns = event.common.timestamp,
    };
  }
  return std::nullopt;
}

std::optional<human_resources::lifecycle::Event> translate_lifecycle_event(const SDL_Event& event) noexcept {
  using human_resources::lifecycle::Event;
  using human_resources::lifecycle::EventKind;
  using human_resources::lifecycle::MemoryPressure;
  switch (event.type) {
  case SDL_EVENT_WILL_ENTER_BACKGROUND:
  case SDL_EVENT_DID_ENTER_BACKGROUND:
    return Event{.kind = EventKind::entered_background};
  case SDL_EVENT_TERMINATING:
    return Event{.kind = EventKind::suspending};
  case SDL_EVENT_WILL_ENTER_FOREGROUND:
  case SDL_EVENT_DID_ENTER_FOREGROUND:
    return Event{.kind = EventKind::entered_foreground};
  case SDL_EVENT_LOW_MEMORY:
    return Event{.kind = EventKind::memory_changed, .memory = MemoryPressure::critical};
  case SDL_EVENT_WINDOW_FOCUS_GAINED:
    return Event{.kind = EventKind::focus_gained};
  case SDL_EVENT_WINDOW_FOCUS_LOST:
    return Event{.kind = EventKind::focus_lost};
  default:
    return std::nullopt;
  }
}

SdlInputDevices::~SdlInputDevices() {
  for (std::size_t index = 0U; index < gamepad_count_; ++index)
    SDL_CloseGamepad(gamepads_[index]);
}

void SdlInputDevices::discover() noexcept {
  int count = 0;
  SDL_JoystickID* instance_ids = SDL_GetGamepads(&count);
  if (instance_ids == nullptr)
    return;
  for (int index = 0; index < count; ++index)
    open(instance_ids[index]);
  SDL_free(instance_ids);
}

void SdlInputDevices::handle_device_event(const SDL_Event& event) noexcept {
  if (event.type == SDL_EVENT_GAMEPAD_ADDED)
    open(event.gdevice.which);
  else if (event.type == SDL_EVENT_GAMEPAD_REMOVED)
    close(event.gdevice.which);
}

void SdlInputDevices::open(const SDL_JoystickID instance_id) noexcept {
  const auto end = gamepads_.begin() + static_cast<std::ptrdiff_t>(gamepad_count_);
  const auto present = std::find_if(gamepads_.begin(), end, [instance_id](SDL_Gamepad* gamepad) {
    return SDL_GetGamepadID(gamepad) == instance_id;
  });
  if (present != end || gamepad_count_ == gamepads_.size())
    return;
  if (SDL_Gamepad* gamepad = SDL_OpenGamepad(instance_id); gamepad != nullptr)
    gamepads_[gamepad_count_++] = gamepad;
}

void SdlInputDevices::close(const SDL_JoystickID instance_id) noexcept {
  const auto end = gamepads_.begin() + static_cast<std::ptrdiff_t>(gamepad_count_);
  const auto found = std::find_if(gamepads_.begin(), end, [instance_id](SDL_Gamepad* gamepad) {
    return SDL_GetGamepadID(gamepad) == instance_id;
  });
  if (found == end)
    return;
  SDL_CloseGamepad(*found);
  std::move(found + 1, end, found);
  gamepads_[--gamepad_count_] = nullptr;
}

SdlStorageBackend::SdlStorageBackend(
    const std::string_view organization, const std::string_view application) {
  const std::string organization_value(organization);
  const std::string application_value(application);
  char* path = SDL_GetPrefPath(organization_value.c_str(), application_value.c_str());
  if (path != nullptr) {
    preference_root_ = path;
    SDL_free(path);
  }
}

SdlStorageBackend::SdlStorageBackend(std::string preference_root)
    : preference_root_(std::move(preference_root)) {}

human_resources::storage::ReadResult SdlStorageBackend::read(
    const human_resources::storage::Scope scope, const human_resources::storage::RelativePath& path,
    const std::size_t maximum_bytes) {
  using human_resources::storage::ReadIssue;
  if (!available() || !human_resources::storage::validate_relative_path(path.value).valid())
    return {.issue = ReadIssue::unavailable};
  const std::string filename = joined_path(joined_path(preference_root_, scope_directory(scope)), path.value);
  // Classify the open failure itself. A metadata/probe failure is never evidence
  // that the user has no data. Windows needs UTF-16 for non-ASCII account paths.
#ifdef _WIN32
  const auto native_path = std::filesystem::path(std::u8string(filename.begin(), filename.end()));
  std::FILE* opened = _wfopen(native_path.c_str(), L"rb");
#else
  std::FILE* opened = std::fopen(filename.c_str(), "rb");
#endif
  if (opened == nullptr)
    return {.issue = errno == ENOENT ? ReadIssue::not_found : ReadIssue::io_failure};
  const std::unique_ptr<std::FILE, decltype(&std::fclose)> file(opened, &std::fclose);
  std::vector<std::byte> bytes;
  std::array<std::byte, 4096U> chunk{};
  while (true) {
    // Read one byte past the budget to detect growth without allocating it.
    const std::size_t remaining = maximum_bytes - bytes.size();
    const std::size_t requested = remaining < chunk.size() ? remaining + 1U : chunk.size();
    const std::size_t count = std::fread(chunk.data(), 1U, requested, file.get());
    if (std::ferror(file.get()) != 0)
      return {.issue = ReadIssue::io_failure};
    if (count > remaining)
      return {.issue = ReadIssue::too_large};
    bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(count));
    if (std::feof(file.get()) != 0)
      return {.bytes = std::move(bytes)};
    if (count == 0U)
      return {.issue = ReadIssue::io_failure};
  }
}

human_resources::storage::WriteResult SdlStorageBackend::write_atomically(
    const human_resources::storage::Scope scope, const human_resources::storage::RelativePath& path,
    const std::span<const std::byte> bytes) {
  using human_resources::storage::WriteIssue;
  if (!available() || !human_resources::storage::validate_relative_path(path.value).valid())
    return {.issue = WriteIssue::unavailable};
  const std::string directory = joined_path(preference_root_, scope_directory(scope));
  const std::string destination = joined_path(directory, path.value);
  const std::size_t separator = destination.find_last_of("/\\");
  if (separator == std::string::npos || !SDL_CreateDirectory(destination.substr(0U, separator).c_str()))
    return {.issue = WriteIssue::io_failure};
  const std::string temporary =
      destination + ".ngf-tmp-" + std::to_string(next_temporary_file.fetch_add(1U));
  if (!SDL_SaveFile(temporary.c_str(), bytes.data(), bytes.size())) {
    (void)SDL_RemovePath(temporary.c_str());
    return {.issue = WriteIssue::io_failure};
  }
  if (!SDL_RenamePath(temporary.c_str(), destination.c_str())) {
    (void)SDL_RemovePath(temporary.c_str());
    return {.issue = WriteIssue::io_failure};
  }
  return {};
}

SdlAssetSource::SdlAssetSource() {
  const char* base = SDL_GetBasePath();
  if (base != nullptr)
    root_ = joined_path(base, "assets");
}

SdlAssetSource::SdlAssetSource(std::string root) : root_(std::move(root)) {}

human_resources::assets::LoadResult SdlAssetSource::load(
    const human_resources::assets::Record& record) {
  using human_resources::assets::LoadIssue;
  if (!available())
    return {.issue = LoadIssue::unavailable};
  if (!human_resources::assets::validate_catalog(std::span(&record, 1U)).valid())
    return {.issue = LoadIssue::io_failure};
  const std::string path = joined_path(root_, record.package_path);
  SDL_IOStream* opened = SDL_IOFromFile(path.c_str(), "rb");
  if (opened == nullptr) {
    // SDL also reads packaged Android assets. Keep that path, but only report
    // absence when the filesystem probe confirms it; denied/error is not missing.
    std::error_code error;
    const std::u8string utf8(path.begin(), path.end());
    const auto status = std::filesystem::status(std::filesystem::path(utf8), error);
    const bool absent = status.type() == std::filesystem::file_type::not_found &&
                        (!error || error == std::errc::no_such_file_or_directory);
    return {.issue = absent ? LoadIssue::not_found : LoadIssue::io_failure};
  }
  const std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(opened, &SDL_CloseIO);
  const auto length = SDL_GetIOSize(stream.get());
  if (length < 0)
    return {.issue = LoadIssue::io_failure};
  if (static_cast<std::uint64_t>(length) != record.size || record.size > std::numeric_limits<std::size_t>::max())
    return {.issue = LoadIssue::size_mismatch};
  // The compiled asset record bounds allocation, including a file changed after
  // opening. Check for truncation, growth and read errors before accepting bytes.
  const auto size = static_cast<std::size_t>(record.size);
  std::vector<std::byte> bytes(size);
  if (size > 0U && SDL_ReadIO(stream.get(), bytes.data(), size) != size)
    return {.issue = SDL_GetIOStatus(stream.get()) == SDL_IO_STATUS_EOF ? LoadIssue::size_mismatch : LoadIssue::io_failure};
  std::byte extra{};
  if (SDL_ReadIO(stream.get(), &extra, 1U) != 0U)
    return {.issue = LoadIssue::size_mismatch};
  if (SDL_GetIOStatus(stream.get()) != SDL_IO_STATUS_EOF)
    return {.issue = LoadIssue::io_failure};
  if (human_resources::assets::content_digest(bytes) != record.digest)
    return {.issue = LoadIssue::digest_mismatch};
  return {.bytes = std::move(bytes)};
}

void SdlLogSink::write(const human_resources::diagnostics::Record& record) noexcept {
  std::array<char, 768> message{};
  const auto subsystem = human_resources::diagnostics::subsystem_name(record.subsystem);
  int written = std::snprintf(message.data(), message.size(), "[%.*s:%u] %s", static_cast<int>(subsystem.size()),
      subsystem.data(), record.code, record.message.data());
  std::size_t used = written > 0 ? std::min(message.size() - 1U, static_cast<std::size_t>(written)) : 0U;
  for (std::size_t index = 0; index < record.field_count; ++index) {
    used = append(message, used, " %s=%s", record.fields[index].key.data(), record.fields[index].value.data());
  }
  if (record.truncated) {
    used = append(message, used, "%s%s", " ", "[truncated]");
  }
  (void)used;
  SDL_LogMessage(category(record.subsystem), priority(record.severity), "%s", message.data());
}

} // namespace human_resources::platform
