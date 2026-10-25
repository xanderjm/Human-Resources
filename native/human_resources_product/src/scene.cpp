#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/product/gear_presentation.h>
#include <human_resources/product/combat_feedback.h>
#include "infantry_equipment.generated.h"
#include "vehicle_assembly.h"
#include "vehicle_projectile.h"
#include <human_resources/gameplay/tuning.h>
#include <human_resources/product/visibility.h>

#include <algorithm>
#include <cmath>
#include <human_resources/presentation/scenery.h>
#include <numbers>
namespace human_resources::product {
namespace {
using presentation::StyleFrame;
using presentation::Vec3;
const InfantryEquipment* equipment(gameplay::InfantryRole role) {
  return role >= gameplay::InfantryRole::rifleman && role <= gameplay::InfantryRole::rocketeer ? &infantry_equipment[static_cast<std::size_t>(role) - 1] : nullptr;
}
const gameplay::PresentationEvent* last_fire(const gameplay::Snapshot& run, std::uint32_t id) {
  const gameplay::PresentationEvent* latest = nullptr;
  for (const auto& event : run.presentation_events)
    if (event.kind == gameplay::PresentationEventKind::enemy_shot && event.actor_id == id &&
        (!latest || event.sequence > latest->sequence)) latest = &event;
  return latest;
}
void add(StyleFrame& f, std::string_view mesh, Vec3 p, Vec3 scale = {1, 1, 1}, Vec3 rotation = {}, std::array<float, 4> tint = {1, 1, 1, 1}, bool effect = false) {
  f.instances.push_back({.mesh = mesh, .position = p, .scale = scale, .rotation = rotation, .tint = tint, .effect = effect, .casts_shadow = !effect});
}
void line(StyleFrame& f, Vec3 a, Vec3 b, float thickness, std::array<float, 4> color) {
  const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
  const float horizontal = std::hypot(dx, dz), length = std::hypot(horizontal, dy);
  if (length < .001F)
    return;
  add(f, "cube", {(a.x + b.x) * .5F, (a.y + b.y) * .5F, (a.z + b.z) * .5F}, {thickness, thickness, length * .5F}, {-std::atan2(dy, horizontal), std::atan2(dx, dz), 0}, color, true);
}
void shot_footprint(StyleFrame& frame, Vec3 ground, float radius) {
  // This filled inscribed diamond is the abstract 2D hit footprint, not a
  // larger missile mesh. Its corners remain inside the collision circle.
  add(frame,"cube",{ground.x,.055F,ground.z},{radius*.64F,.003F,radius*.64F},
      {0,std::numbers::pi_v<float>*.25F,0},{1,.24F,.13F,.52F},true);
  add(frame,"ring",ground,{radius,1,radius},{},{1,.34F,.19F,.96F},true);
}
float smooth(float start, float end, float p) {
  const float u = std::clamp((p - start) / (end - start), 0.F, 1.F);
  return u * u * (3 - 2 * u);
}
Vec3 lerp(Vec3 a, Vec3 b, float u) {
  return {a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u};
}
void cone(StyleFrame& frame, Vec3 ground, Vec3 intake, float radius, float opacity) {
  const float dx = intake.x - ground.x, dy = intake.y - ground.y, dz = intake.z - ground.z;
  const float horizontal = std::hypot(dx, dz), length = std::hypot(horizontal, dy);
  if (length < .001F)
    return;
  // The existing cone's base is local Y=0; its narrow intake is local Y=1.
  // Ry*Rx maps +Y onto the exact ground-to-socket direction, including banking.
  add(frame, "beam", ground, {radius, length, radius}, {std::atan2(horizontal, dy), std::atan2(dx, dz), 0}, {.29F, .96F, .71F, opacity}, true);
}
void capture_guides(StyleFrame& frame, Vec3 ground, Vec3 intake, float radius, float active_seconds, bool reduced_motion) {
  const float dx = intake.x - ground.x, dz = intake.z - ground.z;
  const float length = std::hypot(dx, dz);
  const Vec3 side = length > .01F ? Vec3{dz / length, 0, -dx / length} : Vec3{1, 0, 0};
  const auto point = [&](float u, float sign) {
    auto p = lerp(ground, intake, u);
    const float width = radius * (1 - u) * (.7F + .3F * std::sin(u * std::numbers::pi_v<float>));
    p.x += side.x * width * sign;
    p.z += side.z * width * sign;
    return p;
  };
  // Two short dotted rails read as lift direction, without filling the field
  // with a particle cloud. At six beams this remains at most 84 simple quads.
  constexpr unsigned count = 7;
  const float phase = reduced_motion ? 0 : std::fmod(active_seconds * .7F, 1.F / static_cast<float>(count));
  for (float sign : {-1.F, 1.F})
    for (unsigned i = 0; i < count; ++i) {
      const float u = static_cast<float>(i) / static_cast<float>(count) + phase;
      const float v = std::min(.96F, u + .045F);
      if (u < .96F)
        line(frame, point(u, sign), point(v, sign), .032F, {.46F, 1, .75F, .90F});
    }
}
// A cue is information, so the wider portrait camera must not reduce it to a
// subpixel thread. Use the smallest supported logical viewport as the floor;
// wider phones preserve that weight without changing any collision geometry.
void hostile_trail(StyleFrame& frame, Vec3 tip, Vec3 rear, float pixel, float opacity = 1) {
  line(frame, tip, rear, std::max(.07F, pixel * 1.65F), {.025F, .07F, .11F, .84F * opacity});
  line(frame, tip, rear, std::max(.07F, pixel), {1, .24F, .13F, .88F * opacity});
  line(frame, tip, lerp(tip, rear, .55F), std::max(.025F, pixel * .5F), {1, .94F, .65F, opacity});
}
void warning(StyleFrame& frame, Vec3 origin, Vec3 aim, float active_seconds, bool reduced_motion, float pixel) {
  const float length = std::hypot(aim.x - origin.x, aim.z - origin.z);
  const unsigned count = std::min(40U, static_cast<unsigned>(length / .65F) + 1);
  for (unsigned i = 0; i < count; ++i) {
    const float u = static_cast<float>(i) / static_cast<float>(count);
    const float v = std::min(1.F, (static_cast<float>(i) + .54F) / static_cast<float>(count));
    const auto a = lerp(origin, aim, u), b = lerp(origin, aim, v);
    line(frame, a, b, std::max(.047F, pixel * 1.65F), {.025F, .07F, .11F, .84F});
    line(frame, a, b, std::max(.047F, pixel), {1, .33F, .19F, .96F});
  }
  const float pulse = reduced_motion ? .9F : .76F + .18F * std::sin(active_seconds * 9);
  add(frame, "ring", origin, {1.05F, 1, 1.05F}, {}, {1, .28F, .23F, pulse}, true);
  // The committed aim remains visible after the player dodges. Four open ticks
  // identify the locked point without implying an area-of-effect damage disc.
  for (float sign : {-1.F, 1.F}) {
    line(frame, {aim.x + sign * .42F, aim.y, aim.z}, {aim.x + sign * .16F, aim.y, aim.z}, .045F, {1, .28F, .23F, .95F});
    line(frame, {aim.x, aim.y, aim.z + sign * .42F}, {aim.x, aim.y, aim.z + sign * .16F}, .045F, {1, .28F, .23F, .95F});
  }
}
void beats(StyleFrame& frame, const SceneEffects* effects, float distance, Vec3 ship, bool reduced_motion) {
  if (!effects)
    return;
  for (const auto& beat : effects->beats()) {
    if (beat.kind == SceneBeatKind::capture)
      continue; // One shared intake response below, even with six simultaneous beams.
    if (beat.kind != SceneBeatKind::capture && beat.kind != SceneBeatKind::impact && beat.kind != SceneBeatKind::defeat && beat.kind != SceneBeatKind::hull_hit)
      continue;
    const float u = std::clamp(beat.age / .55F, 0.F, 1.F), fade = (1 - u) * (1 - u);
    const bool capture = beat.kind == SceneBeatKind::capture;
    const bool shield = beat.kind == SceneBeatKind::shield_hit;
    const bool hull = beat.kind == SceneBeatKind::hull_hit;
    const bool defeat = beat.kind == SceneBeatKind::defeat;
    const auto color = capture ? std::array<float, 4>{.44F, 1, .72F, fade} : shield ? std::array<float, 4>{.50F, .94F, 1, fade}
                                                                                    : std::array<float, 4>{1, .45F, .24F, fade};
    const Vec3 p = capture || shield || hull ? Vec3{ship.x, ship.y + .08F, ship.z} : Vec3{beat.position.x, defeat ? .35F : .8F, distance - beat.position.z + 2.3F};
    const float radius = capture ? 1.35F + .75F * u : shield || hull ? 1.45F + .3F * u
                                                                     : defeat ? .55F + 1.25F * u : .18F + .45F * u;
    add(frame, "ring", p, {radius, 1, radius}, {}, color, true);
    if (!reduced_motion && !capture && !shield && !hull)
      for (unsigned i = 0; i < (defeat ? 7U : 3U); ++i) {
        const float angle = static_cast<float>(i) * 2.39996F + static_cast<float>(beat.actor_id);
        const Vec3 end{p.x + std::cos(angle) * radius, p.y + (defeat ? 1.6F : .45F) * u, p.z + std::sin(angle) * radius};
        line(frame, lerp(p, end, .55F), end, defeat ? .085F : .025F, color);
      }
  }
}
} // namespace
std::vector<std::string_view> content_ids() {
  const auto scenery = presentation::scenery_materials();
  std::vector<std::string_view> ids(scenery.begin(), scenery.end());
  for (auto id : {"ufo-modular", "ufo-antenna", "ufo-beam", "ufo-laser", "ufo-shield", "ufo-rim-lens", "shield-shell", "cow", "sheep", "pig", "farmer-body", "straw-hat", "resident-body", "nightcap", "infantry-rifleman-body", "infantry-shotgunner-body", "infantry-rocketeer-body", "weapon-carbine", "weapon-shotgun", "weapon-rocket-launcher", "jeep-chassis", "jeep-wheel", "jeep-minigun-head", "jeep-minigun-fixed-post", "jeep-minigun-yaw-cap", "tank-hull", "tank-turret", "missile", "beam", "plane", "ui-atlas"})
    if (std::find(ids.begin(), ids.end(), id) == ids.end())
      ids.push_back(id);
  return ids;
}
presentation::StyleFrame scene(const gameplay::Snapshot& run, float aspect, bool reduced_motion, bool attract, SceneEffects* effects, presentation::SceneryTheme theme) {
  return scene(run, gameplay_framing(aspect, attract ? FramingContext::attract : FramingContext::flight), reduced_motion, effects, theme);
}
presentation::StyleFrame scene(const gameplay::Snapshot& run, const GameplayFraming& framing, bool reduced_motion, SceneEffects* effects, presentation::SceneryTheme theme) {
  const bool attract = framing.context == FramingContext::attract;
  const float aspect = framing.aspect;
  const float t = static_cast<float>(run.stats.active_seconds);
  const double distance = attract ? 0 : run.runner_distance;
  auto frame = presentation::generate_scenery({theme, 514, distance, aspect, 9.7F}).scene;
  // The authored rotor lies in XY with its spindle on Z. Derive scenery
  // motion from saved active time so pause/background/cold restore stay exact.
  const float windmill_phase = reduced_motion ? 0.F :
      static_cast<float>(std::fmod(run.stats.active_seconds * .45, 2 * std::numbers::pi_v<double>));
  for (auto& prop : frame.instances)
    if (prop.mesh == "windmill-rotor")
      prop.rotation.z = windmill_phase;
  const auto scenery_count = frame.instances.size();
  frame.show_overlay = false;
  frame.ring_angle = reduced_motion ? 0 : effects && !attract ? effects->rotor_angle()
                                                              : t * 1.8F;
  const auto damage = effects && !attract ? effects->damage_feedback() : DamageFeedback{};
  const float hit = damage.age >= 0 && damage.age < .24F ? 1 - damage.age / .24F : 0;
  frame.damage_flash = reduced_motion ? 0 : (damage.hull ? 1.F : -1.F) * hit * hit;
  const float recoil = reduced_motion || damage.age < 0 ? 0 : (1 - std::exp(-damage.age * 90)) * std::exp(-damage.age * 14) * (damage.hull ? 1.F : .4F);
  // A world-space camera follows forward travel; lateral movement remains visible
  // across the entire lane, so the player can judge the danger around the saucer.
  frame.camera = framing.camera;
  frame.world_destination = framing.destination;
  const float threat_unit = framing.logical_pixel;
  const auto velocity = effects && !attract ? effects->flight_velocity() : run.velocity;
  const float response = effects && !attract ? effects->capture_response() : 0;
  const auto dash = effects && !attract ? effects->dash_response() : gameplay::Vec2{};
  const Vec3 ship{attract ? 0.F : run.player.x,
      2.5F + (reduced_motion ? 0 : .055F * std::sin(t * 2.5F) - .12F * response - .13F * recoil),
      attract ? 2.3F : run.runner_distance - run.player.z + 2.3F};
  const float bank = reduced_motion ? 0 : std::clamp(-velocity.x * .035F - dash.x * .15F + .12F * recoil, -.38F, .38F);
  const float pitch = reduced_motion ? 0 : std::clamp(-velocity.z * .022F - dash.z * .13F + .07F * recoil, -.26F, .26F);
  const float ship_scale = (attract ? 1.7F : 1.12F) / .75F;
  add(frame, "ufo-modular", ship, {ship_scale, ship_scale, ship_scale}, {pitch, 0, bank});
  frame.instances.back().polished_hull = true;
  frame.instances.back().damage_reactive = true;
  const ShipPose ship_pose{ship, ship_scale, bank, pitch};
  append_ship_gear(frame, run, ship_pose, reduced_motion);
  const Vec3 intake = ship_socket(ship_pose, {0, .04F, 0});
  if (run.invulnerability_seconds > 0)
    add(frame, "ring", {ship.x, .06F, ship.z}, {1.6F, 1, 1.6F}, {}, {.5F, .95F, 1, .7F}, true);
  // Gameplay +Z is semantic forward. This camera sees render -Z as screen up,
  // matching scenery's distance-along mapping; all targeting shares this seam.
  auto point = [&](gameplay::Vec2 p, float y = 0) { return Vec3{p.x, y, static_cast<float>(distance) - p.z + 2.3F}; };
  if (effects && !attract) effects->begin_capture_frame(run);
  const auto completed = effects && !attract ? effects->completed_captures() : std::span<const gameplay::EntityView>{};
  for (std::size_t actor_index = 0; actor_index < run.entities.size() + completed.size(); ++actor_index) {
    const bool finishing = actor_index >= run.entities.size();
    const auto& e = finishing ? completed[actor_index - run.entities.size()] : run.entities[actor_index];
    auto p = point(e.position);
    if (std::abs(p.z) > 32)
      continue;
    const float progress = std::clamp(e.kind <= gameplay::EntityKind::soldier ? (effects && !attract ? effects->capture_progress(e) : SceneEffects::capture_lift_progress(e.capture_progress)) : e.capture_progress, 0.F, 1.F);
    const float lift = smooth(.08F, 1.F, progress);
    const bool organic = e.kind <= gameplay::EntityKind::soldier;
    // Keep the first lift readable, then sweep into the real fixed-hull intake.
    // Authored lift/struggle clips and head attachments remain on the same root.
    const float travel = smooth(.16F, 1.F, progress);
    float size = 1 - .98F * smooth(.60F, 1.F, progress);
    if (organic && !reduced_motion)
      size *= 1 + .055F * std::sin(std::min(1.F, progress / .3F) * std::numbers::pi_v<float>);
    p = lerp(p, intake, travel);
    p.y += .28F * std::sin(progress * std::numbers::pi_v<float>);
    if (!reduced_motion) {
      const float sway = std::sin(progress * std::numbers::pi_v<float>) * .16F;
      p.x += sway * std::sin(t * 6 + static_cast<float>(e.id));
    }
    const float speed = std::hypot(e.velocity.x, e.velocity.z);
    const auto* gear = equipment(e.infantry_role);
    const auto shot = gameplay::infantry_shot(e.infantry_role, e.position, e.warning_aim);
    const auto attack = gameplay::infantry_attack(e.infantry_role);
    const bool planted = gear && (e.warning_seconds > 0 || e.fire_recovery_seconds > 0);
    float yaw = !organic ? gameplay::vehicle_body_yaw(e) : speed > .08F ? std::atan2(e.velocity.x, -e.velocity.z) : static_cast<float>(e.id) * 1.7F;
    if (planted && shot.valid) yaw = std::atan2(shot.direction.x, -shot.direction.z) - attack.weapon_yaw_radians;
    Vec3 rotation{reduced_motion ? 0 : lift * (organic ? .92F : .32F),
        yaw + (reduced_motion || !organic ? 0 : lift * .65F),
        reduced_motion ? 0 : std::sin(t * 6 + static_cast<float>(e.id)) * lift * (organic ? .44F : .16F)};
    if (finishing) {
      p = intake;
      size = .02F;
    }
    if (effects && !attract) {
      const auto pose = effects->capture_pose(e, {p, rotation, size}, {point(e.position), {0, yaw, 0}, 1},
          run.stats.active_seconds, static_cast<float>(distance),
          reduced_motion ? CaptureMotion::reduced : CaptureMotion::animated);
      p = pose.position;
      rotation = pose.rotation;
      size = pose.scale;
      yaw = rotation.y;
    }
    if (finishing) {
      const Vec3 specimen{p.x, p.y + size * .3F, p.z};
      cone(frame, specimen, intake, size * .6F, .12F);
      capture_guides(frame, specimen, intake, size * .6F, t, reduced_motion);
    }
    const auto body_index = frame.instances.size();
    if (e.kind == gameplay::EntityKind::cow || e.kind == gameplay::EntityKind::sheep || e.kind == gameplay::EntityKind::pig) {
      const auto mesh = e.kind == gameplay::EntityKind::cow ? "cow" : e.kind == gameplay::EntityKind::sheep ? "sheep"
                                                                                                            : "pig";
      add(frame, mesh, p, {size, size, size}, rotation);
    } else if (e.kind == gameplay::EntityKind::farmer || e.kind == gameplay::EntityKind::soldier) {
      const bool resident = e.kind == gameplay::EntityKind::farmer && theme == presentation::SceneryTheme::suburbia;
      add(frame, e.kind == gameplay::EntityKind::farmer ? (resident ? "resident-body" : "farmer-body") : gear ? gear->body : "infantry-rifleman-body", p, {size, size, size}, rotation);
      if (e.kind == gameplay::EntityKind::farmer) {
        add(frame, resident ? "nightcap" : "straw-hat", {});
        auto& hat = frame.instances.back();
        hat.attachment_parent_id = e.id;
        hat.attachment_local = presentation::model_matrix({.position = {0, 1.30F, 0}, .rotation = {0, std::numbers::pi_v<float>, 0}});
      }
      if (gear && e.kind == gameplay::EntityKind::soldier) {
        add(frame, gear->weapon, {});
        auto& gun = frame.instances.back();
        gun.attachment_parent_id = e.id;
        gun.attachment_socket = presentation::AttachmentSocket::weapon;
      }
    } else if (e.kind == gameplay::EntityKind::jeep) {
      float fire_age=-1;
      if(const auto* event=last_fire(run,e.id)) fire_age=static_cast<float>(run.stats.active_seconds-event->active_seconds);
      const bool committed=e.warning_seconds>0 || (fire_age>=0 && fire_age<.12F);
      detail::append_jeep(frame,{.position=p,.scale={size,size,size},.rotation=rotation},e,
          committed ? e.warning_aim : run.player,reduced_motion || !effects ? 0 : effects->wheel_angle(e.id),
          e.capture_progress==0 && e.fire_recovery_seconds==0 ? fire_age : -1);
    } else {
      const float boss=e.kind==gameplay::EntityKind::boss ? 1.6F : 1.F;
      float fire_age=-1;
      if(const auto* event=last_fire(run,e.id)) fire_age=static_cast<float>(run.stats.active_seconds-event->active_seconds);
      const bool committed=e.warning_seconds>0 || (fire_age>=0 && fire_age<.12F);
      detail::append_tank(frame,{.position=p,.scale={size*boss,size*boss,size*boss},.rotation=rotation},
          e,committed ? e.warning_aim : run.player,
          e.capture_progress==0 && e.fire_recovery_seconds==0 ? fire_age : -1);
    }
    if (e.kind >= gameplay::EntityKind::soldier) {
      const float hit_strength = enemy_hit_strength(run, e.id, reduced_motion);
      for (std::size_t i = body_index; i < frame.instances.size(); ++i)
        if (!frame.instances[i].effect) {
          // Brief warm enamel/cloth response, local to the actor actually hit.
          // No displacement: source muzzles and physical rays stay coherent.
          frame.instances[i].tint[0] *= 1 + .8F * hit_strength;
          frame.instances[i].tint[1] *= 1 + .45F * hit_strength;
          frame.instances[i].tint[2] *= 1 + .12F * hit_strength;
        }
    }
    if (organic) {
      auto& body = frame.instances[body_index];
      body.stable_id = e.id;
      body.animation = {.speed = speed, .warning = e.warning_seconds > 0, .capturing = e.capture_progress > 0, .enabled = true};
      body.animation.product_capture_transport = true;
      body.animation.capture_progress = progress;
      body.animation.fleeing = e.kind < gameplay::EntityKind::soldier && !finishing && progress == 0 && speed > .04F;
      if (gear) {
        const auto aim = point(e.warning_aim, 2.5F);
        const float dx = aim.x - p.x, dz = aim.z - p.z;
        body.animation.aim_target = {std::cos(yaw)*dx-std::sin(yaw)*dz, aim.y-p.y, std::sin(yaw)*dx+std::cos(yaw)*dz};
        body.animation.recovering=e.fire_recovery_seconds>0;
        body.animation.fire_age_seconds=std::max(0.F,attack.recovery_seconds-e.fire_recovery_seconds);
        if (const auto* fire = last_fire(run,e.id)) {
          body.animation.fire_sequence = fire->sequence;
          body.animation.fire_age_seconds = static_cast<float>(run.stats.active_seconds-fire->active_seconds);
          const float age = body.animation.fire_age_seconds;
          if (age >= 0 && age < .09F && progress == 0) {
            for (unsigned muzzle = 0; muzzle < (gear->double_muzzle ? 2U : 1U); ++muzzle) {
              const auto& raw = muzzle == 0 ? gear->muzzle : gear->secondary_muzzle;
              add(frame,"cube",{},{1,1,1},{},{1,.66F,.22F,1-age/.09F},true);
              auto& flash=frame.instances.back();
              flash.attachment_parent_id=e.id;
              flash.attachment_socket=presentation::AttachmentSocket::weapon;
              flash.attachment_local=presentation::model_matrix({.position={raw[0],raw[1],raw[2]+.08F},.scale={.045F,.045F,.13F}});
            }
          }
        }
      }
    }
    if (e.warning_seconds > 0) {
      if (gear && shot.valid) {
        const float range=std::hypot(e.warning_aim.x-shot.origin.x,e.warning_aim.z-shot.origin.z);
        for (unsigned i=0;i<attack.pellets;++i) {
          const float angle=(static_cast<float>(i)-static_cast<float>(attack.pellets-1)*.5F)*attack.spread_radians;
          const gameplay::Vec2 ray{shot.direction.x*std::cos(angle)-shot.direction.z*std::sin(angle),shot.direction.x*std::sin(angle)+shot.direction.z*std::cos(angle)};
          warning(frame,point(shot.origin,.08F),point({shot.origin.x+ray.x*range,shot.origin.z+ray.z*range},.08F),t,reduced_motion,threat_unit);
        }
      } else if (e.kind == gameplay::EntityKind::jeep) {
        const auto muzzle=gameplay::jeep_minigun_shot(e,e.warning_aim,e.minigun_barrel);
        if(muzzle.valid) warning(frame,point(muzzle.origin,.08F),point(e.warning_aim,.08F),t,reduced_motion,threat_unit);
      } else if(e.kind==gameplay::EntityKind::tank || e.kind==gameplay::EntityKind::boss) {
        const auto muzzle=gameplay::vehicle_cannon_shot(e,e.warning_aim);
        if (muzzle.valid) {
          const auto pattern = gameplay::cannon_geometry();
          const unsigned count = e.kind == gameplay::EntityKind::boss ? pattern.boss_projectiles : 1U;
          const float range = std::hypot(e.warning_aim.x - muzzle.origin.x, e.warning_aim.z - muzzle.origin.z);
          for (unsigned i = 0; i < count; ++i) {
            const float angle = (static_cast<float>(i) - static_cast<float>(count - 1) * .5F) * pattern.boss_spread_radians;
            const gameplay::Vec2 direction{muzzle.direction.x * std::cos(angle) - muzzle.direction.z * std::sin(angle),
                muzzle.direction.x * std::sin(angle) + muzzle.direction.z * std::cos(angle)};
            warning(frame, point(muzzle.origin, .08F), point({muzzle.origin.x + direction.x * range, muzzle.origin.z + direction.z * range}, .08F), t, reduced_motion, threat_unit);
          }
        }
      } else if (!gear)
        warning(frame, point(e.position, .08F), point(e.warning_aim, .08F), t, reduced_motion, threat_unit);
    }
  }
  for (const auto& beam : run.beams) {
    const auto target = point(beam.target, .025F);
    const bool overdrive = run.harvest_overdrive_seconds > 0;
    cone(frame, target, intake, beam.vehicle ? 1.15F : .72F, (overdrive ? .19F : .12F) + .07F * beam.progress);
    capture_guides(frame, target, intake, beam.vehicle ? 1.15F : .72F, t * (overdrive ? 1.7F : 1.F), reduced_motion);
    add(frame, "ring", target, {.85F, 1, .85F}, {}, {.5F, 1, .76F, .82F}, true);
    const float acquisition = .40F + .30F * (1 - beam.progress);
    add(frame, "ring", intake, {acquisition, 1, acquisition}, {}, {.48F, 1, .76F, .7F}, true);
  }
  append_gear_shots(frame, run, ship_pose, static_cast<float>(distance), reduced_motion);
  for (const auto& projectile : run.projectiles) {
    const auto* gear=equipment(projectile.infantry_role);
    const bool minigun=projectile.kind==gameplay::EnemyProjectileKind::jeep_minigun;
    const float height=(gear || minigun) ? std::min(2.5F,projectile.launch_height+projectile.rise_per_second*projectile.age_seconds) : 2.2F;
    auto p = point(projectile.position, height);
    if(projectile.kind==gameplay::EnemyProjectileKind::vehicle_missile ||
        projectile.kind==gameplay::EnemyProjectileKind::boss_missile) {
      const auto missile=detail::vehicle_missile_pose(projectile);
      p=point(missile.position,missile.height);
      const float horizontal=std::hypot(missile.velocity.x,missile.velocity.z);
      const float speed=std::hypot(horizontal,missile.vertical_speed);
      const float scale=gameplay::cannon_geometry().visible_missile_scale*
          (projectile.kind==gameplay::EnemyProjectileKind::boss_missile ? 1.6F : 1.F);
      add(frame,"missile",p,{scale,scale,scale},
          {-std::atan2(missile.vertical_speed,horizontal),std::atan2(missile.velocity.x,-missile.velocity.z),0});
      if(speed>0) {
        const float trail=std::min(.65F,projectile.age_seconds*speed);
        const Vec3 rear{p.x-missile.velocity.x/speed*trail,p.y-missile.vertical_speed/speed*trail,
            p.z+missile.velocity.z/speed*trail};
        const float glow=smooth(.04F,.12F,projectile.age_seconds);
        // Optical trail uses the already accepted infantry widths. The finite
        // source-fitted missile above is unchanged; glow is not bore geometry.
        line(frame,p,rear,.07F,{1,.24F,.13F,.88F*glow});
        line(frame,p,lerp(p,rear,.55F),.025F,{1,.94F,.65F,glow});
      }
      // The ring communicates the unchanged abstract 2D hit radius, independently
      // of the source-fitted finite missile and its short launch presentation.
      shot_footprint(frame,point(projectile.position,.05F),projectile.radius);
      continue;
    }
    if(minigun) {
      const Vec3 tip=p;
      const float speed=std::hypot(projectile.velocity.x,projectile.velocity.z);
      const float spatial_speed=std::hypot(speed,projectile.rise_per_second);
      const float half_length=std::min(.18F,projectile.age_seconds*spatial_speed*.5F);
      if(spatial_speed>0) {
        p.x-=projectile.velocity.x/spatial_speed*half_length;
        p.y-=projectile.rise_per_second/spatial_speed*half_length;
        p.z+=projectile.velocity.z/spatial_speed*half_length;
      }
      const float width=gameplay::tuning::minigun_visible_tracer_radius;
      add(frame,"cube",p,{width,width,std::max(.001F,half_length)},
          {-std::atan2(projectile.rise_per_second,speed),std::atan2(projectile.velocity.x,-projectile.velocity.z),0},
          {1,.65F,.2F,1},true);
      if(spatial_speed>0) {
        const float trail=std::min(.65F,projectile.age_seconds*spatial_speed);
        const Vec3 rear{tip.x-projectile.velocity.x/spatial_speed*trail,tip.y-projectile.rise_per_second/spatial_speed*trail,
            tip.z+projectile.velocity.z/spatial_speed*trail};
        const float glow=smooth(.04F,.12F,projectile.age_seconds);
        hostile_trail(frame, tip, rear, threat_unit, glow);
      }
      shot_footprint(frame,point(projectile.position,.05F),projectile.radius);
      continue;
    }
    const float yaw = std::atan2(projectile.velocity.x, -projectile.velocity.z);
    const float radius = std::max(.12F, projectile.radius);
    const auto first=frame.instances.size();
    if (gear && projectile.infantry_role != gameplay::InfantryRole::rocketeer)
      add(frame,"cube",p,{.045F,.045F,.20F},{0,yaw,0},{1,.50F,.24F,1},true);
    else
      add(frame, "missile", p, {radius / .285F, radius / .265F, std::max(.75F, radius * 3)}, {0, yaw, 0});
    const Vec3 rear{p.x - std::sin(yaw) * .65F, p.y, p.z - std::cos(yaw) * .65F};
    hostile_trail(frame, p, rear, threat_unit);
    if (gear) {
      const Vec3 raw{(gear->muzzle[0]+gear->secondary_muzzle[0])*.5F,(gear->muzzle[1]+gear->secondary_muzzle[1])*.5F,(gear->muzzle[2]+gear->secondary_muzzle[2])*.5F};
      for (std::size_t i=first;i<frame.instances.size();++i) {
        auto& part=frame.instances[i];
        part.launch_id=projectile.id;
        part.launch_parent_id=projectile.source_id;
        part.launch_local=raw;
        part.launch_origin=point(projectile.launch_position,projectile.launch_height);
        part.launch_bridge=1-smooth(0,.12F,projectile.age_seconds);
        part.launch_elevation=std::clamp((2.5F-height)/std::max(.001F,2.5F-projectile.launch_height),0.F,1.F);
      }
    }
    add(frame, "ring", {p.x, .05F, p.z}, {radius, 1, radius}, {}, {1, .34F, .19F, .65F}, true);
  }
  if (!attract) {
    beats(frame, effects, static_cast<float>(distance), ship, reduced_motion);
    append_gear_effects(frame, run, effects, ship_pose, static_cast<float>(distance), reduced_motion);
  }
  if (attract) {
    add(frame, "cow", {0, .65F, 4.1F}, {1.2F, 1.2F, 1.2F}, {.15F, 1.2F, .25F});
    cone(frame, {0, .04F, 4.1F}, intake, 1.1F, .15F);
    capture_guides(frame, {0, .04F, 4.1F}, intake, 1.1F, t, true);
    add(frame, "ring", {0, .04F, 4.1F}, {1.2F, 1, 1.2F}, {}, {.5F, 1, .76F, .8F}, true);
    add(frame, "sheep", {4, 0, -2.5F});
    add(frame, theme == presentation::SceneryTheme::suburbia ? "resident-body" : "farmer-body", {-5, 0, -6});
    add(frame, theme == presentation::SceneryTheme::suburbia ? "nightcap" : "straw-hat", {-5, 1.42F, -6});
  }
  preserve_play_visibility(frame, scenery_count, attract ? nullptr : &run);
  return frame;
}
} // namespace human_resources::product
