#include <human_resources/platform/sdl_platform.h>

#include "test_support.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using test_support::require;

class TemporaryDirectory final {
public:
  TemporaryDirectory() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    // Reserve a unique directory atomically; parallel verification must never
    // remove another process's live storage fixture.
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
      auto candidate = std::filesystem::temp_directory_path() /
                       ("human_resources_platform_adapter_tests_" + std::to_string(nonce) + "_" + std::to_string(attempt));
      std::error_code error;
      if (std::filesystem::create_directory(candidate, error)) {
        path = std::move(candidate);
        return;
      }
      require(!error, "temporary test directory is created");
    }
    require(false, "unique temporary test directory is reserved");
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }

  TemporaryDirectory(const TemporaryDirectory&) = delete;
  TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

  std::filesystem::path path;
};

std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  require(input.good(), "test file opens for reading");
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  const auto* begin = reinterpret_cast<const std::byte*>(bytes.data());
  return {begin, begin + bytes.size()};
}

void keyboard_and_pointer_events_translate_without_gameplay_knowledge() {
  SDL_Event key{};
  key.type = SDL_EVENT_KEY_DOWN;
  key.key.which = 12U;
  key.key.scancode = SDL_SCANCODE_RETURN;
  key.key.down = true;
  const auto translated = human_resources::platform::translate_input_event(key, {});
  require(translated.has_value() && translated->kind == human_resources::input::DeviceKind::keyboard,
      "keyboard event translates to a device event");
  const auto keyboard = translated.value_or(human_resources::input::DeviceEvent{});
  require(keyboard.control == static_cast<std::int32_t>(SDL_SCANCODE_RETURN) && keyboard.value == 1.0F,
      "keyboard identity and phase value are retained");

  SDL_Event touch{};
  touch.type = SDL_EVENT_FINGER_DOWN;
  touch.tfinger.touchID = 9U;
  touch.tfinger.x = 0.25F;
  touch.tfinger.y = 0.5F;
  const human_resources::platform::WindowMetrics metrics{
      .viewport = {.width = 800.0F, .height = 600.0F}, .valid = true};
  const auto pointer = human_resources::platform::translate_input_event(touch, metrics);
  require(pointer.has_value() && pointer->has_position && pointer->position.x == 200.0F &&
              pointer->position.y == 300.0F,
      "normalized touch coordinates translate into logical window coordinates");
}

void gamepad_axes_are_normalized_at_both_extremes() {
  SDL_Event axis{};
  axis.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
  axis.gaxis.which = 77;
  axis.gaxis.axis = static_cast<Uint8>(SDL_GAMEPAD_AXIS_LEFTX);
  axis.gaxis.value = std::numeric_limits<Sint16>::max();
  const auto positive = human_resources::platform::translate_input_event(axis, {});
  axis.gaxis.value = std::numeric_limits<Sint16>::min();
  const auto negative = human_resources::platform::translate_input_event(axis, {});
  require(positive.has_value() && std::abs(positive->value - 1.0F) < 0.0001F,
      "positive gamepad axis maximum normalizes to one");
  require(negative.has_value() && std::abs(negative->value + 1.0F) < 0.0001F,
      "negative gamepad axis maximum normalizes to negative one");
  using namespace human_resources::input;
  ActionMap actions({{91U, 2U, DeviceKind::gamepad, SDL_GAMEPAD_BUTTON_WEST}});
  axis.gaxis.axis = static_cast<Uint8>(SDL_GAMEPAD_AXIS_RIGHTX);
  axis.gaxis.value = std::numeric_limits<Sint16>::max();
  const auto right_axis = human_resources::platform::translate_input_event(axis, {});
  require(right_axis && !actions.translate(*right_axis, 2U),
      "a stick axis cannot trigger a face-button interaction with the same SDL number");
  SDL_Event button{};
  button.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
  button.gbutton.which = 77;
  button.gbutton.button = static_cast<Uint8>(SDL_GAMEPAD_BUTTON_WEST);
  button.gbutton.down = true;
  const auto press = human_resources::platform::translate_input_event(button, {});
  require(press && actions.translate(*press, 2U).has_value(), "the controller face button maps to interaction");
  require(positive && press && positive->device == human_resources::platform::gamepad_device_id(77) &&
              press->device == human_resources::platform::gamepad_device_id(77) &&
              press->device != human_resources::platform::gamepad_device_id(78),
      "unplug identity matches both translated axes and buttons without cancelling other controllers");
}

