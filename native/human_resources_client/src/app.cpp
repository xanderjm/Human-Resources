#define SDL_MAIN_USE_CALLBACKS 1
#include "resume_qa.h"
#include "interruption_policy.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <human_resources/lifecycle/lifecycle.h>
#include <human_resources/platform/native_accessibility.h>
#include <human_resources/platform/product_audio.h>
#include <human_resources/platform/product_haptics.h>
#include <human_resources/platform/sdl_platform.h>
#include <human_resources/product/accessibility.h>
#include <human_resources/product/controls.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/progression/account_session.h>
#include <human_resources/renderer/style_renderer.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
namespace {
namespace hr = human_resources;
using hr::product::Command;
using hr::product::ControlAction;
using hr::product::Screen;
constexpr hr::input::ContextId context = 1;
std::vector<hr::input::Binding> bindings() {
  using hr::input::DeviceKind;
  std::vector<hr::input::Binding> result;
  const auto add = [&](ControlAction id, DeviceKind kind, int control, float dead = 0) { result.push_back({static_cast<hr::input::ActionId>(id), context, kind, control, 1, dead}); };
  for (auto key : {SDL_SCANCODE_A, SDL_SCANCODE_LEFT})
    add(ControlAction::left, DeviceKind::keyboard, key);
  for (auto key : {SDL_SCANCODE_D, SDL_SCANCODE_RIGHT})
    add(ControlAction::right, DeviceKind::keyboard, key);
  for (auto key : {SDL_SCANCODE_W, SDL_SCANCODE_UP})
    add(ControlAction::up, DeviceKind::keyboard, key);
  for (auto key : {SDL_SCANCODE_S, SDL_SCANCODE_DOWN})
    add(ControlAction::down, DeviceKind::keyboard, key);
  add(ControlAction::accept, DeviceKind::keyboard, SDL_SCANCODE_RETURN);
  add(ControlAction::pause, DeviceKind::keyboard, SDL_SCANCODE_ESCAPE);
  add(ControlAction::pause, DeviceKind::keyboard, SDL_SCANCODE_P);
  add(ControlAction::pause, DeviceKind::keyboard, SDL_SCANCODE_AC_BACK);
  add(ControlAction::dash, DeviceKind::keyboard, SDL_SCANCODE_SPACE);
  add(ControlAction::pick0, DeviceKind::keyboard, SDL_SCANCODE_1);
  add(ControlAction::pick1, DeviceKind::keyboard, SDL_SCANCODE_2);
  add(ControlAction::pick2, DeviceKind::keyboard, SDL_SCANCODE_3);
  add(ControlAction::contact, DeviceKind::pointer, SDL_BUTTON_LEFT);
  add(ControlAction::contact, DeviceKind::touch, 1);
  add(ControlAction::axis_x, DeviceKind::gamepad, hr::platform::gamepad_axis_control_base + SDL_GAMEPAD_AXIS_LEFTX, .15F);
  add(ControlAction::axis_y, DeviceKind::gamepad, hr::platform::gamepad_axis_control_base + SDL_GAMEPAD_AXIS_LEFTY, .15F);
  add(ControlAction::accept, DeviceKind::gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
  add(ControlAction::pause, DeviceKind::gamepad, SDL_GAMEPAD_BUTTON_START);
  add(ControlAction::dash, DeviceKind::gamepad, SDL_GAMEPAD_BUTTON_EAST);
  return result;
}
std::string isolated_test_root(std::string_view name) {
  char* base = SDL_GetPrefPath("HumanResources", "HumanResources-QA");
  if (!base)
    return {};
  const auto path = (std::filesystem::path(base) / (std::string(name) + "-" + std::to_string(SDL_GetTicksNS()))).string();
  SDL_free(base);
  return path;
}
struct App final {
  SDL_Window* window{};
  std::shared_ptr<hr::accessibility::Session> accessibility = std::make_shared<hr::accessibility::Session>();
  std::unique_ptr<hr::platform::NativeAccessibility> native_accessibility;
  std::unique_ptr<hr::renderer::StyleRenderer> renderer;
  hr::platform::SdlStorageBackend backend{"HumanResources", "HumanResources"};
  hr::progression::AccountSession store{backend};
  hr::gameplay::Game game;
  hr::product::SceneEffects scene_effects;
  hr::product::Controls controls;
  hr::product::PromotionFlow promotion;
  hr::client::ResumeQa resume_qa;
  hr::input::ActionMap actions{bindings()};
  hr::platform::SdlInputDevices devices;
  hr::platform::ProductAudio audio;
  hr::platform::ProductHaptics haptics;
  bool audio_focused{true};
  hr::platform::WindowMetrics metrics;
  hr::product::Interface ui;
  hr::lifecycle::State lifecycle;
  Screen screen{Screen::title}, settings_return{Screen::title}, library_return{Screen::hangar};
  hr::presentation::SceneryTheme selected_theme{hr::presentation::SceneryTheme::farmland};
  Command pressed{Command::none};
  std::uint64_t pressed_contact{}, pressed_device{}, run_id{}, previous_ticks{};
  unsigned frames{}, smoke_mask{};
  unsigned qa_frames{};
  unsigned qa_capture_skip{};
  float qa_step{1.F / 60};
  std::uint64_t qa_seed{1};
  bool qa{};
  unsigned qa_menu_frames{}; // QA film readability only; ordinary menu clocks still freeze.
  unsigned qa_vehicle_checkpoints{};
  std::ofstream qa_metrics;
  std::string qa_save_dir, qa_metrics_path, normal_save_dir;
  std::string continuation_mode, continuation_slot;
  std::uint64_t previous_profile_ticks{};
  int focus{};
  float focus_x{}, focus_y{};
  bool keyboard_focus{};
  bool show_recipes{};
  unsigned recipe_page{}, ui_page{}, upgrade_preview{};
  bool show_build{}, show_privacy{};
  hr::accessibility::Preferences frame_preferences;
  std::string ui_context;
  bool smoke{}, playthrough{}, save_error{}, guest{}, pending_settlement{};
  bool restored_run{}, bank_after_checkpoint{}, restore_failed{};
  hr::progression::RunReceipt pending_receipt;
  std::string report, capture_path;
  SDL_IOStream* capture{};
  hr::renderer::CaptureExtent capture_pixels;
  unsigned render_w{1280}, render_h{720};
  unsigned window_w{390}, window_h{844};
};
bool retain_qa_vehicle_checkpoint(App& app) {
  if(!app.qa || app.qa_metrics_path.empty()) return true;
  const auto& run=app.game.snapshot();
  std::array<std::uint32_t,4> sources{};
  for(const auto& shot:run.projectiles) {
    const int index=shot.kind==hr::gameplay::EnemyProjectileKind::jeep_minigun ? 0 :
        shot.kind==hr::gameplay::EnemyProjectileKind::vehicle_missile ? 1 :
        shot.kind==hr::gameplay::EnemyProjectileKind::boss_missile ? 2 : -1;
    if(index>=0) sources[static_cast<std::size_t>(index)]=shot.source_id;
  }
  for(const auto& actor:run.entities)
    if((actor.kind==hr::gameplay::EntityKind::jeep || actor.kind==hr::gameplay::EntityKind::tank) && actor.capture_progress>0)
      sources[3]=actor.id;
  constexpr std::array<std::string_view,4> names{"first-jeep-fire","first-tank-fire","first-boss-fire","first-vehicle-capture"};
  for(std::size_t index=0;index<sources.size();++index) {
    if(!sources[index] || (app.qa_vehicle_checkpoints&(1U<<index))) continue;
    // Only explicitly requested QA metrics retain up to four full Game states.
    // This does not checkpoint the account or alter the observed native run.
    const auto directory=std::filesystem::path(app.qa_metrics_path+".game-checkpoints");
    std::error_code error;std::filesystem::create_directories(directory,error);
    if(error) return false;
    const auto bytes=app.game.save_state();
    if(bytes.empty()) return false;
    const auto path=directory/(std::string(names[index])+".hrs");
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    output.close();if(!output) return false;
    std::ofstream metadata(directory/(std::string(names[index])+".json"));
    metadata<<"{\"frame\":"<<app.frames<<",\"run_id\":"<<app.run_id<<",\"source_id\":"<<sources[index]
        <<",\"active_seconds\":"<<run.stats.active_seconds<<",\"bytes\":"<<bytes.size()<<"}\n";
    metadata.close();if(!metadata) return false;
    app.qa_vehicle_checkpoints|=1U<<index;
    SDL_Log("product-qa-vehicle-checkpoint kind=%s frame=%u source=%u bytes=%zu digest=%016llx",names[index].data(),app.frames,sources[index],bytes.size(),
        static_cast<unsigned long long>(hr::storage::content_digest(bytes)));
  }
  return true;
}
void haptics_policy(App& app) {
  const bool active_screen = app.screen == Screen::run || app.screen == Screen::upgrade || app.screen == Screen::death;
  app.haptics.set_policy(app.store.account().settings.haptics, app.lifecycle.presence == hr::lifecycle::Presence::foreground && app.audio_focused && active_screen && !app.save_error);
}
hr::progression::RunReceipt receipt(const App& app) {
  const auto& s = app.game.snapshot().stats;
  return {app.run_id, static_cast<std::uint64_t>(s.active_seconds * 1000), s.score, s.captures, s.soldier_kills, s.jeep_kills, s.tank_kills, s.vehicle_captures, s.boss_kills};
}
void change_screen(App& app, Screen screen) {
  if (app.screen != screen) {
    app.controls.clear();
    app.pressed = Command::none;
    app.screen = screen;
    if (screen == Screen::upgrade) app.promotion.enter();
    else app.promotion.clear();
    app.ui_page = 0;
    app.upgrade_preview = 0;
    app.show_build = false;
    app.show_privacy = false;
    app.ui_context.clear();
    haptics_policy(app);
    app.focus = 0;
    app.keyboard_focus = false;
    app.previous_ticks = SDL_GetTicksNS();
    SDL_Log("product-screen=%u", static_cast<unsigned>(screen));
  }
  app.smoke_mask |= 1U << static_cast<unsigned>(screen);
}
void reset_run_feedback(App& app) {
  app.promotion.clear();
  app.controls.clear();
  app.pressed = Command::none;
  app.pressed_contact = app.pressed_device = 0;
  app.focus_x = app.focus_y = 0;
  app.previous_ticks = SDL_GetTicksNS();
  app.scene_effects.reset();
  if (app.renderer)
    app.renderer->reset_actor_animations();
  app.audio.set_paused(true);
  app.audio.set_paused(!app.audio_focused || app.save_error);
  app.haptics.set_policy(app.store.account().settings.haptics, false);
}
void log_run_detail(const App& app) {
  const auto& account = app.store.account();
  const auto& saved = account.continuation;
  SDL_Log("product-account-detail credits=%llu checkpoint_run=%llu results_run=%llu", static_cast<unsigned long long>(account.credits), static_cast<unsigned long long>(account.checkpoint.run_id), static_cast<unsigned long long>(account.results.receipt.run_id));
  for (const auto& line : hr::client::resume_log_lines(app.game.snapshot(), app.run_id, static_cast<std::uint8_t>(app.selected_theme), hr::storage::content_digest(saved.game_state)))
    SDL_Log("%s", line.c_str());
}
enum class RestorePurpose { fresh_begin,
  recovery_prompt };
bool publish_saved_game(App& app, RestorePurpose purpose) {
  const auto& saved = app.store.account().continuation;
  if (saved.game_state.empty() || app.game.restore_state(saved.game_state) != hr::gameplay::SaveStateIssue::none) {
    app.restore_failed = app.save_error = true;
    app.controls.clear();
    SDL_Log("Compatible account continuation could not be restored; original save remains protected");
    return false;
  }
  app.run_id = app.store.account().checkpoint.run_id;
  app.selected_theme = static_cast<hr::presentation::SceneryTheme>(saved.location);
  app.restored_run = purpose == RestorePurpose::recovery_prompt;
  if (app.restored_run)
    app.game.pause(); // An earned choice remains choosing_upgrade, with its exact cards.
  reset_run_feedback(app);
  change_screen(app, app.restored_run ? Screen::pause : Screen::run);
  const auto& state = app.game.snapshot();
  SDL_Log("product-run-%s run=%llu phase=%u level=%u active_ms=%llu location=%u state_digest=%016llx", app.restored_run ? "restored" : "started", static_cast<unsigned long long>(app.run_id), static_cast<unsigned>(state.phase), state.stats.level, static_cast<unsigned long long>(state.stats.active_seconds * 1000), static_cast<unsigned>(saved.location), static_cast<unsigned long long>(hr::storage::content_digest(saved.game_state)));
  if (app.resume_qa.active())
    log_run_detail(app); // Common restore is observed before any explicit QA action.
  return true;
}
void start(App& app) {
  hr::gameplay::RunSetup setup;
  setup.seed = app.qa ? app.qa_seed : SDL_GetTicksNS();
  setup.permanent_ranks = app.store.account().ranks;
  setup.unlocked_blueprints = app.store.account().unlocked_blueprints;
  app.game.start(setup);
  app.restored_run = app.restore_failed = app.bank_after_checkpoint = false;
  const auto initial = hr::progression::continuation_for(app.game, static_cast<std::uint8_t>(app.selected_theme));
  const auto result = app.store.begin_run(initial);
  app.save_error = !result.succeeded();
  if (result.succeeded())
    (void)publish_saved_game(app, RestorePurpose::fresh_begin);
  else {
    app.game.pause();
    reset_run_feedback(app);
  }
}
bool checkpoint_game(App& app, bool boundary = false) {
  if (!app.run_id || app.pending_settlement)
    return true;
  const auto current = receipt(app);
  if (!app.store.checkpoint_due(current, boundary))
    return true;
  const auto continuation = hr::progression::continuation_for(app.game, static_cast<std::uint8_t>(app.selected_theme));
  const auto result = app.store.checkpoint(current, continuation, boundary);
  app.save_error = !result.succeeded();
  if (!app.save_error) {
    SDL_Log("product-run-checkpoint run=%llu phase=%u active_ms=%llu state_digest=%016llx", static_cast<unsigned long long>(app.run_id), static_cast<unsigned>(app.game.snapshot().phase), static_cast<unsigned long long>(current.active_milliseconds), static_cast<unsigned long long>(hr::storage::content_digest(continuation.game_state)));
    return true;
  }
  SDL_Log("product-run-checkpoint-protected issue=%u write=%u", static_cast<unsigned>(result.issue), static_cast<unsigned>(result.write_issue));
  app.game.pause();
  app.controls.clear();
  app.audio.set_paused(true);
  if (app.game.snapshot().phase == hr::gameplay::Phase::paused)
    change_screen(app, Screen::pause);
  haptics_policy(app);
  return false;
}
void commit_settlement(App& app) {
  const auto result = app.store.settle(app.pending_receipt);
  app.save_error = !result.succeeded();
  app.pending_settlement = app.save_error;
  if (result.succeeded())
    app.run_id = 0;
  app.restored_run = app.bank_after_checkpoint = false;
  change_screen(app, Screen::results);
}
void retry_save(App& app) {
  const auto pending = app.store.pending_operation();
  const auto result = app.store.retry_save();
  app.save_error = !result.succeeded();
  haptics_policy(app);
  if (!result.succeeded())
    return;
  app.pending_settlement = false;
  app.audio.set_enabled(app.store.account().settings.audio);
  app.audio.set_paused(!app.audio_focused || app.save_error);
  if (pending == hr::progression::SessionOperation::recover_checkpoint || (!app.run_id && pending == hr::progression::SessionOperation::none && app.store.load_result().issue == hr::progression::LoadIssue::recovered_checkpoint)) {
    app.run_id = 0;
    change_screen(app, Screen::results);
    return;
  }
  if (pending == hr::progression::SessionOperation::begin)
    (void)publish_saved_game(app, RestorePurpose::fresh_begin);
  else if (pending == hr::progression::SessionOperation::settle) {
    app.run_id = 0;
    change_screen(app, Screen::results);
  } else if (pending == hr::progression::SessionOperation::checkpoint && app.bank_after_checkpoint) {
    commit_settlement(app);
  } else if (pending == hr::progression::SessionOperation::none && !app.run_id && app.store.account().checkpoint.run_id && !app.store.account().continuation.game_state.empty()) {
    (void)publish_saved_game(app, RestorePurpose::recovery_prompt);
  }
}
void finish(App& app) {
  app.game.pause();
  app.pending_receipt = receipt(app);
  if (!checkpoint_game(app, true)) {
    app.bank_after_checkpoint = true;
    return;
  }
  commit_settlement(app);
}
void dispatch(App& app, Command command) {
  haptics_policy(app);
  if (command >= Command::pick0 && command <= Command::pick2 && app.screen != Screen::upgrade) return;
  const bool page_navigation = command == Command::page_next || command == Command::page_previous;
  if (app.save_error && !page_navigation && command != Command::retry_save && command != Command::back_up_reset && command != Command::back_up_bank)
    return;
  if (app.store.pending() && !page_navigation && command != Command::retry_save)
    return;
  app.audio.play(hr::feedback::Cue::ui);
  switch (command) {
  case Command::preview0:
  case Command::preview1:
  case Command::preview2:
    if (app.screen == Screen::upgrade) {
      app.upgrade_preview = static_cast<unsigned>(command) - static_cast<unsigned>(Command::preview0);
      app.ui_page = 0;
      app.controls.clear();
      app.pressed = Command::none;
      app.focus = 0;
      app.keyboard_focus = false;
      app.focus_x = app.focus_y = 0;
    }
    break;
  case Command::page_next:
  case Command::page_previous:
    for (const auto& button : app.ui.buttons)
      if (button.command == command && button.enabled) {
        app.ui_page = command == Command::page_next ? app.ui.page + 1 : app.ui.page - 1;
        app.controls.clear();
        app.pressed = Command::none;
        app.focus = 0;
        app.keyboard_focus = false;
        app.focus_x = app.focus_y = 0;
        break;
      }
    break;
  case Command::privacy:
  case Command::current_build:
    if ((command == Command::privacy && app.screen == Screen::settings) || (command == Command::current_build && app.screen == Screen::pause)) {
      if (command == Command::privacy) app.show_privacy = !app.show_privacy;
      else app.show_build = !app.show_build;
      app.ui_page = 0;
      app.ui_context.clear();
      app.controls.clear();
      app.pressed = Command::none;
      app.focus = 0;
      app.keyboard_focus = false;
    }
    break;
  case Command::location_next:
    if (app.screen == Screen::title)
      app.selected_theme = hr::product::next_location(app.selected_theme);
    break;
  case Command::start:
  case Command::retry:
    start(app);
    break;
  case Command::pause:
    app.game.pause();
    change_screen(app, Screen::pause);
    (void)checkpoint_game(app, true);
    break;
  case Command::resume:
    if (!app.run_id || !app.store.account().checkpoint.run_id || app.save_error || app.game.snapshot().phase == hr::gameplay::Phase::dead)
      break;
    app.restored_run = false;
    app.controls.clear();
    app.previous_ticks = SDL_GetTicksNS();
    if (app.game.snapshot().phase == hr::gameplay::Phase::choosing_upgrade)
      change_screen(app, Screen::upgrade);
    else {
      app.game.resume();
      change_screen(app, Screen::run);
    }
    break;
  case Command::finish:
    finish(app);
    break;
  case Command::blueprints:
    app.library_return = app.screen;
    app.show_recipes = false;
    app.recipe_page = 0;
    change_screen(app, Screen::blueprints);
    break;
  case Command::hangar:
    change_screen(app, Screen::hangar);
    break;
  case Command::recipes:
    app.show_recipes = !app.show_recipes;
    app.recipe_page = 0;
    app.ui_page = 0;
    break;
  case Command::recipe_next:
    app.recipe_page = (app.recipe_page + 1) % 3;
    break;
  case Command::recipe_previous:
    app.recipe_page = (app.recipe_page + 2) % 3;
    break;
  case Command::recipe0:
  case Command::recipe1:
  case Command::recipe2:
  case Command::recipe3:
  case Command::recipe4:
  case Command::recipe5:
    app.library_return = Screen::upgrade;
    app.show_recipes = true;
    app.recipe_page = (static_cast<unsigned>(command) - static_cast<unsigned>(Command::recipe0)) / 2;
    change_screen(app, Screen::blueprints);
    break;
  case Command::settings:
    app.settings_return = app.screen;
    change_screen(app, Screen::settings);
    break;
  case Command::home:
    if (app.screen == Screen::settings && app.show_privacy) {
      dispatch(app, Command::privacy);
      break;
    }
    change_screen(app, app.screen == Screen::blueprints ? app.library_return : app.screen == Screen::settings ? app.settings_return
                                                                                                              : Screen::title);
    break;
  case Command::pick0:
  case Command::pick1:
  case Command::pick2: {
    const auto index = static_cast<std::size_t>(command) - static_cast<std::size_t>(Command::pick0);
    const auto selected = app.game.snapshot().choices[index];
    if (app.game.choose_upgrade(index)) {
      if (!checkpoint_game(app, true))
        break;
      app.audio.play(hr::feedback::Cue::upgrade);
      change_screen(app, Screen::run);
      app.promotion.confirm(selected);
      app.haptics.play(hr::feedback::HapticCue::upgrade, SDL_GetTicks());
    }
    break;
  }
  case Command::reroll:
    if (app.game.reroll())
      (void)checkpoint_game(app, true);
    break;
  case Command::buy0:
  case Command::buy1:
  case Command::buy2:
  case Command::buy3: {
    const auto result = app.store.purchase(static_cast<hr::progression::Track>(static_cast<unsigned>(command) - static_cast<unsigned>(Command::buy0)));
    if (result.succeeded())
      app.audio.play(hr::feedback::Cue::purchase);
    if (result.issue == hr::progression::MutationIssue::storage_failure || result.issue == hr::progression::MutationIssue::protected_save)
      app.save_error = true;
    break;
  }
  case Command::audio: {
    auto settings = app.store.account().settings;
    settings.audio = !settings.audio;
    app.save_error = !app.store.update_settings(settings).succeeded();
    app.audio.set_enabled(app.store.account().settings.audio);
    break;
  }
  case Command::haptics: {
    auto settings = app.store.account().settings;
    settings.haptics = !settings.haptics;
    app.save_error = !app.store.update_settings(settings).succeeded();
    haptics_policy(app);
    break;
  }
  case Command::motion: {
    auto settings = app.store.account().settings;
    settings.reduced_motion = !settings.reduced_motion;
    app.save_error = !app.store.update_settings(settings).succeeded();
    break;
  }
  case Command::back_up_reset: {
    const auto backup = hr::storage::validate_relative_path("account-backup-" + std::to_string(SDL_GetTicksNS()) + ".bin");
    if (backup.valid()) {
      const auto result = app.store.preserve_and_reset(backup.path);
      app.save_error = !result.succeeded();
      if (result.succeeded()) {
        app.guest = false;
        change_screen(app, Screen::title);
      }
    }
    break;
  }
  case Command::back_up_bank: {
    const auto backup = hr::storage::validate_relative_path("run-backup-" + std::to_string(SDL_GetTicksNS()) + ".bin");
    if (backup.valid()) {
      const auto result = app.store.preserve_and_bank_checkpoint(backup.path);
      app.save_error = !result.succeeded();
      if (result.succeeded()) {
        app.run_id = 0;
        app.restored_run = false;
        reset_run_feedback(app);
        change_screen(app, Screen::results);
      }
    }
    break;
  }
  case Command::retry_save:
    retry_save(app);
    break;
  case Command::dash: {
    hr::input::Action action;
    action.id = static_cast<hr::input::ActionId>(ControlAction::dash);
    action.phase = hr::input::ActionPhase::started;
    action.value = 1;
    app.controls.apply(action, true, false);
    break;
  }
  default:
    break;
  }
}
void rebuild(App& app) {
  app.metrics = hr::platform::query_window_metrics(app.window);
  const auto issue = app.store.load_result().issue;
  const bool can_reset = !app.store.pending() && !app.run_id && (issue == hr::progression::LoadIssue::corrupt || issue == hr::progression::LoadIssue::unsupported_schema);
  const bool can_bank = !app.store.pending() && issue == hr::progression::LoadIssue::incompatible_continuation;
  const std::string_view save_message = app.store.pending() ? "The save could not be written. Your exact progress is retained until Retry Save succeeds." : issue == hr::progression::LoadIssue::corrupt                 ? "This save is damaged. Retry reading it, or preserve a verified backup before resetting."
                                                                                                                                                        : issue == hr::progression::LoadIssue::unsupported_schema        ? "This save was made by a newer version. Retry, or back it up before starting fresh."
                                                                                                                                                        : issue == hr::progression::LoadIssue::incompatible_continuation ? "This run cannot resume on this build. Preserve a verified backup and bank its saved credits, or retry with a compatible build."
                                                                                                                                                        : app.restore_failed                                             ? "The saved run could not be restored. Retry with a compatible build; the original save remains protected."
                                                                                                                                                                                                                         : "The save cannot be read. Retry before playing so existing progress is preserved.";
  app.frame_preferences = app.native_accessibility ? app.native_accessibility->preferences() : hr::accessibility::Preferences{};
  const float text_scale = std::isfinite(app.frame_preferences.text_scale) && app.frame_preferences.text_scale > 0 ? app.frame_preferences.text_scale : 1.F;
  std::string ui_context = std::to_string(static_cast<unsigned>(app.screen)) + "/" + std::to_string(text_scale) + "/" + std::to_string(app.metrics.viewport.width) + "/" + std::to_string(app.metrics.viewport.height) + "/" + std::to_string(app.metrics.safe_area.left) + "/" + std::to_string(app.metrics.safe_area.top) + "/" + std::to_string(app.metrics.safe_area.right) + "/" + std::to_string(app.metrics.safe_area.bottom) + "/" + std::to_string(app.save_error) + "/" + std::to_string(app.store.pending()) + "/" + std::to_string(static_cast<unsigned>(issue)) + "/" + std::to_string(app.show_build) + "/" + std::to_string(app.show_privacy) + "/" + std::to_string(app.show_recipes) + "/" + std::to_string(app.recipe_page);
  if (app.save_error) ui_context += "/" + std::string(save_message);
  if (app.screen == Screen::upgrade) {
    ui_context += "/" + std::to_string(app.game.snapshot().stats.level) + "/" + std::to_string(app.game.snapshot().rerolls_remaining);
    for (const auto& choice : app.game.snapshot().choices)
      ui_context += "/" + choice.title + "/" + choice.description + "/" + choice.evolution_preview;
  }
  if (ui_context != app.ui_context) {
    app.ui_context = std::move(ui_context);
    app.ui_page = 0;
    app.upgrade_preview = 0;
    app.controls.clear();
    app.pressed = Command::none;
    app.focus = 0;
    app.focus_x = app.focus_y = 0;
    app.keyboard_focus = false;
  }
  hr::product::View view{app.screen, &app.game.snapshot(), &app.store.store(), app.metrics.viewport, app.metrics.safe_area, app.save_error, app.guest, app.game.revive_available(), app.store.store().credit_bonus_available(), app.controls.origin(), app.controls.position(), app.controls.dragging(), save_message, can_reset, app.pending_settlement ? &app.pending_receipt : nullptr, app.show_recipes, app.recipe_page, app.selected_theme, app.haptics.available(), app.restored_run, can_bank, text_scale, app.ui_page, app.show_build, app.upgrade_preview, app.show_privacy};
  view.promotion = app.promotion.frame(app.store.account().settings.reduced_motion || app.frame_preferences.reduced_motion);
  view.damage_feedback = app.scene_effects.damage_feedback();
  app.ui = hr::product::interface(view);
  app.ui_page = app.ui.page;
  Command focused = Command::none;
  if (app.keyboard_focus && !app.ui.buttons.empty())
    focused = app.ui.buttons[static_cast<std::size_t>(std::clamp(app.focus, 0, static_cast<int>(app.ui.buttons.size()) - 1))].command;
  (void)app.accessibility->publish(hr::product::accessibility_snapshot(view, app.ui, focused));
  if (app.native_accessibility)
    app.native_accessibility->update(app.window, app.metrics);
  if (app.keyboard_focus && !app.ui.buttons.empty()) {
    app.focus = std::clamp(app.focus, 0, static_cast<int>(app.ui.buttons.size()) - 1);
    const auto r = app.ui.buttons[static_cast<std::size_t>(app.focus)].bounds;
    const float w = app.metrics.viewport.width, h = app.metrics.viewport.height;
    for (const auto border : std::array<hr::ui::Rect, 4>{{{r.x - 3, r.y - 3, r.width + 6, 2}, {r.x - 3, r.bottom() + 1, r.width + 6, 2}, {r.x - 3, r.y - 3, 2, r.height + 6}, {r.right() + 1, r.y - 3, 2, r.height + 6}}})
      app.ui.quads.push_back({{border.x / w, border.y / h, border.width / w, border.height / h}, {border.width, border.height, -.5F, 0}, {1, .8F, .3F, 1}});
  }
}
Command hit(const App& app, hr::ui::Point p) {
  for (auto i = app.ui.buttons.rbegin(); i != app.ui.buttons.rend(); ++i)
    if (i->enabled && i->bounds.contains(p))
      return i->command;
  return Command::none;
}
void semantic(App& app, const hr::input::Action& a) {
  const auto id = static_cast<ControlAction>(a.id);
  const bool begin = a.phase == hr::input::ActionPhase::started;
  const auto button = a.has_position ? hit(app, {a.position.x, a.position.y}) : Command::none;
  app.controls.apply(a, app.screen == Screen::run, button != Command::none);
  if (app.screen != Screen::run) {
    int direction = 0;
    if (begin && (id == ControlAction::left || id == ControlAction::up))
      direction = -1;
    if (begin && (id == ControlAction::right || id == ControlAction::down))
      direction = 1;
    if (id == ControlAction::axis_x || id == ControlAction::axis_y) {
      float& previous = id == ControlAction::axis_x ? app.focus_x : app.focus_y;
      const float current = std::abs(a.value) > .6F ? std::copysign(1.F, a.value) : 0;
      if (current != previous && current != 0)
        direction = current > 0 ? 1 : -1;
      previous = current;
    }
    if (direction && !app.ui.buttons.empty()) {
      const auto count = static_cast<int>(app.ui.buttons.size());
      app.keyboard_focus = true;
      for (int step = 0; step < count; ++step) {
        app.focus = (app.focus + direction + count) % count;
        if (app.ui.buttons[static_cast<std::size_t>(app.focus)].enabled)
          break;
      }
    }
    if (begin && id == ControlAction::accept && app.keyboard_focus && !app.ui.buttons.empty()) {
      const auto& selected = app.ui.buttons[static_cast<std::size_t>(app.focus)];
      if (selected.enabled)
        dispatch(app, selected.command);
      return;
    }
  }

  if (id == ControlAction::contact) {
    if (begin && button != Command::none) {
      app.keyboard_focus = false;
      app.pressed = button;
      app.pressed_contact = a.contact;
      app.pressed_device = a.device;
    }
    if (a.phase == hr::input::ActionPhase::cancelled) {
      if (app.pressed_contact == a.contact && app.pressed_device == a.device)
        app.pressed = Command::none;
    }
    if (a.phase == hr::input::ActionPhase::completed && app.pressed_contact == a.contact && app.pressed_device == a.device) {
      const auto pressed = app.pressed;
      app.pressed = Command::none;
      if (pressed != Command::none && pressed == button)
        dispatch(app, pressed);
    }
  } else if (begin) {
    if (id == ControlAction::pause) {
      if (app.screen == Screen::run || app.screen == Screen::upgrade)
        dispatch(app, Command::pause);
      else if (app.screen == Screen::pause)
        dispatch(app, app.show_build ? Command::current_build : Command::resume);
      else if (app.screen == Screen::settings || app.screen == Screen::hangar || app.screen == Screen::blueprints)
        dispatch(app, Command::home);
      else if (app.screen == Screen::death)
        dispatch(app, Command::finish);
      else if (app.screen == Screen::results)
        dispatch(app, Command::home);
    }
    if (id == ControlAction::accept) {
      if (app.ui.page_count > 1) {
        // Enter/controller accept activates the visible section, never a hidden
        // first offer on a later choice page. Reading-only pages advance.
        for (const auto& item : app.ui.buttons)
          if (item.enabled && item.command != Command::page_next && item.command != Command::page_previous && item.command != Command::home && !(item.command >= Command::preview0 && item.command <= Command::preview2) && !(app.show_build && item.command == Command::current_build) && !(app.show_privacy && item.command == Command::privacy)) {
            dispatch(app, item.command);
            return;
          }
        dispatch(app, Command::page_next);
        return;
      }
      if (app.screen == Screen::title)
        dispatch(app, Command::start);
      else if (app.screen == Screen::pause)
        dispatch(app, app.show_build ? Command::current_build : Command::resume);
      else if (app.screen == Screen::results)
        dispatch(app, Command::retry);
      else if (app.screen == Screen::death)
        dispatch(app, Command::finish);
      else if (app.screen == Screen::upgrade)
        dispatch(app, Command::pick0);
    }
    if (app.screen == Screen::upgrade && id >= ControlAction::pick0 && id <= ControlAction::pick2)
      dispatch(app, static_cast<Command>(static_cast<unsigned>(Command::pick0) + static_cast<unsigned>(id) - static_cast<unsigned>(ControlAction::pick0)));
  }
}
void smoke_input(App& app) {
  const auto click = [&](Command command) {
    for (const auto& button : app.ui.buttons)
      if (button.command == command && button.enabled) {
        hr::input::Action a;
        a.id = static_cast<hr::input::ActionId>(ControlAction::contact);
        a.device = 789;
        a.contact = 123;
        a.source_kind = hr::input::DeviceKind::touch;
        a.has_position = true;
        a.position = {button.bounds.x + button.bounds.width * .5F, button.bounds.y + button.bounds.height * .5F};
        a.phase = hr::input::ActionPhase::started;
        a.value = 1;
        semantic(app, a);
        a.phase = hr::input::ActionPhase::completed;
        a.value = 0;
        semantic(app, a);
        return;
      }
  };

  if (app.frames == 1)
    click(Command::start);
  if (app.frames == 24)
    click(Command::pause);
  if (app.frames == 26)
    click(Command::settings);
  if (app.frames == 28)
    click(Command::home);
  if (app.frames == 30)
    click(Command::resume);
  if (app.screen == Screen::upgrade && app.frames % 5 == 0)
    click(Command::pick0);
  if (app.screen == Screen::death)
    click(Command::finish);
  if (app.frames >= 230 && app.frames <= 233) {
    if (app.screen == Screen::run)
      click(Command::pause);
    else if (app.screen == Screen::pause)
      click(Command::finish);
    else if (app.screen == Screen::upgrade)
      click(Command::pick0);
  }
  if (app.frames == 240)
    click(Command::hangar);
  if (app.frames == 246) {
    for (unsigned i = 0; i < 4; ++i)
      if (app.store.store().hangar()[i].affordable) {
        click(static_cast<Command>(static_cast<unsigned>(Command::buy0) + i));
        break;
      }
  }
  if (app.frames == 250)
    click(Command::home);
  if (app.frames == 255)
    click(Command::start);
  if (app.screen == Screen::run) {
    hr::input::Action a;
    a.id = static_cast<hr::input::ActionId>(ControlAction::axis_x);
    a.value = std::sin(static_cast<float>(app.frames) * .07F);
    a.phase = hr::input::ActionPhase::changed;
    semantic(app, a);
  }
}
} // namespace
SDL_AppResult SDL_AppInit(void** state, int argc, char** argv) {
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
  SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
  SDL_SetAppMetadata("Human Resources", "0.1.0", "com.xanderjm.humanresources");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    return SDL_APP_FAILURE;
  auto app = std::make_unique<App>();
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "--qa-continuation" || arg == "--qa-continuation-slot") {
      if (i + 1 >= argc)
        return SDL_APP_FAILURE;
      (arg == "--qa-continuation" ? app->continuation_mode : app->continuation_slot) = argv[++i];
    } else if (arg == "--save-dir") {
      if (i + 1 >= argc)
        return SDL_APP_FAILURE;
      app->normal_save_dir = argv[++i];
    } else if (arg == "--qa-theme") {
      if (i + 1 >= argc)
        return SDL_APP_FAILURE;
      const std::string_view value = argv[++i];
      if (value == "farmland")
        app->selected_theme = hr::presentation::SceneryTheme::farmland;
      else if (value == "suburbia")
        app->selected_theme = hr::presentation::SceneryTheme::suburbia;
      else if (value == "desert")
        app->selected_theme = hr::presentation::SceneryTheme::desert;
      else {
        SDL_Log("Invalid QA theme; expected farmland, suburbia or desert");
        return SDL_APP_FAILURE;
      }
    } else if (arg == "--qa-frames" || arg == "--qa-seed" || arg == "--qa-step-seconds" || arg == "--qa-metrics" || arg == "--qa-save-dir" || arg == "--qa-capture-skip-frames") {
      if (i + 1 >= argc)
        return SDL_APP_FAILURE;
      const std::string value = argv[++i];
      app->qa = true;
      if (arg == "--qa-save-dir")
        app->qa_save_dir = value;
      else if (arg == "--qa-metrics") {
        app->qa_metrics_path = value;
        app->qa_metrics.open(value);
        if (!app->qa_metrics)
          return SDL_APP_FAILURE;
        app->qa_metrics << "frame,phase,active_seconds,entities,projectiles,beams,frame_interval_ms,game_advance_ms,presentation_ms,render_submit_ms,cpu_frame_ms,world_draws,shadow_draws,triangles\n";
      } else if (arg == "--qa-step-seconds") {
        char* end{};
        app->qa_step = std::strtof(value.c_str(), &end);
        if (end != value.c_str() + value.size() || !std::isfinite(app->qa_step) || app->qa_step <= 0 || app->qa_step > .25F)
          return SDL_APP_FAILURE;
      } else {
        std::uint64_t number{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
          return SDL_APP_FAILURE;
        if (arg == "--qa-seed")
          app->qa_seed = number;
        else {
          if (number == 0 || number > 2000000)
            return SDL_APP_FAILURE;
          if (arg == "--qa-capture-skip-frames")
            app->qa_capture_skip = static_cast<unsigned>(number);
          else
            app->qa_frames = static_cast<unsigned>(number);
        }
      }
    } else if (arg == "--capture-pixels") {
      if (i + 1 >= argc)
        return SDL_APP_FAILURE;
      const auto extent = hr::renderer::parse_capture_extent(argv[++i]);
      if (!extent) {
        SDL_Log("Capture pixels require WIDTHxHEIGHT, each 1..4096, at most 8388608 pixels");
        return SDL_APP_FAILURE;
      }
      app->capture_pixels = *extent;
    } else if (arg == "--width" || arg == "--height") {
      if (i + 1 >= argc)
        return SDL_APP_FAILURE;
      unsigned value{};
      const std::string_view text = argv[++i];
      const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
      if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value < 320 || value > 2560)
        return SDL_APP_FAILURE;
      (arg == "--width" ? app->window_w : app->window_h) = value;
    } else if (arg == "--smoke-test")
      app->smoke = true;
    else if (arg == "--playthrough-smoke")
      app->playthrough = true;
    else if ((arg == "--smoke-report" || arg == "--capture") && i + 1 < argc) {
      if (arg == "--capture")
        app->capture_path = argv[++i];
      else {
        app->report = argv[++i];
        app->smoke = true;
      }
    } else {
      SDL_Log("Unknown or incomplete client option: %s", argv[i]);
      return SDL_APP_FAILURE;
    }
  }
  if (const char* path = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "FOUNDATION_SMOKE_REPORT"); path && *path) {
    app->report = path;
    app->smoke = true;
  }
  if (!app->capture_pixels.empty() && app->capture_path.empty()) {
    SDL_Log("Capture pixels require an explicit --capture path");
    return SDL_APP_FAILURE;
  }
  if (!app->continuation_mode.empty() || !app->continuation_slot.empty()) {
    if (!app->resume_qa.configure(app->continuation_mode, app->continuation_slot) || !app->normal_save_dir.empty() || app->qa || app->playthrough || app->smoke) {
      SDL_Log("Continuation QA requires a valid explicit mode/slot and cannot mix save roots or automation modes");
      return SDL_APP_FAILURE;
    }
  }
  const bool mobile = std::string_view(SDL_GetPlatform()) == "Android" || std::string_view(SDL_GetPlatform()) == "iOS";
  app->window = SDL_CreateWindow("Human Resources", static_cast<int>(app->window_w), static_cast<int>(app->window_h), SDL_WINDOW_HIGH_PIXEL_DENSITY | (mobile ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE));
  if (!app->window)
    return SDL_APP_FAILURE;
  if (!app->capture_pixels.empty()) {
    int width{}, height{};
    if (!SDL_GetWindowSize(app->window, &width, &height) || width <= 0 || height <= 0 ||
        !app->capture_pixels.matches_aspect(static_cast<unsigned>(width), static_cast<unsigned>(height))) {
      SDL_Log("Capture aspect must match the actual logical window; select matching --width/--height");
      return SDL_APP_FAILURE;
    }
  }
  (void)app->audio.initialize();
  app->native_accessibility = hr::platform::create_native_accessibility(app->accessibility);
  SDL_SetWindowMinimumSize(app->window, 320, 320);
  int initial_w{}, initial_h{};
  SDL_GetWindowSizeInPixels(app->window, &initial_w, &initial_h);
  const float initial_factor = std::min(1.F, 1280.F / static_cast<float>(std::max(initial_w, initial_h)));
  app->render_w = static_cast<unsigned>(static_cast<int>(static_cast<float>(initial_w) * initial_factor) / 4 * 4);
  app->render_h = static_cast<unsigned>(static_cast<int>(static_cast<float>(initial_h) * initial_factor) / 4 * 4);
  app->devices.discover();
  if (app->resume_qa.active()) {
    char* base = SDL_GetPrefPath("HumanResources", "HumanResources-QA");
    if (!base)
      return SDL_APP_FAILURE;
    const auto isolated = std::filesystem::path(base) / "continuation" / std::string(app->resume_qa.slot());
    SDL_free(base);
    app->backend = hr::platform::SdlStorageBackend(isolated.string());
    SDL_Log("product-continuation-qa slot=%s", std::string(app->resume_qa.slot()).c_str());
  } else if (!app->normal_save_dir.empty())
    app->backend = hr::platform::SdlStorageBackend(app->normal_save_dir);
  else if (app->qa) {
    if (app->qa_save_dir.empty())
      app->qa_save_dir = (app->qa_metrics_path.empty() ? std::filesystem::path(isolated_test_root("profile")) : std::filesystem::path(app->qa_metrics_path).parent_path() / "qa-save").string();
    app->backend = hr::platform::SdlStorageBackend(app->qa_save_dir);
  } else if (app->playthrough)
    app->backend = hr::platform::SdlStorageBackend(isolated_test_root("playthrough"));
  const auto loaded = app->store.load();
  if (!app->store.account().continuation.game_state.empty() && app->store.account().continuation.location <= 2)
    app->selected_theme = static_cast<hr::presentation::SceneryTheme>(app->store.account().continuation.location);
  (void)app->haptics.initialize();
  app->audio.set_enabled(app->store.account().settings.audio);
  app->save_error = !loaded.playable() || loaded.recovery_write_issue != hr::storage::WriteIssue::none;
  app->guest = false;
  if (loaded.issue == hr::progression::LoadIssue::recovered_checkpoint)
    app->screen = Screen::results;
  else if (loaded.issue == hr::progression::LoadIssue::resumable_run)
    (void)publish_saved_game(*app, RestorePurpose::recovery_prompt);
  if (app->resume_qa.active() && loaded.issue != hr::progression::LoadIssue::resumable_run)
    log_run_detail(*app);
  app->renderer = std::make_unique<hr::renderer::StyleRenderer>();
  const auto ids = hr::product::content_ids();
  constexpr std::array<std::string_view, 8> animated_ids{"cow", "pig", "sheep", "farmer-body", "resident-body", "infantry-rifleman-body", "infantry-shotgunner-body", "infantry-rocketeer-body"};
  const auto root = std::string_view(SDL_GetPlatform()) == "Android" ? std::filesystem::path("runtime/game") : std::filesystem::path(SDL_GetBasePath()) / "assets/runtime/game";
  if (!app->renderer->initialize(app->window, root.string(), app->render_w, app->render_h, ids, animated_ids)) {
    SDL_Log("Game resources failed: %s", SDL_GetError());
    app->renderer.reset();
    SDL_DestroyWindow(app->window);
    return SDL_APP_FAILURE;
  }
  if (!app->capture_path.empty()) {
    app->capture = SDL_IOFromFile(app->capture_path.c_str(), "wb");
    if (!app->capture)
      return SDL_APP_FAILURE;
  }
  rebuild(*app);
  app->previous_ticks = SDL_GetTicksNS();
  app->smoke_mask |= 1U << static_cast<unsigned>(app->screen);
  SDL_Log("human-resources initialized backend=%s render=%ux%u content=%s save=%u", app->renderer->backend(), app->render_w, app->render_h, root.string().c_str(), static_cast<unsigned>(loaded.issue));
  if (app->capture)
    SDL_Log("human-resources capture=%ux%u mode=%s world=%ux%u", app->capture_pixels.empty() ? app->render_w : app->capture_pixels.width, app->capture_pixels.empty() ? app->render_h : app->capture_pixels.height, app->capture_pixels.empty() ? "world-sized-composite" : "explicit-pixel-composite", app->render_w, app->render_h);
  if (!app->report.empty()) {
    std::ofstream out(app->report);
    out << "foundation-smoke backend=" << app->renderer->backend() << " device=SDL_GPU driver=unknown\n";
  }
  *state = app.release();
  return SDL_APP_CONTINUE;
}
SDL_AppResult SDL_AppEvent(void* state, SDL_Event* event) {
  auto& app = *static_cast<App*>(state);
  app.devices.handle_device_event(*event);
  if (event->type == SDL_EVENT_QUIT)
    return SDL_APP_SUCCESS;
  hr::lifecycle::Transition lifecycle_transition{.before = app.lifecycle, .after = app.lifecycle};
  if (const auto event_value = hr::platform::translate_lifecycle_event(*event); event_value)
    lifecycle_transition = hr::lifecycle::apply(app.lifecycle, *event_value);
  if (event->type == SDL_EVENT_WINDOW_FOCUS_GAINED || event->type == SDL_EVENT_DID_ENTER_FOREGROUND) {
    app.audio_focused = true;
    app.audio.set_paused(app.save_error);
  }
  if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST || event->type == SDL_EVENT_WILL_ENTER_BACKGROUND || event->type == SDL_EVENT_DID_ENTER_BACKGROUND || event->type == SDL_EVENT_TERMINATING) {
    app.promotion.clear();
    app.audio_focused = false;
    app.audio.set_paused(true);
    if (app.screen == Screen::run) {
      app.game.pause();
      change_screen(app, Screen::pause);
    }
    app.controls.clear();
    app.pressed = Command::none;
    app.previous_ticks = SDL_GetTicksNS();
    if (hr::client::interruption_checkpoint_due(lifecycle_transition) && app.run_id && !app.pending_settlement && !app.store.pending())
      (void)checkpoint_game(app, true);
  }
  haptics_policy(app);
  if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
    app.controls.remove_device(hr::platform::gamepad_device_id(event->gdevice.which));
    app.pressed = Command::none;
  }
  rebuild(app);
  if (const auto source = hr::platform::translate_input_event(*event, app.metrics); source) {
    if (const auto action = app.actions.translate(*source, context); action)
      semantic(app, *action);
  }
  return SDL_APP_CONTINUE;
}
SDL_AppResult SDL_AppIterate(void* state) {
  auto& app = *static_cast<App*>(state);
  haptics_policy(app);
  const auto now = SDL_GetTicksNS();
  const double frame_interval_ms = app.previous_profile_ticks ? static_cast<double>(now - app.previous_profile_ticks) / 1.e6 : 0;
  app.previous_profile_ticks = now;
  const float elapsed = std::min(.1F, static_cast<float>(now - app.previous_ticks) / 1.e9F);
  app.previous_ticks = now;
  if (app.lifecycle.presence != hr::lifecycle::Presence::foreground) {
    app.controls.clear();
    SDL_Delay(20);
    return SDL_APP_CONTINUE;
  }
  if (app.save_error) app.promotion.clear();
  else app.promotion.advance(elapsed);
  for (const auto action : app.accessibility->drain_actions()) {
    const auto command = hr::product::accessibility_command(action, app.ui);
    if (command != Command::none) {
      dispatch(app, command);
      rebuild(app);
      break;
    }
  }
  if (app.resume_qa.active()) {
    const auto command = app.resume_qa.next_command(app.screen, app.save_error, app.restored_run);
    if (command != Command::none) {
      if (command == Command::finish)
        log_run_detail(app);
      dispatch(app, command);
    }
    if (app.screen == Screen::run) {
      hr::input::Action movement;
      movement.id = static_cast<hr::input::ActionId>(ControlAction::axis_x);
      movement.value = app.resume_qa.movement_x(app.game.snapshot().stats.active_seconds);
      movement.phase = hr::input::ActionPhase::changed;
      semantic(app, movement);
    }
  }
  if (app.qa) {
    if (app.screen == Screen::title || app.screen == Screen::upgrade) {
      if (++app.qa_menu_frames >= 45) {
        app.qa_menu_frames = 0;
        dispatch(app, app.screen == Screen::title ? Command::start : Command::pick0);
      }
    } else {
      app.qa_menu_frames = 0;
    }
    if (app.screen == Screen::death)
      dispatch(app, Command::finish);
    else if (app.screen == Screen::results)
      dispatch(app, Command::retry);
    hr::input::Action movement;
    movement.id = static_cast<hr::input::ActionId>(ControlAction::axis_x);
    movement.value = .8F * std::sin(static_cast<float>(app.frames) * app.qa_step * .7F);
    movement.phase = hr::input::ActionPhase::changed;
    semantic(app, movement);
  }
  const auto game_begin = SDL_GetTicksNS();
  const double active_before = app.game.snapshot().stats.active_seconds;
  if (app.playthrough)
    smoke_input(app);
  if (app.screen == Screen::run && !app.save_error && !app.promotion.holds_combat()) {
    const auto control = app.controls.sample();
    for (unsigned i = 0; i < (app.playthrough ? 12U : 1U); ++i)
      app.game.advance(app.qa ? app.qa_step : (app.playthrough ? 1.F / 60 : elapsed), control);
    if(!retain_qa_vehicle_checkpoint(app)) {
      SDL_Log("Cannot retain bounded QA vehicle Game checkpoint");
      return SDL_APP_FAILURE;
    }
    const auto& current = app.game.snapshot();
    if (current.phase == hr::gameplay::Phase::dead) {
      app.audio.play(hr::feedback::Cue::death);
      app.haptics.play(hr::feedback::HapticCue::death, SDL_GetTicks());
    }
    const auto phase = app.game.snapshot().phase;
    if (phase == hr::gameplay::Phase::choosing_upgrade)
      change_screen(app, Screen::upgrade);
    else if (phase == hr::gameplay::Phase::dead)
      change_screen(app, Screen::death);
    if (!app.guest && !app.pending_settlement)
      (void)checkpoint_game(app, phase == hr::gameplay::Phase::choosing_upgrade || phase == hr::gameplay::Phase::dead);
  }
  if (app.resume_qa.active() && !app.save_error && !app.store.pending() && app.resume_qa.take_hold_marker(app.screen)) {
    log_run_detail(app);
    SDL_Log("product-continuation-qa-held screen=%u slot=%s", static_cast<unsigned>(app.screen), std::string(app.resume_qa.slot()).c_str());
  }
  const auto presentation_begin = SDL_GetTicksNS();
  app.scene_effects.update(app.game.snapshot());
  const auto feedback = app.scene_effects.feedback();
  const auto cues = hr::feedback::select_gameplay_cues({
      .capture = feedback.capture, .laser = feedback.laser, .warning = feedback.warning,
      .hit = feedback.hit, .sweep_capture = feedback.sweep_capture,
      .harvest_overdrive = feedback.harvest_overdrive, .phase_dash = feedback.phase_dash});
  for (std::size_t cue = 0; cue < cues.size(); ++cue)
    if (cues[cue])
      app.audio.play(static_cast<hr::feedback::Cue>(cue));
  haptics_policy(app);
  if (feedback.hull_hit)
    app.haptics.play(hr::feedback::HapticCue::hull_hit, SDL_GetTicks());
  else if (feedback.shield_hit)
    app.haptics.play(hr::feedback::HapticCue::shield_hit, SDL_GetTicks());
  else if (feedback.capture)
    app.haptics.play(hr::feedback::HapticCue::capture, SDL_GetTicks());
  app.audio.set_tractor_active(app.screen == Screen::run && !app.save_error && !app.game.snapshot().beams.empty());
  app.audio.set_paused(!app.audio_focused || app.save_error);
  app.audio.update();
  rebuild(app);
  if (!app.metrics.valid) {
    SDL_Delay(16);
    return SDL_APP_CONTINUE;
  }
  int pw{}, ph{};
  SDL_GetWindowSizeInPixels(app.window, &pw, &ph);
  if (pw <= 0 || ph <= 0)
    return SDL_APP_CONTINUE;
  const float factor = std::min(1.F, 1280.F / static_cast<float>(std::max(pw, ph)));
  const auto w = static_cast<unsigned>(std::max(4, static_cast<int>(static_cast<float>(pw) * factor) / 4 * 4)), h = static_cast<unsigned>(std::max(4, static_cast<int>(static_cast<float>(ph) * factor) / 4 * 4));
  if (!app.capture && (w != app.render_w || h != app.render_h)) {
    if (!app.renderer->resize(w, h))
      return SDL_APP_FAILURE;
    app.render_w = w;
    app.render_h = h;
  }
  auto scene = hr::product::scene(app.game.snapshot(), static_cast<float>(app.render_w) / static_cast<float>(app.render_h), (app.store.account().settings.reduced_motion || app.frame_preferences.reduced_motion), app.screen == Screen::title || app.screen == Screen::hangar || app.screen == Screen::settings, &app.scene_effects, app.selected_theme);
  scene.active_delta_seconds = static_cast<float>(std::max(0., app.game.snapshot().stats.active_seconds - active_before));
  scene.screen_quads = app.ui.quads;
  const auto render_begin = SDL_GetTicksNS();
  const bool recording = app.capture && app.frames >= app.qa_capture_skip;
  const bool ok = app.renderer->render(scene, recording ? app.capture : nullptr, true, recording ? app.capture_pixels : hr::renderer::CaptureExtent{});
  if (!ok) {
    SDL_Log("Game rendering failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  const auto render_end = SDL_GetTicksNS();
  if (app.qa_metrics) {
    const auto& snapshot = app.game.snapshot();
    const auto stats = app.renderer->stats();
    app.qa_metrics << app.frames << ',' << static_cast<unsigned>(snapshot.phase) << ',' << snapshot.stats.active_seconds << ',' << snapshot.entities.size() << ',' << snapshot.projectiles.size() << ',' << snapshot.beams.size() << ',' << frame_interval_ms << ',' << static_cast<double>(presentation_begin - game_begin) / 1.e6 << ',' << static_cast<double>(render_begin - presentation_begin) / 1.e6 << ',' << static_cast<double>(render_end - render_begin) / 1.e6 << ',' << static_cast<double>(render_end - now) / 1.e6 << ',' << stats.world_draws << ',' << stats.shadow_draws << ',' << stats.triangles << '\n';
  }
  ++app.frames;
  if (app.qa && app.qa_frames && app.frames >= app.qa_frames)
    return SDL_APP_SUCCESS;
  if ((app.playthrough && app.frames >= 280) || (app.smoke && !app.playthrough && app.frames >= 3)) {
    if (app.playthrough) {
      constexpr unsigned required = (1U << static_cast<unsigned>(Screen::title)) | (1U << static_cast<unsigned>(Screen::run)) | (1U << static_cast<unsigned>(Screen::pause)) | (1U << static_cast<unsigned>(Screen::settings)) | (1U << static_cast<unsigned>(Screen::results)) | (1U << static_cast<unsigned>(Screen::hangar)) | (1U << static_cast<unsigned>(Screen::upgrade));
      SDL_Log("playthrough-smoke frames=%u screen_mask=%u required=%u score=%llu captures=%llu result=%s", app.frames, app.smoke_mask, required, static_cast<unsigned long long>(app.game.snapshot().stats.score), static_cast<unsigned long long>(app.game.snapshot().stats.captures), (app.smoke_mask & required) == required ? "pass" : "fail");
      if ((app.smoke_mask & required) != required)
        return SDL_APP_FAILURE;
    }
    return SDL_APP_SUCCESS;
  }
  if (!app.playthrough && !app.capture) {
    const auto end = SDL_GetTicksNS();
    constexpr Uint64 frame_ns = 1000000000ULL / 60;
    if (end - now < frame_ns)
      SDL_DelayPrecise(frame_ns - (end - now));
  }
  return SDL_APP_CONTINUE;
}
void SDL_AppQuit(void* state, SDL_AppResult) {
  std::unique_ptr<App> app(static_cast<App*>(state));
  if (app) {
    if (app->capture)
      SDL_CloseIO(app->capture);
    if (app->run_id && !app->guest && !app->pending_settlement && !app->store.pending())
      (void)checkpoint_game(*app, true);
    app->accessibility->close();
    app->native_accessibility.reset();
    app->renderer.reset();
    SDL_DestroyWindow(app->window);
  }
  app.reset();
  SDL_Quit();
}
