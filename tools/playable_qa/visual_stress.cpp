// Synthetic bounded visual capacity fixture. This never advances gameplay,
// awards progression, touches a save, or claims a naturally reachable run.
#include "infantry_transition.h"
#include "vehicle_transition.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <human_resources/gameplay/tuning.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/product/visibility.h>
#include <human_resources/renderer/style_renderer.h>
#include <human_resources/storage/storage.h>
#include <iostream>
#include <map>
#include <new>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>

#ifndef HR_QA_SHOT_CAP
#error HR_QA_SHOT_CAP must be extracted from the production combat shot bound.
#endif
namespace allocation_probe {
thread_local bool active{};
thread_local std::size_t calls{}, bytes{};
} // namespace allocation_probe
void* operator new(std::size_t size) {
  if (allocation_probe::active) {
    ++allocation_probe::calls;
    allocation_probe::bytes += size;
  }
  if (auto* value = std::malloc(std::max(std::size_t{1}, size)))
    return value;
  throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
  return ::operator new(size);
}
void operator delete(void* value) noexcept {
  std::free(value);
}
void operator delete[](void* value) noexcept {
  std::free(value);
}

namespace {
namespace hr = human_resources;
namespace tuning = hr::gameplay::tuning;
constexpr unsigned fps = 60, phase_frames = 480, total_frames = phase_frames * 3;
constexpr float step = 1.F / static_cast<float>(fps);
constexpr unsigned organic_count = tuning::biological_active_cap;
constexpr unsigned soldier_count = static_cast<unsigned>(tuning::enemy_active_cap[0]);
constexpr unsigned jeep_count = static_cast<unsigned>(tuning::enemy_active_cap[1]);
constexpr unsigned tank_count = static_cast<unsigned>(tuning::enemy_active_cap[2]);
constexpr unsigned boss_count = static_cast<unsigned>(tuning::enemy_active_cap[3]);
constexpr unsigned actor_count = organic_count + soldier_count + jeep_count + tank_count + boss_count;
constexpr std::array<hr::gameplay::Module, 6> modules{hr::gameplay::Module::beam_splitter, hr::gameplay::Module::tractor_lens, hr::gameplay::Module::laser_array, hr::gameplay::Module::emp_ring, hr::gameplay::Module::orbit_drones, hr::gameplay::Module::shield_generator};
static_assert(tuning::hardware_slots == modules.size());
static_assert(tuning::evolution_beams == 6 && tuning::maximum_evolutions == 2);
static_assert(actor_count <= hr::product::SceneEffects::actor_capacity);

void require(bool valid, const char* message) {
  if (!valid)
    throw std::runtime_error(message);
}
class NoStorage final : public hr::storage::Backend {
public:
  hr::storage::ReadResult read(hr::storage::Scope, const hr::storage::RelativePath&, std::size_t) override {
    throw std::runtime_error("Synthetic visual fixture attempted storage read");
  }
  hr::storage::WriteResult write_atomically(hr::storage::Scope, const hr::storage::RelativePath&, std::span<const std::byte>) override {
    throw std::runtime_error("Synthetic visual fixture attempted storage write");
  }
};
class Fixture final {
public:
  hr::gameplay::Snapshot run;
  Fixture() {
    run.phase = hr::gameplay::Phase::active;
    run.hull = run.maximum_hull = tuning::base_hull + static_cast<float>(tuning::permanent_maximum_ranks[1]) * tuning::permanent_per_rank[1];
    run.shield = run.maximum_shield = tuning::base_shield + static_cast<float>(tuning::permanent_maximum_ranks[2]) * tuning::permanent_per_rank[2] + 3 * tuning::shield_capacity_step;
    run.capture_radius = tuning::base_capture_radius_metres * (1 + 3 * tuning::lens_radius_step);
    run.movement_speed = tuning::base_movement_metres_per_second;
    run.weapon_damage = tuning::base_laser_damage;
    run.beam_capacity = tuning::evolution_beams;
    run.laser_count = static_cast<unsigned>(tuning::base_lasers) + 3;
    run.next_level_xp = 100;
    run.xp = 72;
    run.stats.level = 30;
    run.stats.evolutions = 2;
    run.evolution_bits = (1U << static_cast<unsigned>(hr::gameplay::Evolution::great_vacuum)) | (1U << static_cast<unsigned>(hr::gameplay::Evolution::storm_drain));
    for (const auto module : modules)
      run.modules[static_cast<std::size_t>(module)] = 3;
    run.entities.resize(actor_count);
    run.projectiles.resize(tuning::projectile_active_cap);
    run.beams.resize(tuning::evolution_beams);
    run.shots.resize(HR_QA_SHOT_CAP);
  }
  void update(unsigned frame) {
    const float time = static_cast<float>(frame) * step;
    run.stats.active_seconds = 600 + static_cast<double>(frame) / fps;
    run.runner_distance = 1400 + time * tuning::forward_scroll_metres_per_second;
    run.player = {.6F * std::sin(time * .8F), run.runner_distance};
    run.velocity = {.48F * std::cos(time * .8F), tuning::forward_scroll_metres_per_second};
    run.invulnerability_seconds = .25F;
    run.capture_burst_seconds = .5F;
    // Replace a cohort every two seconds: bounded actor resource churn, without
    // pretending these synthetic snapshots are natural capture/death outcomes.
    const unsigned cohort = frame / 120;
    const auto id = [&](unsigned index) { return cohort * actor_count + index + 1; };
    for (unsigned i = 0; i < actor_count; ++i) {
      auto& entity = run.entities[i];
      entity = {};
      entity.id = id(i);
      entity.maximum_hull = entity.hull = 100;
      if (i < organic_count) {
        entity.kind = static_cast<hr::gameplay::EntityKind>(i % 4);
        const float angle = static_cast<float>(i) * std::numbers::pi_v<float> / 3;
        const float x = i < tuning::evolution_beams ? run.player.x + 3.4F * std::cos(angle) : (static_cast<float>(i % 5) - 2) * 2.5F;
        const float z = i < tuning::evolution_beams ? 3.4F * std::sin(angle) : -11.F + static_cast<float>((i - tuning::evolution_beams) / 5) * 3;
        entity.position = {x, run.runner_distance + z};
        entity.velocity = {.4F * std::cos(time + static_cast<float>(i)), .4F * std::sin(time + static_cast<float>(i))};
      } else {
        const unsigned military = i - organic_count;
        entity.kind = military < soldier_count ? hr::gameplay::EntityKind::soldier : military < soldier_count + jeep_count            ? hr::gameplay::EntityKind::jeep
                                                                                 : military < soldier_count + jeep_count + tank_count ? hr::gameplay::EntityKind::tank
                                                                                                                                      : hr::gameplay::EntityKind::boss;
        if (entity.kind == hr::gameplay::EntityKind::soldier)
          entity.infantry_role = static_cast<hr::gameplay::InfantryRole>(1 + military % 3);
        entity.position = {(static_cast<float>(military % 6) - 2.5F) * 2.3F, run.runner_distance + 5 + static_cast<float>(military / 6) * 3};
        entity.velocity = {.2F, -.6F};
        if (military < tuning::warning_active_cap) {
          entity.warning_seconds = .6F;
          entity.warning_aim = {run.player.x, run.player.z - 5};
        }
      }
      if (i < tuning::evolution_beams) {
        const float progress = .12F + .75F * std::fmod(time * .35F + static_cast<float>(i) / 6, 1.F);
        entity.capture_progress = progress;
        run.beams[i] = {entity.id, entity.position, progress, false};
      }
    }
    for (std::size_t i = 0; i < run.projectiles.size(); ++i) {
      const float row = static_cast<float>(i / 12), column = static_cast<float>(i % 12);
      run.projectiles[i] = {.id = static_cast<std::uint32_t>(i + 1), .position = {(column - 5.5F) * 1.25F, run.runner_distance - 13 + std::fmod(row * 3.2F + time * 3, 26.F)}, .velocity = {.4F, -5.5F}, .radius = .4F, .damage = 12};
    }
    for (std::size_t i = 0; i < run.shots.size(); ++i) {
      const auto& target = run.entities[organic_count + i % (actor_count - organic_count)];
      const bool drone = i % 7 >= 4;
      run.shots[i] = {run.player, target.position, .1F, drone,
          drone ? hr::gameplay::ShotKind::drone : hr::gameplay::ShotKind::laser,
          static_cast<std::uint8_t>(drone ? i % 3 : i % 4)};
    }
    constexpr std::array<hr::gameplay::PresentationEventKind, 6> kinds{hr::gameplay::PresentationEventKind::impact, hr::gameplay::PresentationEventKind::defeat, hr::gameplay::PresentationEventKind::capture, hr::gameplay::PresentationEventKind::shield_hit, hr::gameplay::PresentationEventKind::emp_pulse, hr::gameplay::PresentationEventKind::impact};
    for (std::size_t i = 0; i < run.presentation_events.size(); ++i) {
      const auto& entity = run.entities[i % run.entities.size()];
      const bool emp = kinds[i % kinds.size()] == hr::gameplay::PresentationEventKind::emp_pulse;
      run.presentation_events[i] = {static_cast<std::uint64_t>(frame) * hr::gameplay::presentation_event_capacity + i + 1, run.stats.active_seconds - .12, kinds[i % kinds.size()], emp ? 0 : entity.id, emp ? run.player : entity.position, emp ? tuning::storm_radius : 0};
    }
  }
};
void fixture_contract(const Fixture& fixture) {
  const auto& run = fixture.run;
  require(run.entities.size() == actor_count, "actor capacity proof failed");
  require(run.projectiles.size() == tuning::projectile_active_cap, "projectile capacity proof failed");
  require(run.beams.size() == 6 && run.beam_capacity == 6, "six-beam proof failed");
  require(std::count_if(run.modules.begin(), run.modules.end(), [](auto rank) { return rank != 0; }) == 6, "six-slot proof failed");
  require(std::count_if(run.entities.begin(), run.entities.end(), [](const auto& e) { return e.warning_seconds > 0; }) == tuning::warning_active_cap, "warning capacity proof failed");
}
hr::presentation::StyleFrame present(Fixture& fixture, hr::product::SceneEffects& effects, hr::progression::AccountStore& store, unsigned frame, bool capacity = true) {
  fixture.update(frame);
  if (capacity) {
    fixture_contract(fixture);
  } else {
    // A separately labelled visual composition makes installed gear readable;
    // the measured fixture always retains every production capacity above.
    fixture.run.entities.resize(6);
    fixture.run.projectiles.clear();
    fixture.run.shots.clear();
    fixture.run.presentation_events.fill({});
    fixture.run.presentation_events[0] = {1, fixture.run.stats.active_seconds - .12, hr::gameplay::PresentationEventKind::emp_pulse, 0, fixture.run.player, tuning::storm_radius};
  }
  effects.update(fixture.run);
  const auto theme = static_cast<hr::presentation::SceneryTheme>(frame / phase_frames);
  const bool reduced = frame % phase_frames >= phase_frames - 120;
  auto scene = hr::product::scene(fixture.run, 588.F / 1280.F, reduced, false, &effects, theme);
  scene.active_delta_seconds = step;
  hr::product::View view;
  view.screen = hr::product::Screen::run;
  view.run = &fixture.run;
  view.store = &store;
  view.viewport = {0, 0, 390, 844};
  view.theme = theme;
  scene.screen_quads = hr::product::interface(view).quads;
  const auto mesh_count = [&](std::string_view id) { return std::count_if(scene.instances.begin(), scene.instances.end(), [&](const auto& instance) { return instance.mesh == id; }); };
  require(mesh_count("beam") == 6, "production scene did not produce six capture cones");
  require(mesh_count("ufo-modular") == 1 && mesh_count("ship-drone") + mesh_count("ship-drone-elite") == 3,
      "production scene did not produce the ship plus three armed orbit drones");
  require(mesh_count("ufo-laser") == 4, "production scene did not produce four independently aimed ship lasers");
  require(mesh_count("ship-shield-base") == 1 && mesh_count("ship-shield-crystal") == 3,
      "production scene did not produce installed shield machinery");
  require(std::any_of(scene.instances.begin(), scene.instances.end(), [](const auto& instance) { return instance.shield_rim; }), "production shield rim material absent");
  require(std::any_of(effects.beats().begin(), effects.beats().end(), [](const auto& beat) { return beat.kind == hr::product::SceneBeatKind::emp_pulse && beat.radius == tuning::storm_radius; }), "authoritative radius EMP beat absent");
  require(!capacity || effects.beats().size() == hr::product::SceneEffects::beat_capacity, "bounded effects cache was not saturated");
  return scene;
}

// Separately labelled native composition evidence. These snapshots do not
// advance the Game, earn equipment, or qualify natural encounters/performance.
template <typename Capture>
void milestone_proof(hr::renderer::StyleRenderer& renderer, hr::progression::AccountStore& store, Capture capture, bool composition_only = false) {
  constexpr std::array<std::string_view, 3> themes{"farmland", "suburbia", "desert"};
  const auto present_snapshot = [&](const hr::gameplay::Snapshot& run, unsigned theme, bool reduced = false) {
    auto scene = hr::product::scene(run, 588.F / 1280.F, reduced, false, nullptr, static_cast<hr::presentation::SceneryTheme>(theme));
    scene.active_delta_seconds = step;
    hr::product::View view;
    view.screen = hr::product::Screen::run;
    view.run = &run;
    view.store = &store;
    view.viewport = {0, 0, 390, 844};
    view.theme = static_cast<hr::presentation::SceneryTheme>(theme);
    scene.screen_quads = hr::product::interface(view).quads;
    return scene;
  };
  hr::gameplay::Game starter;
  starter.start({.seed = 514});
  for (float station : {0.F, 33.6F}) {
    auto run = starter.snapshot();
    run.runner_distance = station;
    run.player.z = station;
    capture(present_snapshot(run, 0), station == 0 ? "synthetic-composition-station-0" : "synthetic-composition-station-33_6");
  }
  if (composition_only)
    return;
  for (unsigned theme = 0; theme < themes.size(); ++theme) {
    for (unsigned corner = 0; corner < 4; ++corner) {
      Fixture gear;
      gear.update(0);
      auto& run = gear.run;
      run.runner_distance = 33.6F;
      run.player = {(corner & 1 ? 1.F : -1.F) * tuning::field_half_width, run.runner_distance + (corner & 2 ? 6.F : -6.F)};
      run.velocity = {corner & 1 ? -7.2F : 7.2F, tuning::forward_scroll_metres_per_second};
      run.stats.active_seconds = static_cast<double>(corner) * 1.8479957;
      run.invulnerability_seconds = run.capture_burst_seconds = 0;
      run.entities.clear();
      run.projectiles.clear();
      run.beams.clear();
      run.shots.clear();
      run.presentation_events.fill({});
      capture(present_snapshot(run, theme), "synthetic-gear-extreme-" + std::string(themes[theme]) + "-" + std::to_string(corner));
    }
    auto run = starter.snapshot();
    run.runner_distance = 33.6F;
    run.player = {-tuning::field_half_width, run.runner_distance + 6};
    run.stats.active_seconds = 30;
    for (unsigned role = 1; role <= 3; ++role) {
      hr::gameplay::EntityView actor;
      actor.id = 100 + role;
      actor.kind = hr::gameplay::EntityKind::soldier;
      actor.position = {tuning::field_half_width - .25F, run.runner_distance + 10 - static_cast<float>(role) * 4};
      actor.warning_aim = run.player;
      actor.warning_seconds = .4F;
      actor.hull = actor.maximum_hull = 100;
      actor.infantry_role = static_cast<hr::gameplay::InfantryRole>(role);
      run.entities.push_back(actor);
    }
    run.entities.push_back({.id = 104, .kind = hr::gameplay::EntityKind::cow, .position = {-5.3F, run.runner_distance - 4}, .hull = 10, .maximum_hull = 10});
    run.projectiles.push_back({.id = 200, .position = {5.4F, run.runner_distance - 4}, .velocity = {-2, -3}, .radius = .4F});
    auto scene = present_snapshot(run, theme);
    const auto faded = std::count_if(scene.instances.begin(), scene.instances.end(), [](const auto& i) { return !i.effect && i.tint[3] < .99F; });
    std::cout << "synthetic-candidate-visibility theme=" << themes[theme] << " station=33.6 player=-5.65,+6 faded_props=" << faded << '\n';
    // Advance only renderer animation for the labelled stationary warning pose.
    for (unsigned frame = 0; frame < 30; ++frame)
      require(renderer.render(scene, nullptr, false), SDL_GetError());
    capture(scene, "synthetic-threat-visibility-" + std::string(themes[theme]));
    renderer.reset_actor_animations();
  }
  hr::gameplay::Snapshot fan;
  fan.entities.push_back({.id = 301, .kind = hr::gameplay::EntityKind::soldier, .position = {0, 12}, .warning_aim = {0, -12}, .warning_seconds = 1, .infantry_role = hr::gameplay::InfantryRole::shotgunner});
  const auto muzzle = hr::gameplay::infantry_shot(hr::gameplay::InfantryRole::shotgunner, {0, 12}, {0, -12});
  const auto attack = hr::gameplay::infantry_attack(hr::gameplay::InfantryRole::shotgunner);
  require(muzzle.valid && attack.pellets == 3, "production shotgun fan fixture invalid");
  const float distance = std::hypot(-muzzle.origin.x, -12 - muzzle.origin.z), angle = attack.spread_radians;
  const hr::gameplay::Vec2 edge{muzzle.origin.x + distance * (muzzle.direction.x * std::cos(angle) - muzzle.direction.z * std::sin(angle)), muzzle.origin.z + distance * (muzzle.direction.x * std::sin(angle) + muzzle.direction.z * std::cos(angle))};
  const auto scenery_count = hr::presentation::generate_scenery({hr::presentation::SceneryTheme::farmland, 514, fan.runner_distance, 588.F / 1280.F}).scene.instances.size();
  auto frame = hr::product::scene(fan, 588.F / 1280.F, true);
  frame.instances.insert(frame.instances.begin(), {.mesh = "cactus-tall", .position = {edge.x, 0, -edge.z + 3.5F}, .scale = {.4F, .4F, .4F}});
  auto centre_only = frame;
  auto untyped = fan;
  untyped.entities.front().infantry_role = hr::gameplay::InfantryRole::none;
  hr::product::preserve_play_visibility(centre_only, scenery_count + 1, &untyped);
  hr::product::preserve_play_visibility(frame, scenery_count + 1, &fan);
  require(centre_only.instances.front().tint[3] > .8F, "forced cactus must independently cover the outer fan, not another critical object");
  require(frame.instances.front().tint[3] < .3F, "actual fan edge failed to protect forced cactus");
  std::cout << "synthetic-forced-fan-occluder alpha=" << frame.instances.front().tint[3] << " centre_only_alpha=" << centre_only.instances.front().tint[3] << " aim_z=-12 (unit geometry, outside current player band)\n";
  capture(frame, "synthetic-forced-fan-cactus");
}

// Labelled View fixtures through the real atlas/renderer. OS preference delivery
// and actual touch/recreation remain a separate mobile interaction qualification.
template <typename Capture>
void readable_ui_proof(hr::progression::AccountStore& store, Capture capture) {
  hr::gameplay::Game game;
  game.start({.seed = 514});
  auto run = game.snapshot();
  run.modules[0] = 3; run.modules[1] = 2;
  run.research[0] = 15; run.evolution_bits = 1;
  run.stats.score = 9999;
  run.rerolls_remaining = 2;
  for (unsigned i = 0; i < run.choices.size(); ++i) {
    auto& choice = run.choices[i];
    choice.title = "Overtime Reactor " + std::to_string(i + 1);
    choice.hardware = true; choice.next_rank = 3;
    choice.description = "Jeep capture hull threshold: 60 -> 75%. Uses 2 slots; 8s vehicle cooldown. Tanks <=35% hull. Vehicle kills: +40% bio speed, 5s.";
    choice.evolution_preview = "Hostile Acquisition";
  }
  hr::product::View view;
  view.run = &run; view.store = &store;
  view.viewport = {0,0,320,568}; view.safe_area = {0,20,0,0};
  const auto frame = [&](const std::string& label) {
    const auto ui = hr::product::interface(view);
    auto scene = hr::product::scene(run, 320.F / 568.F, true, false);
    scene.screen_quads = ui.quads;
    std::cout << "synthetic-readable-ui " << label << " scale=" << view.text_scale << " page=" << ui.page << '/' << ui.page_count << " buttons=" << ui.buttons.size() << '\n';
    capture(scene, "synthetic-readable-" + label);
  };
  const auto find = [&](hr::product::Command command, std::string_view text = {}) {
    if (command >= hr::product::Command::pick0 && command <= hr::product::Command::pick2) view.upgrade_preview = static_cast<unsigned>(command) - static_cast<unsigned>(hr::product::Command::pick0);
    view.ui_page = 0;
    const auto count = hr::product::interface(view).page_count;
    for (unsigned i = 0; i < count; ++i) {
      view.ui_page = i;
      const auto ui = hr::product::interface(view);
      if ((!text.empty() && ui.page_text.find(text) != std::string::npos) || (command != hr::product::Command::none && std::any_of(ui.buttons.begin(), ui.buttons.end(), [&](const auto& b) { return b.command == command; }))) return;
    }
    throw std::runtime_error("Requested readable proof page is absent");
  };
  view.screen = hr::product::Screen::run;
  frame("corrected-scale1-run-score");
  const float saved_hull = run.hull, saved_shield = run.shield;
  run.hull = run.maximum_hull = run.shield = run.maximum_shield = 9999;
  run.stats.level = UINT32_MAX;
  frame("corrected-scale1-maximum-hud");
  run.hull = run.maximum_hull = saved_hull; run.shield = run.maximum_shield = saved_shield; run.stats.level = 1;
  view.text_scale = 3.2F; view.screen = hr::product::Screen::upgrade;
  for (unsigned preview = 0; preview < 3; ++preview) {
    view.upgrade_preview = preview; view.ui_page = 0;
    frame("corrected-scale3_2-preview" + std::to_string(preview + 1) + "-actual-name");
  }
  view.upgrade_preview = 1; view.ui_page = 1;
  frame("corrected-scale3_2-choice2-continuation");
  view.save_error = true; view.can_bank_checkpoint = true; view.ui_page = 0;
  view.save_message = "This run cannot resume on this build. Preserve a verified backup and bank its saved credits, or retry with a compatible build.";
  frame("corrected-scale3_2-protected-first");
  find(hr::product::Command::back_up_bank);
  frame("corrected-scale3_2-protected-backup-bank");

}
template <typename Capture>
void privacy_ui_proof(hr::progression::AccountStore& store, Capture capture) {
  hr::gameplay::Game game; game.start({.seed=514});const auto run=game.snapshot();
  hr::product::View view;view.screen=hr::product::Screen::settings;view.run=&run;view.store=&store;
  view.viewport={0,0,320,568};view.safe_area={0,20,0,0};
  const auto frame=[&](const std::string& label) {
    const auto ui=hr::product::interface(view);auto scene=hr::product::scene(run,320.F/568.F,true,false);
    scene.screen_quads=ui.quads;std::cout<<"privacy-proof "<<label<<" page="<<ui.page<<'/'<<ui.page_count<<'\n';
    capture(scene,"privacy-"+label);
  };
  for(float scale:{1.F,3.2F}) {
    view.text_scale=scale;view.show_privacy=false;view.ui_page=0;
    const auto count=hr::product::interface(view).page_count;bool found=false;
    for(unsigned page=0;page<count;++page) {
      view.ui_page=page;const auto ui=hr::product::interface(view);
      if(std::any_of(ui.buttons.begin(),ui.buttons.end(),[](const auto& b){return b.command==hr::product::Command::privacy;})) {found=true;break;}
    }
    require(found,"actual Settings Privacy entry is reachable");const std::string label=scale==1 ? "default-" : "largest-";
    frame(label+"settings-entry");view.show_privacy=true;view.ui_page=0;frame(label+"reader-first");
    view.ui_page=hr::product::interface(view).page_count-1;frame(label+"reader-last");
  }
}
int run(int argc, char** argv) {
  const bool validate = argc == 2 && std::string_view(argv[1]) == "--validate-only";
  const bool proof = argc == 4 && std::string_view(argv[1]) == "--visual-proof";
  const bool milestone = argc == 4 && std::string_view(argv[1]) == "--milestone-proof";
  const bool transition = argc == 4 && std::string_view(argv[1]) == "--infantry-transition";
  const bool privacy = argc == 4 && std::string_view(argv[1]) == "--privacy-proof";
  const bool shots = argc == 4 && std::string_view(argv[1]) == "--vehicle-shot-proof";
  const bool vehicle = argc == 4 && std::string_view(argv[1]) == "--vehicle-transition";
  const bool readable = argc == 4 && std::string_view(argv[1]) == "--readable-ui-proof";
  const bool composition = argc == 4 && std::string_view(argv[1]) == "--composition-proof";
  require(validate || proof || milestone || transition || vehicle || shots || privacy || composition || readable || argc == 3, "usage: visual_stress CONTENT_ROOT FRAMES.csv | --visual-proof CONTENT_ROOT OUTPUT_DIR | --milestone-proof CONTENT_ROOT OUTPUT_DIR | --composition-proof CONTENT_ROOT OUTPUT_DIR | --infantry-transition CONTENT_ROOT FIFO | --readable-ui-proof CONTENT_ROOT OUTPUT_DIR | --validate-only");
  Fixture fixture;
  hr::product::SceneEffects effects;
  NoStorage backend;
  hr::progression::AccountStore store(backend);
  if (validate) {
    hr::gameplay::Game starter;
    starter.start();
    const auto starter_scene = hr::product::scene(starter.snapshot(), 588.F / 1280.F, false);
    const auto mesh_proof = [](const auto& scene) {
      std::map<std::string_view, unsigned> counts;
      for (const auto& instance : scene.instances)
        ++counts[instance.mesh];
      for (const auto& [name, count] : counts)
        std::cout << ' ' << name << '=' << count;
      std::cout << '\n';
    };
    std::cout << "production starter geometry:";
    mesh_proof(starter_scene);
    for (unsigned frame : {0U, 360U, 480U, 840U, 960U, 1320U}) {
      auto scene = present(fixture, effects, store, frame);
      std::cout << "synthetic-capacity frame=" << frame << " theme=" << frame / phase_frames << " scene_instances=" << scene.instances.size() << " six_beams=pass actors=" << fixture.run.entities.size() << " projectiles=" << fixture.run.projectiles.size() << " beats=" << effects.beats().size() << '\n';
      mesh_proof(scene);
    }
    return 0;
  }
  require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
  auto* window = SDL_CreateWindow("Human Resources / SYNTHETIC VISUAL CAPACITY", 390, 844, SDL_WINDOW_HIGH_PIXEL_DENSITY);
  require(window != nullptr, SDL_GetError());
  int result = 0;
  {
    hr::renderer::StyleRenderer renderer;
    const auto ids = hr::product::content_ids();
    constexpr std::array<std::string_view, 8> animated{"cow", "pig", "sheep", "farmer-body", "resident-body", "infantry-rifleman-body", "infantry-shotgunner-body", "infantry-rocketeer-body"};
    const auto preload_start = SDL_GetTicksNS();
    require(renderer.initialize(window, argv[(proof || milestone || transition || vehicle || shots || privacy || composition || readable) ? 2 : 1], (readable || shots || privacy) ? 320U : 588U, (readable || privacy) ? 568U : shots ? 696U : 1280U, ids, animated), SDL_GetError());
    std::cout << "synthetic-visual-capacity initialized_ms=" << static_cast<double>(SDL_GetTicksNS() - preload_start) / 1.e6 << " actors=" << actor_count << " projectiles=" << tuning::projectile_active_cap << " shots=" << HR_QA_SHOT_CAP << " beams=6 slots=6 evolutions=2 beats=" << hr::product::SceneEffects::beat_capacity << " backend=" << renderer.backend() << std::endl;
    if (shots) {
      hrqa::vehicle_shot_readability_proof(renderer, store, argv[3]);
    } else if (vehicle) {
      hrqa::vehicle_transition_proof(renderer, store, argv[3]);
    } else if (transition) {
      hrqa::infantry_transition_proof(renderer, store, argv[3]);
    } else if (proof || milestone || composition || readable || privacy) {
      const auto capture = [&](const hr::presentation::StyleFrame& scene, const std::string& name) {
        const auto path = std::string(argv[3]) + '/' + name + ".rgba";
        auto* stream = SDL_IOFromFile(path.c_str(), "wb");
        require(stream != nullptr, SDL_GetError());
        const bool ok = renderer.render(scene, stream, false);
        SDL_CloseIO(stream);
        require(ok, SDL_GetError());
        const auto stats = renderer.stats();
        std::cout << "synthetic-visual-proof " << name << " instances=" << scene.instances.size() << " world_draws=" << stats.world_draws << " shadow_draws=" << stats.shadow_draws << " triangles=" << stats.triangles << '\n';
      };
      if (privacy) {
        privacy_ui_proof(store, capture);
      } else if (readable) {
        readable_ui_proof(store, capture);
      } else if (milestone || composition) {
        milestone_proof(renderer, store, capture, composition);
      } else {
        hr::gameplay::Game starter;
        starter.start();
        capture(hr::product::scene(starter.snapshot(), 588.F / 1280.F, false), "production-starter");
        constexpr std::array<std::string_view, 3> themes{"farmland", "suburbia", "desert"};
        for (unsigned theme = 0; theme < themes.size(); ++theme)
          for (bool capacity : {false, true}) {
            Fixture visual_fixture;
            hr::product::SceneEffects visual_effects;
            capture(present(visual_fixture, visual_effects, store, theme * phase_frames, capacity), std::string("synthetic-") + (capacity ? "capacity-" : "gear-") + std::string(themes[theme]));
          }
      }
      require(renderer.wait_idle(), SDL_GetError());
    } else {
      std::ofstream csv(argv[2]);
      require(static_cast<bool>(csv), "cannot open stress metrics");
      csv << "frame,theme,reduced_motion,cohort,entities,projectiles,beams,beats,scene_instances,frame_interval_ms,presentation_ms,render_submit_ms,cpu_frame_ms,cpp_allocations,cpp_allocated_bytes,world_draws,shadow_draws,visible_instances,triangles\n";
      Uint64 previous{}, deadline = SDL_GetTicksNS();
      for (unsigned frame = 0; frame < total_frames; ++frame) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
          if (event.type == SDL_EVENT_QUIT) {
            result = 2;
            break;
          }
        if (result)
          break;
        const auto begin = SDL_GetTicksNS();
        const auto interval = previous ? static_cast<double>(begin - previous) / 1.e6 : 0;
        previous = begin;
        allocation_probe::calls = allocation_probe::bytes = 0;
        allocation_probe::active = true;
        auto scene = present(fixture, effects, store, frame);
        const auto render_begin = SDL_GetTicksNS();
        const bool ok = renderer.render(scene);
        const auto end = SDL_GetTicksNS();
        allocation_probe::active = false;
        require(ok, SDL_GetError());
        const auto stats = renderer.stats();
        csv << frame << ',' << frame / phase_frames << ',' << (frame % phase_frames >= phase_frames - 120) << ',' << frame / 120 << ',' << fixture.run.entities.size() << ',' << fixture.run.projectiles.size() << ',' << fixture.run.beams.size() << ',' << effects.beats().size() << ',' << scene.instances.size() << ',' << interval << ',' << static_cast<double>(render_begin - begin) / 1.e6 << ',' << static_cast<double>(end - render_begin) / 1.e6 << ',' << static_cast<double>(end - begin) / 1.e6 << ',' << allocation_probe::calls << ',' << allocation_probe::bytes << ',' << stats.world_draws << ',' << stats.shadow_draws << ',' << stats.visible_instances << ',' << stats.triangles << '\n';
        if (frame + 1 < total_frames) {
          deadline += 1000000000ULL / fps;
          const auto now = SDL_GetTicksNS();
          if (deadline > now)
            SDL_DelayPrecise(deadline - now);
          else
            deadline = now;
        }
      }
      require(renderer.wait_idle(), SDL_GetError());
    }
  }
  SDL_DestroyWindow(window);
  SDL_Quit();
  return result;
}
} // namespace
int main(int argc, char** argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception& error) {
    allocation_probe::active = false;
    std::cerr << "synthetic visual stress failed: " << error.what() << '\n';
    SDL_Quit();
    return 1;
  }
}