void touch_drag_cancel_and_synthetic_mouse_are_distinct() {
  using namespace human_resources::input;
  using human_resources::platform::translate_input_event;
  const human_resources::platform::WindowMetrics metrics{
      .viewport = {.width = 390.0F, .height = 844.0F}, .valid = true};
  SDL_Event event{};
  event.type = SDL_EVENT_FINGER_MOTION;
  event.tfinger.touchID = 9;
  event.tfinger.fingerID = 0x100000002LL;
  event.tfinger.x = 0.5F;
  event.tfinger.y = 0.5F;
  event.tfinger.timestamp = 7'123'456'789U;
  const auto drag = translate_input_event(event, metrics);
  require(drag && drag->phase == ActionPhase::changed && drag->contact == 0x100000002LL &&
              drag->position.x == 195.0F && drag->position.y == 422.0F,
      "portrait drag keeps full finger identity and point coordinates");
  require(drag && drag->timestamp_ns == event.tfinger.timestamp, "native touch time remains in nanoseconds");
  event.type = SDL_EVENT_FINGER_CANCELED;
  const auto cancel = translate_input_event(event, metrics);
  require(cancel && cancel->phase == ActionPhase::cancelled && cancel->value == 0.0F,
      "touch cancellation becomes an explicit zero-valued cancel");
  event.type = SDL_EVENT_FINGER_DOWN;
  const auto down = translate_input_event(event, metrics);
  event.type = SDL_EVENT_FINGER_UP;
  const auto up = translate_input_event(event, metrics);
  require(down && down->phase == ActionPhase::started && up && up->phase == ActionPhase::completed &&
              up->contact == down->contact,
      "touch press and release retain their contact and explicit phases");
  event.tfinger.touchID = SDL_MOUSE_TOUCHID;
  require(!translate_input_event(event, metrics), "mouse-generated touch does not duplicate pointer input");
  event = {};
  event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  event.button.which = SDL_TOUCH_MOUSEID;
  require(!translate_input_event(event, metrics), "touch-generated mouse press is filtered");
  event = {};
  event.type = SDL_EVENT_MOUSE_MOTION;
  event.motion.which = SDL_TOUCH_MOUSEID;
  require(!translate_input_event(event, metrics), "touch-generated mouse does not duplicate joystick input");
  event.motion.which = 1;
  event.motion.state = SDL_BUTTON_LMASK;
  event.motion.x = 80.0F;
  event.motion.timestamp = 8'123'456'789U;
  const auto mouse = translate_input_event(event, metrics);
  require(mouse && mouse->phase == ActionPhase::changed && mouse->value == 1.0F && mouse->position.x == 80.0F,
      "real mouse drag can exercise the same joystick on desktop");
  require(mouse && mouse->timestamp_ns == event.motion.timestamp, "desktop pointer preview preserves native event time");
}

void temporary_directories_never_remove_a_live_fixture() {
  TemporaryDirectory first;
  std::ofstream(first.path / "sentinel") << "keep";
  {
    TemporaryDirectory second;
    require(first.path != second.path && std::filesystem::exists(first.path / "sentinel"),
        "a new fixture never removes an existing fixture");
  }
  require(std::filesystem::exists(first.path / "sentinel"), "fixture cleanup affects only its own directory");
}

void lifecycle_events_remain_policy_free() {
  SDL_Event event{};
  event.type = SDL_EVENT_LOW_MEMORY;
  const auto translated = human_resources::platform::translate_lifecycle_event(event);
  require(translated.has_value() && translated->kind == human_resources::lifecycle::EventKind::memory_changed &&
              translated->memory == human_resources::lifecycle::MemoryPressure::critical,
      "SDL low-memory event translates into the portable lifecycle contract");
}

