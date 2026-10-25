#include <algorithm>
#include <array>
#include <cmath>
#include <human_resources/product/visibility.h>
#include <limits>
#include <product_mesh_bounds.generated.h>
namespace human_resources::product {
namespace {
using presentation::Vec3;
constexpr std::size_t region_capacity = 256;
constexpr float silhouette_padding = .025F, fade_padding = .06F, minimum_alpha = .12F;
float smooth(float x) {
  x = std::clamp(x, 0.F, 1.F);
  return x * x * (3 - 2 * x);
}
ProjectedRegion expanded(ProjectedRegion r, float padding) {
  r.left -= padding;
  r.right += padding;
  r.top -= padding;
  r.bottom += padding;
  return r;
}
bool on_screen(const ProjectedRegion& r) {
  return r.right >= -1 && r.left <= 1 && r.bottom >= -1 && r.top <= 1 && r.farthest >= 0 && r.nearest <= 1;
}
float covering(const ProjectedRegion& occluder, const ProjectedRegion& target) {
  const float gap = std::max({occluder.left - target.right, target.left - occluder.right, occluder.top - target.bottom, target.top - occluder.bottom, 0.F});
  const float depth = target.farthest - occluder.nearest;
  return (1 - smooth(gap / fade_padding)) * smooth(depth / .01F);
}
struct Regions final {
  std::array<ProjectedRegion, region_capacity> values{};
  std::size_t count{};
  void add(ProjectedRegion value) {
    value = expanded(value, silhouette_padding);
    if (!on_screen(value))
      return;
    if (count < values.size())
      values[count++] = value;
    else {
      // Preserve safety under a malformed over-capacity public fixture without
      // allocating or dropping new critical geometry. Real runtime fits below256.
      auto& last = values.back();
      last.left = std::min(last.left, value.left);
      last.right = std::max(last.right, value.right);
      last.top = std::min(last.top, value.top);
      last.bottom = std::max(last.bottom, value.bottom);
      last.nearest = std::min(last.nearest, value.nearest);
      last.farthest = std::max(last.farthest, value.farthest);
    }
  }
};
VisibilityBounds world_box(Vec3 a, Vec3 b, float padding) {
  return {{std::min(a.x, b.x) - padding, std::min(a.y, b.y) - padding, std::min(a.z, b.z) - padding},
      {std::max(a.x, b.x) + padding, std::max(a.y, b.y) + padding, std::max(a.z, b.z) + padding}};
}
} // namespace
std::optional<VisibilityBounds> visibility_mesh_bounds(std::string_view mesh) {
  const auto& rows = detail::visibility_mesh_bounds;
  const auto found = std::lower_bound(rows.begin(), rows.end(), mesh, [](const auto& row, std::string_view key) { return row.id < key; });
  if (found == rows.end() || found->id != mesh)
    return std::nullopt;
  return VisibilityBounds{{found->minimum[0], found->minimum[1], found->minimum[2]}, {found->maximum[0], found->maximum[1], found->maximum[2]}};
}
ProjectedRegion project_region(const presentation::Matrix& matrix, const VisibilityBounds& box) {
  const float inf = std::numeric_limits<float>::infinity();
  ProjectedRegion r{inf, inf, -inf, -inf, inf, -inf};
  for (float x : {box.minimum.x, box.maximum.x})
    for (float y : {box.minimum.y, box.maximum.y})
      for (float z : {box.minimum.z, box.maximum.z}) {
        const float w = matrix[12] * x + matrix[13] * y + matrix[14] * z + matrix[15];
        if (!std::isfinite(w) || w <= 0)
          continue;
        const float px = (matrix[0] * x + matrix[1] * y + matrix[2] * z + matrix[3]) / w;
        const float py = -(matrix[4] * x + matrix[5] * y + matrix[6] * z + matrix[7]) / w;
        const float depth = (matrix[8] * x + matrix[9] * y + matrix[10] * z + matrix[11]) / w;
        r.left = std::min(r.left, px);
        r.right = std::max(r.right, px);
        r.top = std::min(r.top, py);
        r.bottom = std::max(r.bottom, py);
        r.nearest = std::min(r.nearest, depth);
        r.farthest = std::max(r.farthest, depth);
      }
  return r;
}
void preserve_play_visibility(presentation::StyleFrame& frame, std::size_t scenery_count, const gameplay::Snapshot* run) {
  Regions critical;
  scenery_count = std::min(scenery_count, frame.instances.size());
  bool shield_added = false;
  for (std::size_t index = scenery_count; index < frame.instances.size(); ++index) {
    const auto& instance = frame.instances[index];
    if ((instance.effect && !instance.shield_rim) || instance.attachment_parent_id)
      continue;
    if (instance.shield_rim) {
      if (shield_added)
        continue;
      shield_added = true;
    }
    if (const auto box = visibility_mesh_bounds(instance.mesh))
      critical.add(project_region(presentation::multiply(frame.camera, presentation::model_matrix(instance)), *box));
  }
  if (run) {
    const auto point = [&](gameplay::Vec2 p, float height) { return Vec3{p.x, height, run->runner_distance - p.z + 2.3F}; };
    for (const auto& actor : run->entities) {
      if (actor.kind == gameplay::EntityKind::boss) {
        const auto* boss = run->boss_combat && run->boss_combat->actor_id == actor.id ? &*run->boss_combat : nullptr;
        if (boss && boss->plan && (boss->phase == gameplay::BossPhase::warning || boss->phase == gameplay::BossPhase::salvo))
          for (std::size_t i = boss->next_release; i < boss->plan->shells.size(); ++i) {
            const auto& shell = boss->plan->shells[i];
            const float range = std::hypot(shell.aim.x - shell.shot.origin.x, shell.aim.z - shell.shot.origin.z);
            const gameplay::Vec2 end{shell.shot.origin.x + range * shell.trajectory.x,
                shell.shot.origin.z + range * shell.trajectory.z};
            critical.add(project_region(frame.camera, world_box(point(shell.shot.origin, .08F), point(end, .08F), .45F)));
          }
        continue;
      }
      if (actor.warning_seconds > 0) {
        critical.add(project_region(frame.camera, world_box(point(actor.position, .08F), point(actor.warning_aim, .08F), .45F)));
        if (actor.kind == gameplay::EntityKind::jeep) {
          const auto muzzle=gameplay::jeep_minigun_shot(actor,actor.warning_aim,actor.minigun_barrel);
          if(muzzle.valid) critical.add(project_region(frame.camera,world_box(point(muzzle.origin,.08F),point(actor.warning_aim,.08F),.45F)));
        }
        if(actor.kind==gameplay::EntityKind::tank) {
          const auto muzzle=gameplay::vehicle_cannon_shot(actor,actor.warning_aim);
          if(muzzle.valid)
            critical.add(project_region(frame.camera,world_box(point(muzzle.origin,.08F),point(actor.warning_aim,.08F),.45F)));
        }
        if (actor.kind != gameplay::EntityKind::soldier)
          continue;
        const auto attack = gameplay::infantry_attack(actor.infantry_role);
        const auto muzzle = gameplay::infantry_shot(actor.infantry_role, actor.position, actor.warning_aim);
        if (!muzzle.valid)
          continue;
        const float distance = std::hypot(actor.warning_aim.x - muzzle.origin.x, actor.warning_aim.z - muzzle.origin.z);
        for (unsigned pellet = 0; pellet < attack.pellets; ++pellet) {
          const float angle = (static_cast<float>(pellet) - static_cast<float>(attack.pellets - 1) * .5F) * attack.spread_radians;
          const gameplay::Vec2 end{muzzle.origin.x + distance * (muzzle.direction.x * std::cos(angle) - muzzle.direction.z * std::sin(angle)),
              muzzle.origin.z + distance * (muzzle.direction.x * std::sin(angle) + muzzle.direction.z * std::cos(angle))};
          critical.add(project_region(frame.camera, world_box(point(muzzle.origin, .08F), point(end, .08F), .45F)));
        }
      }
    }
    for (const auto& beam : run->beams) {
      const auto p = point(beam.target, .025F);
      critical.add(project_region(frame.camera, world_box(p, p, beam.vehicle ? 1.2F : .9F)));
    }
    for (const auto& shot : run->projectiles) {
      const auto hazard=point(shot.position,.05F);
      critical.add(project_region(frame.camera,world_box(hazard,hazard,shot.radius)));
      if (shot.kind != gameplay::EnemyProjectileKind::jeep_minigun && shot.infantry_role == gameplay::InfantryRole::none)
        continue;
      const float height=std::min(2.5F,shot.launch_height+shot.rise_per_second*shot.age_seconds);
      const auto p=point(shot.position,height);
      critical.add(project_region(frame.camera,world_box(p,p,.22F)));
    }
  }
  if (!critical.count)
    return;
  for (std::size_t index = 0; index < scenery_count; ++index) {
    auto& prop = frame.instances[index];
    if (prop.effect)
      continue;
    const auto box = visibility_mesh_bounds(prop.mesh);
    if (!box)
      continue;
    const auto model = presentation::model_matrix(prop);
    const float highest = model[7] + std::max(model[4] * box->minimum.x, model[4] * box->maximum.x) + std::max(model[5] * box->minimum.y, model[5] * box->maximum.y) + std::max(model[6] * box->minimum.z, model[6] * box->maximum.z);
    // Short ground dressing does not occlude actor silhouettes; elevated
    // rotors and every tall landmark type share the same geometric rule.
    if (highest < .6F)
      continue;
    const auto region = project_region(presentation::multiply(frame.camera, model), *box);
    if (!on_screen(region))
      continue;
    float strength = 0;
    for (std::size_t i = 0; i < critical.count && strength < .999F; ++i)
      strength = std::max(strength, covering(region, critical.values[i]));
    prop.tint[3] = std::min(prop.tint[3], 1 - (1 - minimum_alpha) * strength);
  }
}
} // namespace human_resources::product
