#pragma once
#include <human_resources/gameplay/gameplay.h>
#include <algorithm>
#include <cmath>
namespace human_resources::product::detail {
struct VehicleMissilePose {
  gameplay::Vec2 position{}, velocity{};
  float height{}, vertical_speed{};
};
// Saved authority remains two-dimensional. This short source-axis departure
// joins the existing boss fan without changing its committed collision path.
// Elevation starts horizontal at the real aperture and eases to the UFO plane.
inline VehicleMissilePose vehicle_missile_pose(const gameplay::ProjectileView& shot) {
  VehicleMissilePose pose{shot.position,shot.velocity,shot.launch_height,0};
  const float age=std::max(0.F,shot.age_seconds);
  constexpr float departure_seconds=.12F;
  const float u=std::clamp(age/departure_seconds,0.F,1.F);
  const float weight=1-u*u*(3-2*u);
  const float derivative=age<departure_seconds ? -6*u*(1-u)/departure_seconds : 0;
  const float speed=std::hypot(shot.velocity.x,shot.velocity.z);
  const gameplay::Vec2 correction{shot.launch_direction.x*speed-shot.velocity.x,
      shot.launch_direction.z*speed-shot.velocity.z};
  pose.position.x+=correction.x*age*weight;
  pose.position.z+=correction.z*age*weight;
  pose.velocity.x+=correction.x*(weight+age*derivative);
  pose.velocity.z+=correction.z*(weight+age*derivative);
  const float rise=std::max(0.F,2.5F-shot.launch_height);
  if(rise>0 && shot.rise_per_second>0) {
    const float flight=rise/shot.rise_per_second;
    const float t=std::clamp(age/flight,0.F,1.F);
    pose.height+=rise*t*t*(3-2*t);
    pose.vertical_speed=shot.rise_per_second*6*t*(1-t);
  }
  return pose;
}
}
