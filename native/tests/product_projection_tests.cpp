#include <human_resources/gameplay/tuning.h>
#include <human_resources/product/controls.h>
#include <human_resources/product/presentation.h>

#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
namespace hr = human_resources;
using test_support::require;
using test_support::require_near;
constexpr float phone_aspect = 390.F / 844.F;

hr::ui::Point projected(const hr::presentation::StyleFrame& frame, hr::presentation::Vec3 p) {
  const auto& m = frame.camera;
  const float w = m[12] * p.x + m[13] * p.y + m[14] * p.z + m[15];
  return {(m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3]) / w,
      -(m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7]) / w};
}
const hr::presentation::StyleInstance& mesh(const hr::presentation::StyleFrame& frame, std::string_view id) {
  const auto it = std::find_if(frame.instances.begin(), frame.instances.end(), [&](const auto& instance) { return instance.mesh == id; });
  require(it != frame.instances.end(), "required public scene instance exists");
  return *it;
}
hr::presentation::Vec3 transformed(const hr::presentation::StyleInstance& instance, hr::presentation::Vec3 p) {
  const auto m = hr::presentation::model_matrix(instance);
  return {m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3],
      m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7],
      m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]};
}
hr::input::Action action(hr::product::ControlAction id, hr::input::ActionPhase phase, float value = 1) {
  hr::input::Action a;
  a.id = static_cast<hr::input::ActionId>(id);
  a.phase = phase;
  a.value = value;
  a.device = 1;
  a.source_control = static_cast<int>(id);
  return a;
}
hr::ui::Point moved_ship(hr::product::Controls& controls) {
  hr::gameplay::Game game;
  game.start({.seed = 731});
  for (unsigned i = 0; i < 24; ++i)
    game.advance(1.F / 60, controls.sample());
  const auto frame = hr::product::scene(game.snapshot(), phone_aspect, true);
  return projected(frame, mesh(frame, "ufo-modular").position);
}
void screen_directions_follow_semantic_controls() {
  hr::product::Controls neutral;
  const auto origin = moved_ship(neutral);
  for (const auto direction : {hr::product::ControlAction::up, hr::product::ControlAction::down,
           hr::product::ControlAction::left, hr::product::ControlAction::right}) {
    hr::product::Controls controls;
    controls.apply(action(direction, hr::input::ActionPhase::started), true, false);
    const auto position = moved_ship(controls);
    if (direction == hr::product::ControlAction::up)
      require(position.y < origin.y - .01F, "W/Up semantic movement moves the actual projected ship up");
    if (direction == hr::product::ControlAction::down)
      require(position.y > origin.y + .01F, "S/Down semantic movement moves the actual projected ship down");
    if (direction == hr::product::ControlAction::left)
      require(position.x < origin.x - .01F, "A/Left semantic movement moves the actual projected ship left");
    if (direction == hr::product::ControlAction::right)
      require(position.x > origin.x + .01F, "D/Right semantic movement moves the actual projected ship right");
  }
  hr::product::Controls touch;
  auto a = action(hr::product::ControlAction::contact, hr::input::ActionPhase::started);
  a.contact = 9;
  a.source_kind = hr::input::DeviceKind::touch;
  a.has_position = true;
  a.position = {180, 600};
  touch.apply(a, true, false);
  a.phase = hr::input::ActionPhase::changed;
  a.position.y -= 44;
  touch.apply(a, true, false);
  require(moved_ship(touch).y < origin.y - .01F, "upward finger drag moves the projected ship up");
  hr::product::Controls pad;
  a = action(hr::product::ControlAction::axis_y, hr::input::ActionPhase::changed, -1);
  a.source_kind = hr::input::DeviceKind::gamepad;
  pad.apply(a, true, false);
  require(moved_ship(pad).y < origin.y - .01F, "controller stick up moves the projected ship up");
}
void actor_and_effect_geometry_share_one_forward_axis(bool reduced_motion) {
  hr::gameplay::Snapshot run;
  run.runner_distance = 100;
  run.player = {1, 100};
  run.velocity = {5, 0};
  run.entities = {{.id = 17, .kind = hr::gameplay::EntityKind::cow, .position = {1, 108}, .velocity = {0, 1}},
      {.id = 18, .kind = hr::gameplay::EntityKind::soldier, .position = {3, 104}, .velocity = {0, 1}, .warning_aim = {1, 100}, .warning_seconds = .5F, .infantry_role = hr::gameplay::InfantryRole::rifleman}};
  run.beams = {{.target_id = 17, .target = {1, 108}, .progress = .4F}};
  run.shots = {{.from = run.player, .to = {1, 108}, .remaining_seconds = .1F}};
  run.projectiles = {{.id = 19, .position = {1, 108}, .velocity = {0, -1}, .radius = .23F, .damage = 1}};
  const auto frame = hr::product::scene(run, phone_aspect, reduced_motion);
  const auto& ship = mesh(frame, "ufo-modular");
  const auto& cow = mesh(frame, "cow");
  const auto& soldier = mesh(frame, "infantry-rifleman-body");
  const auto infantry_muzzle = hr::gameplay::infantry_shot(hr::gameplay::InfantryRole::rifleman, run.entities[1].position, run.entities[1].warning_aim);
  require(infantry_muzzle.valid && soldier.stable_id == 18, "typed infantry fixture exposes its real body and valid warning muzzle");
  const hr::presentation::Vec3 warning_origin{infantry_muzzle.origin.x, .08F, run.runner_distance - infantry_muzzle.origin.z + 2.3F};
  const auto& beam = mesh(frame, "beam");
  const auto& gun = mesh(frame, "ufo-laser");
  // Assembly-guide socket and measured source-part muzzle, transformed through
  // the actual mounted geometry. Do not assume rays originate at ship centre.
  const auto socket = transformed(ship, {-.36F, -.16F, .29F});
  require_near(gun.position.x, socket.x, .0001F, "laser mount follows authored banked fixed-hull socket X");
  require_near(gun.position.y, socket.y, .0001F, "laser mount follows authored banked fixed-hull socket Y");
  require_near(gun.position.z, socket.z, .0001F, "laser mount follows authored banked fixed-hull socket Z");
  const auto muzzle = transformed(gun, {0, 0, .65F});
  const hr::presentation::Vec3 target{cow.position.x, cow.position.y + .65F, cow.position.z};
  require(projected(frame, cow.position).y < projected(frame, ship.position).y,
      "actor eight metres ahead appears above the ship");
  require(cow.position.z < ship.position.z, "forward gameplay station follows scenery's negative local Z");
  require_near(beam.position.x, 1, .0001F, "capture beam base aligns with authoritative target X");
  require_near(beam.position.z, -5.7F, .0001F, "capture beam base aligns with authoritative target Z");
  require(std::cos(cow.rotation.y) < -.99F, "forward walking actor faces rendered forward direction");
  bool warning = false, laser = false, projectile = false;
  unsigned reticle_ticks = 0;
  float reticle_x = 0, reticle_z = 0;
  for (const auto& instance : frame.instances) {
    if (instance.mesh != "cube" || !instance.effect)
      continue;
    if (std::abs(instance.tint[1] - .33F) < .001F) {
      warning = true;
      require(instance.position.x <= std::max(warning_origin.x, ship.position.x) && instance.position.x >= std::min(warning_origin.x, ship.position.x), "warning dash stays between actual infantry muzzle and committed aim X");
      require(instance.position.z >= std::min(warning_origin.z, ship.position.z) && instance.position.z <= std::max(warning_origin.z, ship.position.z), "warning dash stays between actual infantry muzzle and committed aim Z");
    } else if (std::abs(instance.tint[1] - .93F) < .001F) {
      laser = true;
      const auto start = transformed(instance, {0, 0, -1});
      const auto end = transformed(instance, {0, 0, 1});
      require_near(start.x, muzzle.x, .0001F, "ray starts at actual rendered muzzle X");
      require_near(start.y, muzzle.y, .0001F, "ray starts at actual rendered muzzle Y");
      require_near(start.z, muzzle.z, .0001F, "ray starts at actual rendered muzzle Z");
      require_near(end.x, target.x, .0001F, "ray reaches common actor target X");
      require_near(end.y, target.y, .0001F, "ray reaches common actor target height");
      require_near(end.z, target.z, .0001F, "ray reaches common actor target forward station");
      const auto projected_start = projected(frame, start), projected_muzzle = projected(frame, muzzle);
      const auto projected_end = projected(frame, end), projected_target = projected(frame, target);
      require_near(projected_start.x, projected_muzzle.x, .0001F, "projected ray starts on rendered gun X");
      require_near(projected_start.y, projected_muzzle.y, .0001F, "projected ray starts on rendered gun Y");
      require_near(projected_end.x, projected_target.x, .0001F, "projected ray ends on actor X");
      require_near(projected_end.y, projected_target.y, .0001F, "projected ray ends on actor Y");
    } else if (std::abs(instance.tint[1] - .28F) < .001F) {
      ++reticle_ticks;
      reticle_x += instance.position.x;
      reticle_z += instance.position.z;
    }
  }
  require(reticle_ticks == 4, "committed target has four clear ground reticle ticks");
  require_near(reticle_x / 4, ship.position.x, .0001F, "reticle remains at locked target X");
  require_near(reticle_z / 4, ship.position.z, .0001F, "reticle remains at locked target Z");
  const auto& missile = mesh(frame, "missile");
  projectile = missile.position.x == 1 && std::abs(missile.position.z + 5.7F) < .0001F;
  require(warning && laser && projectile, "actual scene exposes warning, laser and projectile geometry");
}
void portrait_extremes_keep_player_footprint_visible() {
  for (const auto aspect : {320.F / 568.F, 375.F / 667.F, phone_aspect, 430.F / 932.F})
    for (const float station : {0.F, 100000.F})
      for (const float x : {-hr::gameplay::tuning::field_half_width, hr::gameplay::tuning::field_half_width})
        for (const float forward : {-6.F, 6.F}) {
          hr::gameplay::Snapshot run;
          run.runner_distance = station;
          run.player = {x, station + forward};
          const auto frame = hr::product::scene(run, aspect, true);
          const auto ship = mesh(frame, "ufo-modular");
          // The existing 2.6m saucer at 1.12 scale occupies approximately 2.92m.
          for (const float side : {-1.46F, 1.46F})
            for (const float along : {-1.46F, 1.46F}) {
              auto edge = ship.position;
              edge.x += side;
              edge.z += along;
              const auto screen = projected(frame, edge);
              require(std::abs(screen.x) < .99F && std::abs(screen.y) < .95F,
                  "portrait movement extremes retain the whole saucer footprint in frame");
            }
        }
}
} // namespace
int main() {
  screen_directions_follow_semantic_controls();
  actor_and_effect_geometry_share_one_forward_axis(true);
  actor_and_effect_geometry_share_one_forward_axis(false);
  portrait_extremes_keep_player_footprint_visible();
  std::cout << "Product input/projection, shared effect geometry and portrait extremes verified\n";
}
