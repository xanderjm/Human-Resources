#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <human_resources/audio/audio_service.h>
#include <human_resources/diagnostics/diagnostics.h>
#include <human_resources/input/input.h>
#include <human_resources/lifecycle/lifecycle.h>
#include <human_resources/platform/native_accessibility.h>
#include <human_resources/platform/sdl_platform.h>
#include <human_resources/renderer/gpu_renderer.h>
#include <human_resources/ui/layout.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr human_resources::ui::NodeId screen_id = 1U;
constexpr human_resources::ui::NodeId panel_id = 2U;
constexpr human_resources::ui::NodeId status_id = 3U;
constexpr human_resources::ui::NodeId action_id = 4U;
constexpr human_resources::input::ActionId activate_action = 1U;
constexpr human_resources::input::ContextId ui_context = 1U;

std::vector<human_resources::input::Binding> application_bindings() {
  using human_resources::input::Binding;
  using human_resources::input::DeviceKind;
  return {
      Binding{.action = activate_action, .context = ui_context, .device_kind = DeviceKind::keyboard, .control = static_cast<std::int32_t>(SDL_SCANCODE_RETURN)},
      Binding{.action = activate_action, .context = ui_context, .device_kind = DeviceKind::keyboard, .control = static_cast<std::int32_t>(SDL_SCANCODE_SPACE)},
      Binding{.action = activate_action, .context = ui_context, .device_kind = DeviceKind::pointer, .control = static_cast<std::int32_t>(SDL_BUTTON_LEFT)},
      Binding{.action = activate_action, .context = ui_context, .device_kind = DeviceKind::touch, .control = 1},
      Binding{.action = activate_action, .context = ui_context, .device_kind = DeviceKind::gamepad, .control = static_cast<std::int32_t>(SDL_GAMEPAD_BUTTON_SOUTH)},
  };
}

struct App final {
  App() : renderer(logger), actions(application_bindings()) {}

  human_resources::diagnostics::Logger logger;
  human_resources::platform::SdlLogSink log_sink;
  human_resources::renderer::GpuRenderer renderer;
  human_resources::platform::SdlInputDevices input_devices;
  human_resources::input::ActionMap actions;
  human_resources::lifecycle::State lifecycle;
  human_resources::audio::NullAudioService audio;
  human_resources::ui::DesignTokens tokens;
  human_resources::ui::LayoutContext ui_context;
  human_resources::ui::Node scene;
  std::vector<human_resources::ui::DrawCommand> draw_commands;
  human_resources::platform::WindowMetrics window_metrics;
  std::shared_ptr<human_resources::accessibility::Session> accessibility =
      std::make_shared<human_resources::accessibility::Session>();
  std::unique_ptr<human_resources::platform::NativeAccessibility> native_accessibility;

  SDL_Window* window{};
  std::uint64_t demo_count{};
  std::uint64_t presented_count{std::numeric_limits<std::uint64_t>::max()};
  human_resources::ui::NodeId pressed_node{human_resources::ui::invalid_node_id};
  std::uint32_t frames{};
  bool layout_dirty{true};
  bool smoke_test{};
  std::string smoke_report_path;
};

std::string counter_text(const std::uint64_t value) {
  std::array<char, 48> buffer{};
  constexpr std::string_view prefix = "Example count: ";
  std::copy(prefix.begin(), prefix.end(), buffer.begin());
  const auto result = std::to_chars(buffer.data() + prefix.size(), buffer.data() + buffer.size() - 1U, value);
  if (result.ec != std::errc{})
    return std::string(prefix) + "?";
  return std::string(buffer.data(), result.ptr);
}

human_resources::ui::Color mix(const human_resources::ui::Color first, const human_resources::ui::Color second,
    const float amount) noexcept {
  const float clamped = std::clamp(amount, 0.0F, 1.0F);
  return {
      .red = first.red + (second.red - first.red) * clamped,
      .green = first.green + (second.green - first.green) * clamped,
      .blue = first.blue + (second.blue - first.blue) * clamped,
      .alpha = first.alpha + (second.alpha - first.alpha) * clamped,
  };
}