void storage_writes_replace_atomically_and_clean_up_failures() {
  using namespace human_resources::storage;
  TemporaryDirectory temporary;
  human_resources::platform::SdlStorageBackend storage(temporary.path.string());
  require(storage.available(), "explicit storage root is available");
  const RelativePath path{"profiles/player/save.bin"};
  const std::array first{std::byte{1}, std::byte{2}};
  const std::array second{std::byte{3}, std::byte{4}, std::byte{5}};
  require(storage.write_atomically(Scope::saves, path, first).succeeded(), "initial storage write succeeds");
  require(storage.write_atomically(Scope::saves, path, second).succeeded(), "existing save is replaced");
  require(read_bytes(temporary.path / "saves/profiles/player/save.bin") ==
              std::vector<std::byte>(second.begin(), second.end()),
      "replacement exposes only the complete new payload");

  const auto blocked = temporary.path / "saves/blocked.bin";
  std::error_code error;
  std::filesystem::create_directories(blocked, error);
  require(!error, "replacement-failure fixture is created");
  require(storage.write_atomically(Scope::saves, RelativePath{"blocked.bin"}, first).issue ==
              WriteIssue::io_failure,
      "failed replacement is reported");
  for (const auto& entry : std::filesystem::directory_iterator(temporary.path / "saves")) {
    require(entry.path().filename().string().find(".ngf-tmp-") == std::string::npos,
        "failed replacement removes its sibling temporary file");
  }
  require(storage.write_atomically(Scope::saves, RelativePath{"../escape.bin"}, first).issue ==
              WriteIssue::unavailable,
      "unsafe storage path fails before I/O");
}

