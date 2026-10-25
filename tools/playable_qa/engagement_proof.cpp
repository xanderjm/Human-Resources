// Public-Game review film and separately labelled View layout fixtures.
// No private Game access, forced health/gear/rewards, user input or account I/O.
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <human_resources/audio/product_audio.h>
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/gameplay/tuning.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/renderer/style_renderer.h>
#include <human_resources/storage/storage.h>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace hr = human_resources;
namespace gp = hr::gameplay;
namespace pr = hr::product;
namespace {
constexpr float step = 1.F / 60;
void check(bool valid, const char* error) {
  if (!valid)
    throw std::runtime_error(error);
}
class NoAccountIO final : public hr::storage::Backend {
public:
  hr::storage::ReadResult read(hr::storage::Scope, const hr::storage::RelativePath&, std::size_t) override {
    throw std::runtime_error("Review fixture attempted account read");
  }
  hr::storage::WriteResult write_atomically(hr::storage::Scope, const hr::storage::RelativePath&, std::span<const std::byte>) override {
    throw std::runtime_error("Review fixture attempted account write");
  }
};
struct Outcome {
  unsigned frames{}, drafts{}, dash_events{}, sweeps{}, boosts{}, boss_projectile_peak{};
  double first_sweep{-1}, first_boost{-1}, first_threat{-1}, first_dash{-1}, first_choice{-1}, first_boss_shot{-1};
  gp::RunStats stats;
  float hull{}, shield{};
};
const gp::EntityView* nearest_passive(const gp::Snapshot& run) {
  const gp::EntityView* target = nullptr;
  float nearest = 1.e9F;
  for (const auto& entity : run.entities) {
    if (entity.kind >= gp::EntityKind::soldier)
      continue;
    const float dx = entity.position.x - run.player.x, dz = entity.position.z - run.player.z;
    const float distance = dx * dx + dz * dz;
    if (distance < nearest) {
      nearest = distance;
      target = &entity;
    }
  }
  return target;
}
gp::Control pursuit(const gp::Snapshot& run) {
  const auto* target = nearest_passive(run);
  gp::Control control;
  if (target) {
    const float speed = std::max(1.F, run.movement_speed);
    control.movement = {(target->position.x - run.player.x) * 2 / speed,
        ((target->position.z - run.player.z) * 2 - run.forward_speed) / speed};
  }
  bool danger = false;
  for (const auto& actor : run.entities)
    if (actor.warning_seconds > 0 && std::hypot(actor.position.x - run.player.x, actor.position.z - run.player.z) < 7)
      danger = true;
  for (const auto& shot : run.projectiles)
    if (std::hypot(shot.position.x - run.player.x, shot.position.z - run.player.z) < 3)
      danger = true;
  // One early voluntary evade demonstrates agency; later evades react to real
  // nearby warnings/projectiles. The actual Game owns readiness and targeting.
  control.dash = run.dash_cooldown_seconds <= 0 && (danger || (run.stats.active_seconds >= 4.5 && run.stats.active_seconds < 4.55));
  return control;
}
gp::Control input_policy(const gp::Snapshot& run, bool extents, bool composition) {
  if (composition && run.stats.active_seconds < 1.3)
    return {{0, run.stats.active_seconds < .7 ? 1.F : 0}, false};
  return extents ? gp::Control{{0, run.stats.active_seconds < 5 ? 1.F : -1.F}, false} : pursuit(run);
}
template <typename Visit>
Outcome play(Visit visit, const std::filesystem::path& continuation = {}, bool extents = false, bool composition = false) {
  gp::Game game;
  game.start({.seed = 514});
  if (!continuation.empty()) {
    std::ifstream input(continuation, std::ios::binary | std::ios::ate);
    check(static_cast<bool>(input), "Cannot open actual boss continuation");
    const auto size = input.tellg();
    check(size > 0 && size <= 48 * 1024, "Actual continuation exceeds Game bound");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    check(static_cast<bool>(input) && game.restore_state(bytes) == gp::SaveStateIssue::none, "Actual boss continuation is incompatible; no state patch allowed");
  }
  const double end_seconds = game.snapshot().stats.active_seconds + (!continuation.empty() ? 12 : extents   ? 15
                                                                                              : composition ? 8
                                                                                                            : 120);
  Outcome result;
  std::uint64_t last_event = 0;
  for (const auto& event : game.snapshot().presentation_events)
    last_event = std::max(last_event, event.sequence);
  unsigned choice_hold = 0;
  pr::PromotionFlow promotion;
  bool draft_open = false;
  std::optional<gp::Control> collected_control;
  while (game.snapshot().stats.active_seconds < end_seconds && result.frames < 10000) {
    promotion.advance(step);
    const auto phase = game.snapshot().phase;
    if (phase == gp::Phase::dead) {
      visit(game, result.frames++, pr::Screen::death, result, promotion.frame(), promotion.holds_combat());
      break;
    }
    if (promotion.holds_combat()) {
      const auto control = input_policy(game.snapshot(), extents, composition);
      const bool dash = control.dash || (collected_control && collected_control->dash);
      collected_control = control;
      collected_control->dash = dash;
    } else if (phase == gp::Phase::choosing_upgrade) {
      if (!draft_open) {
        promotion.enter();
        draft_open = true;
      }
      if (result.first_choice < 0)
        result.first_choice = game.snapshot().stats.active_seconds;
      if (++choice_hold >= 90) {
        const auto selected = game.snapshot().choices[0];
        check(game.choose_upgrade(0), "Actual earned first choice was rejected");
        ++result.drafts;
        choice_hold = 0;
        promotion.confirm(selected);
        draft_open = false;
      }
    } else {
      const auto control = collected_control.value_or(input_policy(game.snapshot(), extents, composition));
      collected_control.reset();
      game.advance(step, control);
      if (game.snapshot().phase == gp::Phase::choosing_upgrade) {
        promotion.enter();
        draft_open = true;
      }
    }
    const auto& run = game.snapshot();
    for (const auto& actor : run.entities)
      if (actor.kind >= gp::EntityKind::soldier && result.first_threat < 0)
        result.first_threat = run.stats.active_seconds;
    unsigned boss_projectiles = 0;
    for (const auto& projectile : run.projectiles)
      boss_projectiles += projectile.kind == gp::EnemyProjectileKind::boss_missile;
    result.boss_projectile_peak = std::max(result.boss_projectile_peak, boss_projectiles);
    if (boss_projectiles && result.first_boss_shot < 0)
      result.first_boss_shot = run.stats.active_seconds;
    auto events = run.presentation_events;
    std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.sequence < b.sequence; });
    for (const auto& event : events) {
      if (event.sequence <= last_event)
        continue;
      last_event = event.sequence;
      if (event.kind == gp::PresentationEventKind::sweep_capture) {
        ++result.sweeps;
        if (result.first_sweep < 0)
          result.first_sweep = run.stats.active_seconds;
      }
      if (event.kind == gp::PresentationEventKind::harvest_overdrive) {
        ++result.boosts;
        if (result.first_boost < 0)
          result.first_boost = run.stats.active_seconds;
      }
      if (event.kind == gp::PresentationEventKind::phase_dash) {
        ++result.dash_events;
        if (result.first_dash < 0)
          result.first_dash = run.stats.active_seconds;
      }
    }
    visit(game, result.frames++, !promotion.holds_combat() && run.phase == gp::Phase::choosing_upgrade ? pr::Screen::upgrade : pr::Screen::run, result, promotion.frame(), promotion.holds_combat());
  }
  result.stats = game.snapshot().stats;
  result.hull = game.snapshot().hull;
  result.shield = game.snapshot().shield;
  return result;
}
void outcome(const Outcome& r) {
  std::cout << "{\"frames\":" << r.frames << ",\"drafts\":" << r.drafts << ",\"sweeps\":" << r.sweeps << ",\"boosts\":" << r.boosts << ",\"dash_events\":" << r.dash_events
            << ",\"first_sweep\":" << r.first_sweep << ",\"first_boost\":" << r.first_boost << ",\"first_dash\":" << r.first_dash << ",\"first_threat\":" << r.first_threat << ",\"first_choice\":" << r.first_choice
            << ",\"boss_projectile_peak\":" << r.boss_projectile_peak << ",\"first_boss_shot\":" << r.first_boss_shot << ",\"seconds\":" << r.stats.active_seconds << ",\"captures\":" << r.stats.captures << ",\"score\":" << r.stats.score << ",\"level\":" << r.stats.level << ",\"hull\":" << r.hull << ",\"shield\":" << r.shield << "}\n";
}
void ui_frames(SDL_Window* window, hr::renderer::StyleRenderer& renderer, hr::progression::AccountStore& store, const std::filesystem::path& output, bool focused) {
  gp::Game game;
  game.start({.seed = 514});
  auto run = game.snapshot();
  pr::View view{pr::Screen::run, &run, &store, {0, 0, 390, 844}, {0, 20, 0, 0}};
  const auto capture = [&](const std::string& name) {
    constexpr std::array<std::string_view, 8> requested{"actual-starter-run-390", "actual-earned-first-choice-390", "actual-earned-first-choice-320", "actual-selected-receipt-390", "actual-selected-receipt-max-text-320", "synthetic-max-text-choice-320", "synthetic-hull-hit-320", "synthetic-shield-hit-max-text-320"};
    if (focused && std::find(requested.begin(), requested.end(), name) == requested.end())
      return;
    const unsigned width = static_cast<unsigned>(view.viewport.width), height = static_cast<unsigned>(view.viewport.height);
    check(SDL_SetWindowSize(window, static_cast<int>(width), static_cast<int>(height)), SDL_GetError());
    SDL_PumpEvents();
    const float factor = std::min(1.F, 1280.F / static_cast<float>(height * 2));
    const unsigned world_width = static_cast<unsigned>(static_cast<float>(width * 2) * factor) / 4 * 4;
    const unsigned world_height = static_cast<unsigned>(static_cast<float>(height * 2) * factor) / 4 * 4;
    check(renderer.resize(world_width, world_height), SDL_GetError());
    renderer.reset_actor_animations();
    auto scene = pr::scene(run, static_cast<float>(world_width) / static_cast<float>(world_height), true);
    const auto ui = pr::interface(view);
    scene.screen_quads = ui.quads;
    const auto path = output / (name + ".rgba");
    auto* stream = SDL_IOFromFile(path.string().c_str(), "wb");
    check(stream != nullptr, SDL_GetError());
    const bool valid = renderer.render(scene, stream, false, {width * 2, height * 2});
    SDL_CloseIO(stream);
    check(valid, SDL_GetError());
    std::cout << "ui-frame " << name << " width=" << width * 2 << " height=" << height * 2 << " page=" << ui.page << "/" << ui.page_count << std::endl;
  };
  capture("actual-starter-run-390");
  while (game.snapshot().phase == gp::Phase::active && game.snapshot().stats.active_seconds < 20)
    game.advance(step, pursuit(game.snapshot()));
  check(game.snapshot().phase == gp::Phase::choosing_upgrade, "Natural first draft not reached");
  run = game.snapshot();
  const auto earned = run;
  view.screen = pr::Screen::upgrade;
  capture("actual-earned-first-choice-390");
  view.viewport = {0, 0, 320, 568};
  capture("actual-earned-first-choice-320");
  view.viewport = {0, 0, 390, 844};
  pr::PromotionFlow confirmation;
  confirmation.confirm(run.choices[0]);
  check(game.choose_upgrade(0), "Actual fixture earned choice was rejected");
  confirmation.advance(.3F);
  check(!confirmation.holds_combat() && confirmation.frame().installed, "Post-hold receipt must remain visible after combat resumes");
  run = game.snapshot();
  view.screen = pr::Screen::run;
  view.promotion = confirmation.frame();
  capture("actual-selected-receipt-390");
  view.viewport = {0, 0, 320, 568};
  view.text_scale = 3.2F;
  capture("actual-selected-receipt-max-text-320");
  view.promotion = {};
  view.text_scale = 1;
  view.screen = pr::Screen::upgrade;
  run = earned;
  for (auto& choice : run.choices) {
    choice.hardware = true;
    choice.module = gp::Module::heavy_winch;
    choice.next_rank = 3;
    choice.title = "Heavy Winch";
    choice.before = 60;
    choice.after = 75;
    choice.description = "Jeep capture hull threshold: 60 -> 75%. Uses 2 slots; 8s vehicle cooldown. Tanks <=35% hull. Vehicle kills: +40% bio speed, 5s.";
    choice.evolution_preview = "Hostile Acquisition";
  }
  view.viewport = {0, 0, 320, 568};
  capture("synthetic-long-rank3-choice-320");
  view.text_scale = 3.2F;
  capture("synthetic-max-text-choice-320");
  run = earned;
  view.viewport = {0, 0, 390, 844};
  view.text_scale = 1;
  view.screen = pr::Screen::pause;
  view.show_build = true;
  run.modules.fill(0);
  run.research.fill(0);
  run.evolution_bits = 0;
  capture("synthetic-empty-build-390");
  for (unsigned i = 0; i < 6; ++i)
    run.modules[i] = 3;
  run.research.fill(15);
  run.evolution_bits = 3;
  for (unsigned page = 0; page < 3; ++page) {
    view.ui_page = page;
    capture("synthetic-full-build-390-page" + std::to_string(page));
  }
  view.viewport = {0, 0, 320, 568};
  view.text_scale = 3.2F;
  view.ui_page = 0;
  capture("synthetic-max-text-build-320");
  view.show_build = false;
  view.screen = pr::Screen::run;
  view.text_scale = 1;
  run = earned; // Restore the naturally reached starter equipment, including rank-zero Phase Drive.
  run.phase = gp::Phase::active;
  run.harvest_chain = 2;
  run.harvest_chain_seconds = 3.2F;
  run.harvest_overdrive_seconds = 0;
  run.dash_cooldown_seconds = 0;
  capture("synthetic-chain-and-starter-dash-320");
  run.harvest_chain = 0;
  run.harvest_overdrive_seconds = 4.2F;
  run.dash_cooldown_seconds = 1.2F;
  view.text_scale = 3.2F;
  capture("synthetic-max-boost-and-dash-cooldown-320");
  view.text_scale = 1;
  run.harvest_overdrive_seconds = 0;
  run.dash_cooldown_seconds = 0;
  run.stats.active_seconds = 1;
  capture("synthetic-first-flight-guide-320");
  run.stats.active_seconds = 6;
  capture("synthetic-close-sweep-guide-320");
  for (bool hull : {false, true}) {
    view.damage_feedback = {.1F, hull};
    view.text_scale = 1;
    capture(hull ? "synthetic-hull-hit-320" : "synthetic-shield-hit-320");
    view.text_scale = 3.2F;
    capture(hull ? "synthetic-hull-hit-max-text-320" : "synthetic-shield-hit-max-text-320");
  }
}
void selection_return(hr::renderer::StyleRenderer& renderer, hr::progression::AccountStore& store, const std::filesystem::path& output) {
  gp::Game game;
  game.start({.seed = 514});
  while (game.snapshot().phase == gp::Phase::active && game.snapshot().stats.active_seconds < 20)
    game.advance(step, pursuit(game.snapshot()));
  check(game.snapshot().phase == gp::Phase::choosing_upgrade, "Selection return requires an actual earned draft");
  const auto selected = game.snapshot().choices[0];
  pr::PromotionFlow promotion;
  promotion.enter();
  pr::SceneEffects effects;
  effects.update(game.snapshot());
  renderer.reset_actor_animations();
  auto* stream = SDL_IOFromFile((output / "ui-return.rgba").string().c_str(), "wb");
  check(stream != nullptr, SDL_GetError());
  std::ofstream trace(output / "ui-return.csv");
  trace << "frame,presentation_seconds,active_seconds,hold,receipt\n";
  double rendered_seconds = game.snapshot().stats.active_seconds;
  for (unsigned frame = 0; frame < 90; ++frame) {
    if (frame)
      promotion.advance(step);
    if (frame == 15) {
      check(game.choose_upgrade(0), "Actual selection-return choice rejected");
      promotion.confirm(selected);
    }
    if (frame >= 15 && !promotion.holds_combat())
      game.advance(step, pursuit(game.snapshot()));
    const auto& run = game.snapshot();
    effects.update(run);
    trace << frame << ',' << static_cast<float>(frame) * step << ',' << run.stats.active_seconds << ',' << promotion.holds_combat() << ',' << (promotion.frame().installed != nullptr) << '\n';
    if (frame % 2)
      continue;
    auto scene = pr::scene(run, 588.F / 1280, false, false, &effects);
    scene.active_delta_seconds = static_cast<float>(run.stats.active_seconds - rendered_seconds);
    rendered_seconds = run.stats.active_seconds;
    pr::View view{frame < 15 ? pr::Screen::upgrade : pr::Screen::run, &run, &store, {0, 0, 390, 844}, {0, 20, 0, 0}};
    view.promotion = promotion.frame();
    view.damage_feedback = effects.damage_feedback();
    scene.screen_quads = pr::interface(view).quads;
    check(renderer.render(scene, stream, false, {390, 844}), SDL_GetError());
  }
  SDL_CloseIO(stream);
}
int execute(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--validate-only") {
    const auto result = play([](gp::Game&, unsigned, pr::Screen, const Outcome&, pr::PromotionFrame, bool) {});
    outcome(result);
    check(result.first_boost > 0 && result.first_sweep > 0 && result.dash_events > 0 && result.drafts > 0 && result.first_threat > 0, "Public-Game review route must exercise real engagement outcomes");
    return 0;
  }
  const bool boss = argc == 5 && std::string_view(argv[1]) == "--boss";
  check(argc == 4 || boss, "usage: engagement_proof --movie|--extents|--ui CONTENT_ROOT OUTPUT_DIR | --boss CONTENT_ROOT OUTPUT_DIR ACTUAL_CHECKPOINT");
  const bool extents = std::string_view(argv[1]) == "--extents";
  const bool composition = std::string_view(argv[1]) == "--composition";
  const bool focused = std::string_view(argv[1]) == "--ui-review";
  const bool movie = std::string_view(argv[1]) == "--movie" || boss || extents || composition;
  check(movie || focused || std::string_view(argv[1]) == "--ui", "Unknown proof mode");
  const std::filesystem::path output = argv[3];
  SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
  SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
  check(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
  auto* window = SDL_CreateWindow("Human Resources / isolated quality proof", 390, 844, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
  check(window != nullptr, SDL_GetError());
  {
    hr::renderer::StyleRenderer renderer;
    const auto ids = pr::content_ids();
    constexpr std::array<std::string_view, 8> animated{"cow", "pig", "sheep", "farmer-body", "resident-body", "infantry-rifleman-body", "infantry-shotgunner-body", "infantry-rocketeer-body"};
    check(renderer.initialize(window, argv[2], 588, 1280, ids, animated), SDL_GetError());
    NoAccountIO backend;
    hr::progression::AccountStore store{backend};
    if (!movie) {
      ui_frames(window, renderer, store, output, focused);
      check(SDL_SetWindowSize(window, 390, 844), SDL_GetError());
      SDL_PumpEvents();
      check(renderer.resize(588, 1280), SDL_GetError());
      selection_return(renderer, store, output);
    } else {
      auto* stream = SDL_IOFromFile((output / "movie.rgba").string().c_str(), "wb");
      check(stream != nullptr, SDL_GetError());
      std::ofstream csv(output / "frames.csv");
      csv << "frame,active_seconds,phase,level,captures,score,hull,shield,chain,chain_seconds,overdrive_seconds,dash_seconds,entities,projectiles,sweeps,boosts,dashes,boss_warnings,boss_projectiles,forward_speed,player_x,player_relative_z,promotion_entry,promotion_hold,promotion_receipt,damage_age,hull_hit,runner_distance,passive_id,passive_kind,passive_x,passive_z,passive_vx,passive_vz,ground_id,ground_x,ground_z,ground_capture_progress,ground_beam_owned\n";
      pr::SceneEffects effects;
      hr::feedback::Mixer mixer;
      std::ofstream audio(output / "cues.s16le", std::ios::binary);
      std::array<float, 1600> samples{};
      std::array<std::int16_t, 1600> pcm{};
      unsigned previous_drafts = 0;
      std::array<bool, 5> saved{};
      double rendered_seconds = 0;
      std::vector<std::uint32_t> ever_beamed;
      const auto result = play([&](gp::Game& game, unsigned frame, pr::Screen screen, const Outcome& current, pr::PromotionFrame promotion, bool holding) {
        const auto& run = game.snapshot();
        if (frame == 0 && boss)
          rendered_seconds = run.stats.active_seconds - step;
        effects.update(run);
        const auto feedback = effects.feedback();
        const auto cues = hr::feedback::select_gameplay_cues({.capture = feedback.capture, .laser = feedback.laser, .warning = feedback.warning, .hit = feedback.hit, .sweep_capture = feedback.sweep_capture, .harvest_overdrive = feedback.harvest_overdrive, .phase_dash = feedback.phase_dash});
        for (unsigned i = 0; i < cues.size(); ++i)
          if (cues[i])
            (void)mixer.play(static_cast<hr::feedback::Cue>(i));
        if (current.drafts > previous_drafts)
          (void)mixer.play(hr::feedback::Cue::upgrade);
        previous_drafts = current.drafts;
        if (screen == pr::Screen::death)
          (void)mixer.play(hr::feedback::Cue::death);
        mixer.set_tractor_active(screen == pr::Screen::run && !run.beams.empty());
        mixer.render(samples);
        for (unsigned i = 0; i < samples.size(); ++i)
          pcm[i] = static_cast<std::int16_t>(std::clamp(samples[i], -1.F, 1.F) * 32767.F);
        audio.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(sizeof(pcm)));
        check(static_cast<bool>(audio), "Cannot retain synchronized source Mixer PCM");
        const std::array<bool, 5> ready{current.sweeps > 0, current.boosts > 0, current.dash_events > 0, current.first_threat >= 0, screen == pr::Screen::upgrade};
        constexpr std::array<std::string_view, 5> names{"first-sweep", "first-overdrive", "first-dash", "first-threat", "first-earned-choice"};
        for (unsigned i = 0; i < saved.size(); ++i)
          if (ready[i] && !saved[i]) {
            const auto bytes = game.save_state();
            check(gp::inspect_saved_state(bytes).valid(), "Actual Game checkpoint failed its current codec");
            std::ofstream checkpoint(output / (std::string(names[i]) + ".hrs"), std::ios::binary);
            checkpoint.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            check(static_cast<bool>(checkpoint), "Cannot preserve actual Game checkpoint");
            saved[i] = true;
          }
        unsigned boss_warnings = 0, boss_projectiles = 0;
        for (const auto& actor : run.entities)
          boss_warnings += actor.kind == gp::EntityKind::boss && actor.warning_seconds > 0;
        for (const auto& projectile : run.projectiles)
          boss_projectiles += projectile.kind == gp::EnemyProjectileKind::boss_missile;
        const auto damage = effects.damage_feedback();
        const auto* passive = nearest_passive(run);
        for (const auto& beam : run.beams)
          if (std::find(ever_beamed.begin(), ever_beamed.end(), beam.target_id) == ever_beamed.end())
            ever_beamed.push_back(beam.target_id);
        for (const auto& actor : run.entities)
          if (actor.capture_progress > 0 && std::find(ever_beamed.begin(), ever_beamed.end(), actor.id) == ever_beamed.end())
            ever_beamed.push_back(actor.id);
        const gp::EntityView* ground = nullptr;
        float nearest_ground = 49;
        for (const auto& actor : run.entities) {
          if (actor.kind >= gp::EntityKind::soldier || actor.capture_progress > 0 || std::find(ever_beamed.begin(), ever_beamed.end(), actor.id) != ever_beamed.end())
            continue;
          const float dx = actor.position.x - run.player.x, dz = actor.position.z - run.player.z, distance = dx * dx + dz * dz;
          if (distance < nearest_ground) {
            nearest_ground = distance;
            ground = &actor;
          }
        }
        csv << frame << ',' << run.stats.active_seconds << ',' << static_cast<unsigned>(run.phase) << ',' << run.stats.level << ',' << run.stats.captures << ',' << run.stats.score << ',' << run.hull << ',' << run.shield << ',' << static_cast<unsigned>(run.harvest_chain) << ',' << run.harvest_chain_seconds << ',' << run.harvest_overdrive_seconds << ',' << run.dash_cooldown_seconds << ',' << run.entities.size() << ',' << run.projectiles.size() << ',' << current.sweeps << ',' << current.boosts << ',' << current.dash_events << ',' << boss_warnings << ',' << boss_projectiles << ',' << run.forward_speed << ',' << run.player.x << ',' << run.player.z - run.runner_distance << ',' << promotion.entry_seconds << ',' << holding << ',' << (promotion.installed ? 1 : 0) << ',' << damage.age << ',' << damage.hull << ',' << run.runner_distance << ',' << (passive ? passive->id : 0) << ',' << (passive ? static_cast<unsigned>(passive->kind) : 0) << ',' << (passive ? passive->position.x : 0) << ',' << (passive ? passive->position.z : 0) << ',' << (passive ? passive->velocity.x : 0) << ',' << (passive ? passive->velocity.z : 0) << ',' << (ground ? ground->id : 0) << ',' << (ground ? ground->position.x : 0) << ',' << (ground ? ground->position.z : 0) << ',' << (ground ? ground->capture_progress : 0) << ',' << (ground && std::any_of(run.beams.begin(), run.beams.end(), [&](const auto& beam) { return beam.target_id == ground->id; })) << '\n';
        if (frame % 2)
          return;
        pr::View view{screen, &run, &store, {0, 0, 390, 844}, {0, 20, 0, 0}};
        view.promotion = promotion;
        view.damage_feedback = effects.damage_feedback();
        auto scene = pr::scene(run, 588.F / 1280, false, false, &effects);
        scene.active_delta_seconds = static_cast<float>(run.stats.active_seconds - rendered_seconds);
        rendered_seconds = run.stats.active_seconds;
        scene.screen_quads = pr::interface(view).quads;
        check(renderer.render(scene, stream, false, {390, 844}), SDL_GetError());
      },
          boss ? std::filesystem::path(argv[4]) : std::filesystem::path{}, extents, composition);
      SDL_CloseIO(stream);
      outcome(result);
    }
    check(renderer.wait_idle(), SDL_GetError());
  }
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
} // namespace
int main(int argc, char** argv) {
  try {
    return execute(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << "engagement proof failed: " << error.what() << '\n';
    SDL_Quit();
    return 1;
  }
}
