#include "test_support.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <human_resources/product/controls.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/visibility.h>
#include <numbers>
namespace {
namespace hr = human_resources;
using test_support::require;
using test_support::require_near;
const hr::presentation::StyleInstance& instance(const hr::presentation::StyleFrame& f, std::string_view mesh) {
  const auto found = std::find_if(f.instances.begin(), f.instances.end(), [&](const auto& i) { return i.mesh == mesh; });
  require(found != f.instances.end(), "expected scene instance exists");
  return *found;
}
float screen_y(const hr::presentation::StyleFrame& frame, hr::presentation::Vec3 point) {
  const std::array<float, 4> p{point.x, point.y, point.z, 1};
  float clip_y = 0, clip_w = 0;
  for (std::size_t k = 0; k < 4; ++k) {
    clip_y += frame.camera[4 + k] * p[k];
    clip_w += frame.camera[12 + k] * p[k];
  }
  require(std::isfinite(clip_y) && clip_w > 0, "finite front-facing projection");
  return (1 - clip_y / clip_w) * .5F;
}
void player_keeps_prior_apparent_size_while_world_shrinks() {
  for (float aspect : {320.F / 568.F, 390.F / 844.F}) {
    hr::gameplay::Snapshot run;
    run.entities.push_back({.id = 45, .kind = hr::gameplay::EntityKind::cow, .position = {2, 4}});
    const auto frame = hr::product::scene(run, aspect, true);
    auto previous = frame;
    hr::presentation::set_overhead_camera(previous, aspect, 17, {}, 27);
    const auto width = [&](const auto& f, const auto& i) {
      const auto bounds = hr::product::visibility_mesh_bounds(i.mesh);
      require(bounds.has_value(), "actual source bounds exist for size comparison");
      const auto region = hr::product::project_region(hr::presentation::multiply(f.camera, hr::presentation::model_matrix(i)), *bounds);
      return region.right - region.left;
    };
    auto old_ship = instance(frame, "ufo-modular");
    old_ship.scale = {1.12F, 1.12F, 1.12F};
    require_near(width(frame, instance(frame, "ufo-modular")), width(previous, old_ship), 1e-5F, "player retains exact previous apparent source width");
    require_near(width(frame, instance(frame, "cow")) / width(previous, instance(frame, "cow")), .75F, 1e-5F, "world characters retain requested three-quarter apparent width");
    const auto title = hr::product::scene(run, aspect, true, true);
    require_near(instance(title, "ufo-modular").scale.x, 1.7F / .75F, 1e-5F, "title ship preserves its independent prior visual role");
  }
}
void semantic_up_projects_up(float aspect) {
  hr::gameplay::Game game;
  game.start({.seed = 417});
  for (unsigned step = 0; step < 5; ++step)
    game.advance(.1F);
  const auto before = hr::product::scene(game.snapshot(), aspect, true);
  const float before_y = screen_y(before, instance(before, "ufo-modular").position);
  hr::product::Controls controls;
  hr::input::Action up;
  up.id = static_cast<hr::input::ActionId>(hr::product::ControlAction::up);
  up.device = 1;
  up.source_control = 26;
  up.phase = hr::input::ActionPhase::started;
  up.value = 1;
  controls.apply(up, true, false);
  for (unsigned step = 0; step < 5; ++step)
    game.advance(.1F, controls.sample());
  const auto after = hr::product::scene(game.snapshot(), aspect, true);
  require(screen_y(after, instance(after, "ufo-modular").position) < before_y - .005F, "semantic up moves the actual saucer screen UP after runner scrolling");
}
void ahead_and_targeting_share_projection(float aspect) {
  hr::gameplay::Snapshot run;
  run.runner_distance = 100;
  run.player = {1, 100};
  run.entities.push_back({.id = 7001, .kind = hr::gameplay::EntityKind::cow, .position = {2, 110}, .velocity = {0, 1}, .warning_aim = {4, 114}, .warning_seconds = 1});
  run.beams.push_back({.target_id = 7001, .target = {2, 110}, .progress = .3F});
  run.shots.push_back({.from = {1, 100}, .to = {2, 110}, .remaining_seconds = .1F});
  run.projectiles.push_back({.id = 7002, .position = {3, 112}, .velocity = {0, -1}, .radius = .2F});
  const auto f = hr::product::scene(run, aspect, true);
  const auto& ship = instance(f, "ufo-modular");
  const auto& cow = instance(f, "cow");
  require_near(ship.position.z, 2.3F, 1e-5F, "ship is relative to runner station");
  require_near(cow.position.z, -7.7F, 1e-5F, "ahead actor agrees with scenery distance-along sign");
  require(screen_y(f, cow.position) < screen_y(f, ship.position), "spawn ahead projects ABOVE the saucer");
  require_near(std::cos(cow.rotation.y), -1, 1e-5F, "forward actor faces render -Z");
  require_near(instance(f, "beam").position.z, -7.7F, 1e-5F, "beam base shares authoritative target mapping");
  bool warning = false, shot = false, projectile = false;
  for (const auto& i : f.instances)
    if (i.mesh == "cube") {
      if (i.tint[0] == 1.F && i.tint[1] == .33F) {
        require(i.position.z <= -7.7F && i.position.z >= -11.7F, "warning dashes stay between committed endpoints");
        warning = true;
      }
      if (i.tint[0] == .55F) {
        const auto m = hr::presentation::model_matrix(i);
        const float end_z = m[11] + m[10];
        require_near(end_z, -7.7F, 1e-4F, "physical-muzzle shot ends at authoritative target mapping");
        shot = true;
      }
    }
  const auto& missile = instance(f, "missile");
  require_near(missile.position.z, -9.7F, 1e-5F, "missile shares authoritative projectile mapping");
  projectile = true;
  require(warning && shot && projectile, "all target presentations exercised");
  // A global gameplay-station offset must disappear at the presentation seam.
  run.runner_distance += 1000;
  run.player.z += 1000;
  for (auto& e : run.entities) {
    e.position.z += 1000;
    e.warning_aim.z += 1000;
  }
  for (auto& b : run.beams)
    b.target.z += 1000;
  for (auto& s : run.shots) {
    s.from.z += 1000;
    s.to.z += 1000;
  }
  for (auto& p : run.projectiles)
    p.position.z += 1000;
  const auto translated = hr::product::scene(run, aspect, true);
  require_near(instance(translated, "cow").position.z, cow.position.z, 1e-4F, "global station translation preserves actor placement");
  require_near(instance(translated, "ufo-modular").position.z, ship.position.z, 1e-4F, "global station translation preserves ship placement");
}
void infantry_source_and_launch_contract(float aspect) {
  using Role = hr::gameplay::InfantryRole;
  const std::array roles{Role::rifleman, Role::shotgunner, Role::rocketeer};
  const std::array bodies{"infantry-rifleman-body", "infantry-shotgunner-body", "infantry-rocketeer-body"};
  const std::array weapons{"weapon-carbine", "weapon-shotgun", "weapon-rocket-launcher"};
  for (std::size_t k = 0; k < roles.size(); ++k) {
    hr::gameplay::Snapshot run;
    run.stats.active_seconds = 10;
    run.entities.push_back({.id=8001, .kind=hr::gameplay::EntityKind::soldier,
        .position={1,5}, .warning_aim={0,0}, .warning_seconds=.5F, .infantry_role=roles[k]});
    const auto warning = hr::product::scene(run, aspect, true);
    const auto& body = instance(warning, bodies[k]);
    const auto& weapon = instance(warning, weapons[k]);
    require(body.stable_id==8001 && body.animation.warning && body.animation.enabled,
        "typed role selects the equipped authored body");
    require(weapon.attachment_parent_id==8001 && weapon.attachment_socket==hr::presentation::AttachmentSocket::weapon,
        "exact source weapon uses its named socket, preserving head attachments");
    const auto rings=std::count_if(warning.instances.begin(),warning.instances.end(),[](const auto& part){
      return part.mesh=="ring" && part.tint[0]==1.F && part.tint[1]==.28F;
    });
    require(rings==static_cast<int>(hr::gameplay::infantry_attack(roles[k]).pellets),
        "committed warning exposes each authoritative spread lane");
    auto& actor=run.entities.front();
    actor.warning_seconds=0; actor.fire_recovery_seconds=.55F;
    run.presentation_events[0]={.sequence=21, .active_seconds=9.95,
        .kind=hr::gameplay::PresentationEventKind::enemy_shot, .actor_id=8001};
    const auto shot=hr::gameplay::infantry_shot(roles[k],actor.position,actor.warning_aim);
    run.projectiles.push_back({.id=8002,.position=shot.origin,.velocity=shot.direction,.radius=.12F,
        .source_id=8001,.infantry_role=roles[k],.launch_position=shot.origin,
        .launch_height=shot.height,.rise_per_second=.5F,
        .kind=roles[k]==Role::rocketeer ? hr::gameplay::EnemyProjectileKind::infantry_rocket : hr::gameplay::EnemyProjectileKind::infantry_round,
        .launch_direction=shot.direction});
    const auto firing=hr::product::scene(run,aspect,true);
    require(instance(firing,bodies[k]).animation.fire_sequence==21 && instance(firing,bodies[k]).animation.recovering,
        "only a typed emitted-shot outcome starts the authored reaction");
    unsigned parts=0,flashes=0;
    for(const auto& part:firing.instances) {
      if(part.launch_id==8002) {
        ++parts;
        require(part.launch_parent_id==8001 && part.launch_bridge==1 && part.launch_elevation==1,
            "body and trails share bounded firing-time launch identity and initial correction");
        require_near(part.launch_origin.y,shot.height,1e-5F,"launch retains source-derived muzzle height");
      }
      if(part.attachment_parent_id==8001 && part.effect) ++flashes;
    }
    require(parts==4 && flashes==(roles[k]==Role::shotgunner ? 2U : 1U),
        "projectile and three optical trail layers share one launch; flash uses actual muzzle count");
    run.presentation_events={};
    run.projectiles.front().age_seconds=.5F;
    const auto restored=hr::product::scene(run,aspect,true);
    require(instance(restored,bodies[k]).animation.fire_sequence==0,
        "cold continuation never infers a new fire event from recovery or projectile state");
    for(const auto& part:restored.instances) {
      require(!(part.attachment_parent_id==8001 && part.effect),"cold continuation does not replay a muzzle flash");
      if(part.launch_id==8002) require(part.launch_bridge==0,"already-aged projectile never replays its short launch bridge");
    }
  }
}
void small_phone_threat_cues_keep_weight_without_widening_projectiles() {
  for(const auto phone:std::array<hr::ui::Rect,2>{{{0,0,320,568},{0,0,390,844}}}) {
    hr::gameplay::Snapshot run;run.phase=hr::gameplay::Phase::active;run.stats.active_seconds=10;
    run.entities.push_back({.id=91,.kind=hr::gameplay::EntityKind::soldier,.position={1,6},.warning_aim={0,0},.warning_seconds=.5F,.infantry_role=hr::gameplay::InfantryRole::rifleman});
    const auto before=run;
    const auto warning=hr::product::scene(run,phone.width/phone.height,true);
    unsigned rails=0,underlays=0;
    for(const auto& p:warning.instances)if(p.mesh=="cube") {
      if(p.tint[0]==1 && p.tint[1]==.33F) {
        ++rails;
        require(p.scale.x*std::abs(warning.camera[0])*phone.width>=1.99F,"committed coral warning remains at least two logical pixels at320phone width");
      }
      if(p.tint[0]==.025F && p.tint[1]==.07F)++underlays;
    }
    require(rails>0 && rails==underlays,"each committed warning dash has one bounded contrast underlay");
    require(run.player.x==before.player.x && run.entities[0].warning_aim.x==before.entities[0].warning_aim.x && run.entities[0].warning_seconds==before.entities[0].warning_seconds,"cue readability never changes player/committedaim/warning timers");
    run.entities[0].warning_seconds=0;
    const auto shot=hr::gameplay::infantry_shot(hr::gameplay::InfantryRole::rifleman,{1,6},{0,0});
    run.projectiles.push_back({.id=92,.position=shot.origin,.velocity=shot.direction,.radius=.12F,.source_id=91,.infantry_role=hr::gameplay::InfantryRole::rifleman,.launch_position=shot.origin,.launch_height=shot.height,.rise_per_second=.5F,.kind=hr::gameplay::EnemyProjectileKind::infantry_round,.launch_direction=shot.direction});
    const auto firing=hr::product::scene(run,phone.width/phone.height,true);
    unsigned bodies=0,cores=0,halos=0;
    for(const auto& p:firing.instances)if(p.launch_id==92) {
      if(p.tint[1]==.50F) {++bodies;require(p.scale.x==.045F && p.scale.y==.045F,"physical rifle projectile width remains exact; only optical trail carries additional legibility");}
      if(p.tint[1]==.94F) {++cores;require(p.scale.x*std::abs(firing.camera[0])*phone.width>=.99F,"optical tracer core remains at least one logical pixel on smallest phone");}
      if(p.tint[0]==.025F && p.tint[1]==.07F)++halos;
    }
    require(bodies==1 && cores==1 && halos==1,"one unchanged projectile has one compact layered optical trail");
  }
}

void boss_warning_exposes_every_committed_fan_lane() {
  const auto pattern = hr::gameplay::cannon_geometry();
  require(pattern.boss_projectiles == 3 && pattern.boss_spread_radians == .22F,
      "shared cannon contract preserves the original three-shot22hundredths-radian fan");
  for (bool reduced : {false, true})
    for (unsigned heading = 0; heading < 12; ++heading) {
      hr::gameplay::Snapshot run;
      run.stats.active_seconds = 154;
      run.player = {-2, 0};
      const float yaw = static_cast<float>(heading) * 2 * std::numbers::pi_v<float> / 12;
      run.entities.push_back({.id = 72, .kind = hr::gameplay::EntityKind::boss, .position = {1, 12}, .velocity = {std::sin(yaw), -std::cos(yaw)}, .warning_aim = {-2, 0}, .warning_seconds = .5F});
      const auto shot = hr::gameplay::vehicle_cannon_shot(run.entities.front(), run.entities.front().warning_aim);
      require(shot.valid, "rotated boss retains a valid actual source opening");
      const float range = std::hypot(-2 - shot.origin.x, -shot.origin.z);
      for (bool dodged : {false, true}) {
        if (dodged)
          run.player = {4, -2};
        const auto frame = hr::product::scene(run, 390.F / 844.F, reduced);
        std::vector<hr::presentation::Vec3> markers;
        unsigned origins = 0;
        for (const auto& part : frame.instances) {
          if (part.mesh == "ring" && part.tint[0] == 1 && part.tint[1] == .28F)
            ++origins;
          if (part.mesh == "cube" && part.tint[0] == 1 && part.tint[1] == .28F)
            markers.push_back(part.position);
        }
        require(origins == 3 && markers.size() == 12, "all three actual boss lanes have visible origin and four-point locked markers");
        for (unsigned lane = 0; lane < pattern.boss_projectiles; ++lane) {
          const float angle = (static_cast<float>(lane) - 1) * pattern.boss_spread_radians;
          const hr::gameplay::Vec2 direction{shot.direction.x * std::cos(angle) - shot.direction.z * std::sin(angle),
              shot.direction.x * std::sin(angle) + shot.direction.z * std::cos(angle)};
          hr::presentation::Vec3 center{};
          for (unsigned j = 0; j < 4; ++j) {
            center.x += markers[lane * 4 + j].x * .25F;
            center.z += markers[lane * 4 + j].z * .25F;
          }
          require_near(center.x, shot.origin.x + direction.x * range, 1e-5F, "committed fan marker matches exact authoritative projectile direction X after dodge");
          require_near(center.z, 2.3F - shot.origin.z - direction.z * range, 1e-5F, "committed fan marker matches exact authoritative projectile direction Z after dodge");
        }
      }
    }
}
} // namespace
int main() {
  player_keeps_prior_apparent_size_while_world_shrinks();
  small_phone_threat_cues_keep_weight_without_widening_projectiles();
  boss_warning_exposes_every_committed_fan_lane();
  for (float aspect : {9.F / 16.F, 16.F / 9.F}) {
    semantic_up_projects_up(aspect);
    ahead_and_targeting_share_projection(aspect);
    infantry_source_and_launch_contract(aspect);
  }
}