void build_scene(App& app) {
  app.scene = human_resources::ui::components::screen(screen_id, app.tokens);
  human_resources::ui::Node panel =
      human_resources::ui::components::panel(panel_id, human_resources::ui::LayoutKind::column, app.tokens);
  panel.layout.width = human_resources::ui::Dimension::automatic();
  panel.layout.height = human_resources::ui::Dimension::weight();
  panel.layout.main_alignment = human_resources::ui::MainAlignment::center;
  panel.layout.cross_alignment = human_resources::ui::Alignment::stretch;
  panel.semantics.label = "Foundation proof";

  human_resources::ui::Node status =
      human_resources::ui::components::label(status_id, counter_text(app.demo_count), app.tokens);
  status.layout.height = human_resources::ui::Dimension::points(app.tokens.minimum_interaction_target.height);
  human_resources::ui::Node action = human_resources::ui::components::button(action_id, "Increment counter", app.tokens);
  action.layout.width = human_resources::ui::Dimension::automatic();
  const float ratio = static_cast<float>(app.demo_count % 100U) / 100.0F;
  action.visual.background = mix(app.tokens.action, app.tokens.action_pressed, ratio);
  action.semantics.value = counter_text(app.demo_count);
  panel.children.push_back(std::move(status));
  panel.children.push_back(std::move(action));
  app.scene.children.push_back(std::move(panel));
}

bool rebuild_layout(App& app) {
  app.window_metrics = human_resources::platform::query_window_metrics(app.window);
  if (!app.window_metrics.valid)
    return false;
  build_scene(app);
  const auto result =
      human_resources::ui::layout(app.scene, app.window_metrics.viewport, app.window_metrics.safe_area, app.ui_context);
  if (!result.valid) {
    const std::array fields{
        human_resources::diagnostics::Field::unsigned_value("issue_count", result.issues.size()),
    };
    (void)app.logger.try_record({.severity = human_resources::diagnostics::Severity::error,
        .subsystem = human_resources::diagnostics::Subsystem::ui,
        .code = 3001U,
        .message = "UI layout validation failed",
        .fields = fields});
    return false;
  }
  app.draw_commands = human_resources::ui::build_draw_list(app.scene);
  if (!app.accessibility->publish(human_resources::ui::accessibility_tree(app.scene)))
    return false;
  if (app.native_accessibility)
    app.native_accessibility->update(app.window, app.window_metrics);
  app.presented_count = app.demo_count;
  app.layout_dirty = false;
  return true;
}

void submit_increment(App& app) {
  ++app.demo_count;
  app.audio.play({.code = 1U, .bus = human_resources::audio::Bus::interface});
}

} // namespace