void packaged_asset_source_checks_size_and_digest() {
  using namespace human_resources::assets;
  TemporaryDirectory temporary;
  const auto asset_path = temporary.path / "images/panel.bin";
  std::error_code error;
  std::filesystem::create_directories(asset_path.parent_path(), error);
  require(!error, "asset fixture directory is created");
  const std::array bytes{std::byte{7}, std::byte{8}, std::byte{9}};
  std::ofstream output(asset_path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  output.close();
  require(output.good(), "asset fixture is written");

  human_resources::platform::SdlAssetSource source(temporary.path.string());
  const Record valid{.id = {"ui/panel"},
      .package_path = "images/panel.bin",
      .size = bytes.size(),
      .digest = content_digest(bytes)};
  require(source.load(valid).succeeded(), "packaged asset with exact size and digest loads");
  Record wrong_size = valid;
  ++wrong_size.size;
  require(source.load(wrong_size).issue == LoadIssue::size_mismatch, "asset size mismatch fails closed");
  Record wrong_digest = valid;
  ++wrong_digest.digest;
  require(source.load(wrong_digest).issue == LoadIssue::digest_mismatch,
      "asset digest mismatch fails closed");
  Record missing = valid;
  missing.package_path = "images/missing.bin";
  require(source.load(missing).issue == LoadIssue::not_found, "missing packaged asset is explicit");
  auto unsafe = valid;
  unsafe.package_path = "../panel.bin";
  require(source.load(unsafe).issue == LoadIssue::io_failure, "packaged asset reads stay within the validated package path");
  // Even matching bytes outside the package must not be reachable by traversal.
  std::filesystem::create_directory(temporary.path / "package");
  human_resources::platform::SdlAssetSource nested((temporary.path / "package").string());
  unsafe.package_path = "../images/panel.bin";
  require(nested.load(unsafe).issue == LoadIssue::io_failure, "matching parent-file bytes cannot bypass package validation");
  auto invalid_parent = valid;
  invalid_parent.package_path = "images/panel.bin/child.bin";
  require(source.load(invalid_parent).issue == LoadIssue::io_failure, "invalid parent is an I/O failure, not absence");
  auto empty = valid;
  empty.package_path = "images/empty.bin";
  empty.size = 0U;
  empty.digest = content_digest({});
  std::ofstream(temporary.path / empty.package_path, std::ios::binary).close();
  require(source.load(empty).succeeded(), "empty asset is distinct from absence");
  std::filesystem::resize_file(asset_path, std::uintmax_t{16U} * 1024U * 1024U);
  require(source.load(valid).issue == LoadIssue::size_mismatch, "oversized asset is rejected against its compiled byte budget");
#ifndef _WIN32
  std::filesystem::permissions(asset_path, std::filesystem::perms::none);
  const auto denied = source.load(valid);
  std::filesystem::permissions(asset_path, std::filesystem::perms::owner_all);
  require(denied.issue == LoadIssue::io_failure, "permission denial is never reported as a missing asset");
#endif
}

void storage_reads_are_bounded_and_failures_are_not_absence() {
  using namespace human_resources::storage;
  TemporaryDirectory temporary;
  human_resources::platform::SdlStorageBackend backend(temporary.path.string());
  const RelativePath path{"document.bin"};
  require(backend.read(Scope::saves, path, 4U).issue == ReadIssue::not_found, "missing file is explicit");
  const std::array payload{std::byte{1}, std::byte{2}, std::byte{3}};
  require(backend.write_atomically(Scope::saves, path, payload).succeeded(), "read fixture written");
  require(backend.read(Scope::saves, path, 3U).bytes == std::vector<std::byte>(payload.begin(), payload.end()),
      "exact read budget succeeds");
  require(backend.read(Scope::saves, path, 2U).issue == ReadIssue::too_large, "larger file is rejected");
  require(backend.read(Scope::saves, path, 0U).issue == ReadIssue::too_large, "zero budget rejects content");
  require(backend.write_atomically(Scope::saves, {"empty.bin"}, {}).succeeded(), "empty fixture written");
  require(backend.read(Scope::saves, {"empty.bin"}, 0U).succeeded(), "empty file is distinct from missing");
  require(backend.read(Scope::saves, {"document.bin/child.bin"}, 9U).issue == ReadIssue::io_failure,
      "invalid parent metadata is an error, not a new document");
  require(backend.read(Scope::saves, {"../escape.bin"}, 9U).issue == ReadIssue::unavailable,
      "invalid path is rejected before read");
#ifndef _WIN32
  // Exercise the real permission boundary; unlike a fake unavailable backend,
  // this catches accidentally treating every failed open/stat as not-found.
  const auto file = temporary.path / "saves/document.bin";
  std::filesystem::permissions(file, std::filesystem::perms::none);
  const auto denied = backend.read(Scope::saves, path, 9U);
  std::filesystem::permissions(file, std::filesystem::perms::owner_all);
  require(denied.issue == ReadIssue::io_failure, "permission denial must protect saved data");
#endif
}

void display_pointer_and_native_frames_share_one_scale() {
  using namespace human_resources::platform;
  for (const float scale : {1.0F, 2.0F, 3.0F, 3.5F}) {
    const auto metrics = normalize_window_metrics({400.0F * scale, 800.0F * scale},
        {0.0F, 24.0F * scale, 0.0F, 16.0F * scale}, 1.0F, scale);
    require(metrics.valid && metrics.viewport.width == 400.0F && metrics.safe_area.top == 24.0F,
        "pixel-based windows normalize to points");
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.x = 40.0F * scale;
    event.button.y = 80.0F * scale;
    const auto pointer = pointer_position(event, metrics);
    const auto input = translate_input_event(event, metrics);
    require(pointer && input && pointer->x == 40.0F && input->position.x == pointer->x,
        "mouse hit testing and semantic input agree in points");
    const auto frame = window_coordinates({40.0F, 80.0F, 48.0F, 48.0F}, metrics);
    require(frame.x == event.button.x && frame.width == 48.0F * scale,
        "native accessibility frames convert back to window coordinates");
    const auto apple = normalize_window_metrics({400.0F, 800.0F}, {}, scale, scale);
    require(apple.valid && apple.viewport.width == 400.0F && apple.window_units_per_point == 1.0F,
        "point-based high-density windows retain their point dimensions");
  }
  require(!normalize_window_metrics({400.0F, 800.0F}, {}, 0.0F, 2.0F).valid, "invalid density rejected");
  require(!normalize_window_metrics({0.0F, 800.0F}, {}, 1.0F, 1.0F).valid, "zero drawable suspended");
  require(!normalize_window_metrics({400.0F, 800.0F}, {500.0F, 0.0F, 0.0F, 0.0F}, 1.0F, 1.0F).valid,
      "impossible safe area rejected");
}

} // namespace

int main() {
  temporary_directories_never_remove_a_live_fixture();
  keyboard_and_pointer_events_translate_without_gameplay_knowledge();
  gamepad_axes_are_normalized_at_both_extremes();
  touch_drag_cancel_and_synthetic_mouse_are_distinct();
  lifecycle_events_remain_policy_free();
  storage_writes_replace_atomically_and_clean_up_failures();
  packaged_asset_source_checks_size_and_digest();
  storage_reads_are_bounded_and_failures_are_not_absence();
  display_pointer_and_native_frames_share_one_scale();
  return 0;
}