SDL_AppResult SDL_AppInit(void** appstate, const int argc, char** argv) {
  SDL_SetAppMetadata("Human Resources", "0.1.0", "com.xanderjm.humanresources");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    return SDL_APP_FAILURE;

  auto app = std::make_unique<App>();
  app->native_accessibility = human_resources::platform::create_native_accessibility(app->accessibility);
  if (!app->actions.validation().valid()) {
    SDL_Quit();
    return SDL_APP_FAILURE;
  }
  (void)app->logger.add_sink(app->log_sink);
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    app->smoke_test = app->smoke_test || argument == "--smoke-test";
    if (argument == "--smoke-report") {
      if (index + 1 >= argc) {
        SDL_Quit();
        return SDL_APP_FAILURE;
      }
      app->smoke_test = true;
      app->smoke_report_path = argv[++index];
    }
  }
  if (const char* report_path = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "FOUNDATION_SMOKE_REPORT");
      report_path != nullptr && report_path[0] != '\0') {
    app->smoke_test = true;
    app->smoke_report_path = report_path;
  }
  app->window =
      SDL_CreateWindow("Human Resources", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (app->window == nullptr) {
    SDL_Quit();
    return SDL_APP_FAILURE;
  }
  if (!app->renderer.initialize(app->window) || !rebuild_layout(*app)) {
    app->renderer.shutdown();
    (void)app->logger.drain();
    SDL_DestroyWindow(app->window);
    SDL_Quit();
    return SDL_APP_FAILURE;
  }
  if (app->smoke_test) {
    const auto backend = app->renderer.backend_name();
    const auto device = app->renderer.device_name();
    const auto driver = app->renderer.driver_version();
    if (!app->smoke_report_path.empty()) {
      std::ofstream report(app->smoke_report_path, std::ios::binary | std::ios::trunc);
      report << "foundation-smoke backend=" << backend << " device=" << device << " driver=" << driver << '\n';
      report.flush();
      if (!report.good()) {
        app->renderer.shutdown();
        (void)app->logger.drain();
        SDL_DestroyWindow(app->window);
        SDL_Quit();
        return SDL_APP_FAILURE;
      }
    }
  }
  app->input_devices.discover();
  (void)app->logger.try_record({.severity = human_resources::diagnostics::Severity::info,
      .subsystem = human_resources::diagnostics::Subsystem::application,
      .code = 1U,
      .message = "client foundation initialized"});
  *appstate = app.release();
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  auto& app = *static_cast<App*>(appstate);
  app.input_devices.handle_device_event(*event);
  if (event->type == SDL_EVENT_QUIT)
    return SDL_APP_SUCCESS;
  if (const auto lifecycle_event = human_resources::platform::translate_lifecycle_event(*event);
      lifecycle_event.has_value()) {
    const auto transition = human_resources::lifecycle::apply(app.lifecycle, *lifecycle_event);
    if (transition.changed && app.lifecycle.presence == human_resources::lifecycle::Presence::background)
      app.audio.handle_interruption(human_resources::audio::Interruption::began);
    else if (transition.changed && app.lifecycle.presence == human_resources::lifecycle::Presence::foreground)
      app.audio.handle_interruption(human_resources::audio::Interruption::ended_should_resume);
  }
  if (event->type == SDL_EVENT_WINDOW_RESIZED || event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
      event->type == SDL_EVENT_WINDOW_SAFE_AREA_CHANGED || event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
    app.layout_dirty = true;
    app.pressed_node = human_resources::ui::invalid_node_id;
    return SDL_APP_CONTINUE;
  }
  if (event->type == SDL_EVENT_FINGER_CANCELED || event->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
    app.pressed_node = human_resources::ui::invalid_node_id;
    return SDL_APP_CONTINUE;
  }
  // Input queued after resize must use the new frames and density immediately.
  if (app.layout_dirty) {
    if (!human_resources::platform::query_window_metrics(app.window).valid)
      return SDL_APP_CONTINUE;
    if (!rebuild_layout(app))
      return SDL_APP_FAILURE;
  }
  const auto source = human_resources::platform::translate_input_event(*event, app.window_metrics);
  if (!source.has_value())
    return SDL_APP_CONTINUE;
  const auto action = app.actions.translate(*source, ui_context);
  if (!action.has_value() || action->id != activate_action)
    return SDL_APP_CONTINUE;
  if (!action->has_position && action->phase == human_resources::input::ActionPhase::started) {
    submit_increment(app);
    return SDL_APP_CONTINUE;
  }
  if (action->has_position && action->phase == human_resources::input::ActionPhase::started) {
    const human_resources::ui::Point point{action->position.x, action->position.y};
    app.pressed_node =
        human_resources::ui::hit_test(app.scene, point).value_or(human_resources::ui::invalid_node_id);
  } else if (action->has_position && action->phase == human_resources::input::ActionPhase::completed) {
    const human_resources::ui::Point point{action->position.x, action->position.y};
    const auto released = human_resources::ui::hit_test(app.scene, point);
    if (app.pressed_node == action_id && released == action_id)
      submit_increment(app);
    app.pressed_node = human_resources::ui::invalid_node_id;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto& app = *static_cast<App*>(appstate);
  if (app.lifecycle.presence != human_resources::lifecycle::Presence::foreground) {
    SDL_Delay(16U);
    return SDL_APP_CONTINUE;
  }
  const auto preferences = app.native_accessibility->preferences();
  if (preferences.text_scale != app.ui_context.text_scale ||
      preferences.reduced_motion != app.ui_context.reduced_motion) {
    app.ui_context.text_scale = preferences.text_scale;
    app.ui_context.reduced_motion = preferences.reduced_motion;
    app.layout_dirty = true;
  }
  for (const auto action : app.accessibility->drain_actions()) {
    if (action.node_id == action_id && action.kind == human_resources::accessibility::ActionKind::activate)
      submit_increment(app);
  }
  if (app.layout_dirty && !human_resources::platform::query_window_metrics(app.window).valid) {
    SDL_Delay(16U);
    return SDL_APP_CONTINUE;
  }
  if ((app.layout_dirty || app.presented_count != app.demo_count) && !rebuild_layout(app)) {
    (void)app.logger.drain();
    return SDL_APP_FAILURE;
  }
  if (!app.renderer.render({app.window_metrics.viewport.width, app.window_metrics.viewport.height},
          app.draw_commands)) {
    (void)app.logger.drain();
    return SDL_APP_FAILURE;
  }
  (void)app.logger.drain();
  ++app.frames;
  if (app.smoke_test && app.frames >= 3U)
    return SDL_APP_SUCCESS;
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  (void)result;
  std::unique_ptr<App> app(static_cast<App*>(appstate));
  if (app) {
    (void)app->logger.try_record({.severity = human_resources::diagnostics::Severity::info,
        .subsystem = human_resources::diagnostics::Subsystem::application,
        .code = 2U,
        .message = "client foundation shutting down"});
    (void)app->logger.drain();
    app->accessibility->close();
    app->native_accessibility.reset();
    app->renderer.shutdown();
    SDL_DestroyWindow(app->window);
  }
  app.reset();
  SDL_Quit();
}
