#include <human_resources/animation/animation.h>
#include "pose_math.h"
#include <algorithm>
#include <cmath>

namespace human_resources::animation {
namespace {
Vec3 minus(Vec3 a, Vec3 b) { return {a[0]-b[0],a[1]-b[1],a[2]-b[2]}; }
Vec3 plus(Vec3 a, Vec3 b) { return {a[0]+b[0],a[1]+b[1],a[2]+b[2]}; }
Vec3 times(Vec3 a,float s) { return {a[0]*s,a[1]*s,a[2]*s}; }
float product(Vec3 a,Vec3 b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
float magnitude(Vec3 a) { return std::sqrt(product(a,a)); }
Vec3 unit(Vec3 a) { return times(a,1/std::max(.000001F,magnitude(a))); }
Vec3 cross(Vec3 a,Vec3 b) { return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}; }
Vec3 position(const Matrix& m) { return {m[12],m[13],m[14]}; }
Quaternion conjugate(Quaternion q) { return {-q[0],-q[1],-q[2],q[3]}; }
Quaternion multiply(Quaternion a,Quaternion b) {
  return {a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
      a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
}
Quaternion normalized(Quaternion q) {
  float n=0;for(float v:q)n+=v*v;
  const float inverse=1/std::sqrt(std::max(.00000001F,n));for(float& v:q)v*=inverse;return q;
}
Quaternion rotation(const Matrix& m) {
  Quaternion q{};const float trace=m[0]+m[5]+m[10];
  if(trace>0) {const float s=std::sqrt(trace+1)*2;q={(m[6]-m[9])/s,(m[8]-m[2])/s,(m[1]-m[4])/s,.25F*s};}
  else if(m[0]>m[5] && m[0]>m[10]) {const float s=std::sqrt(1+m[0]-m[5]-m[10])*2;q={.25F*s,(m[4]+m[1])/s,(m[8]+m[2])/s,(m[6]-m[9])/s};}
  else if(m[5]>m[10]) {const float s=std::sqrt(1+m[5]-m[0]-m[10])*2;q={(m[4]+m[1])/s,.25F*s,(m[9]+m[6])/s,(m[8]-m[2])/s};}
  else {const float s=std::sqrt(1+m[10]-m[0]-m[5])*2;q={(m[8]+m[2])/s,(m[9]+m[6])/s,.25F*s,(m[1]-m[4])/s};}
  return normalized(q);
}
Quaternion between(Vec3 from,Vec3 to) {
  from=unit(from);to=unit(to);const float d=product(from,to);
  if(d<-.9999F) {const auto axis=unit(cross(from,std::abs(from[0])<.8F ? Vec3{1,0,0}:Vec3{0,1,0}));return {axis[0],axis[1],axis[2],0};}
  const auto axis=cross(from,to);return normalized({axis[0],axis[1],axis[2],1+d});
}
bool orient(const Asset& asset,Pose& pose,std::size_t node,Quaternion world) {
  const auto parent=asset.nodes()[node].parent;
  const Quaternion parent_rotation=parent<0 ? Quaternion{0,0,0,1}:rotation(pose.world[static_cast<std::size_t>(parent)]);
  pose.local[node].rotation=normalized(multiply(conjugate(parent_rotation),world));
  return detail::compose(asset,pose);
}
constexpr std::array<std::array<std::string_view,3>,2> legs{{{"thigh.L","shin.L","foot.L"},{"thigh.R","shin.R","foot.R"}}};
}
bool place_infantry_feet(const Asset& asset,const Pose& base,const std::array<Matrix,2>& targets,const std::array<Vec3,2>& knee_poles,Pose& output,float weight) {
  if(base.node_count!=asset.nodes().size() || !std::isfinite(weight) || weight<0 || weight>1)return false;
  Pose candidate=base;
  for(std::size_t side=0;side<2;++side) {
    const auto thigh=asset.find_node(legs[side][0]),shin=asset.find_node(legs[side][1]),foot=asset.find_node(legs[side][2]);
    if(thigh>=base.node_count || shin>=base.node_count || foot>=base.node_count || !detail::finite_matrix(targets[side]) ||
        asset.nodes()[shin].parent!=static_cast<std::int32_t>(thigh) || asset.nodes()[foot].parent!=static_cast<std::int32_t>(shin))return false;
    const auto start=position(candidate.world[thigh]),old_knee=position(candidate.world[shin]);
    const float a=magnitude(minus(old_knee,start)),b=magnitude(minus(position(candidate.world[foot]),old_knee));
    const auto end=position(targets[side]);const auto ray=minus(end,start);const float distance=magnitude(ray);
    if(!(a>.0001F && b>.0001F && distance>.0001F) || distance>a+b+.0002F || distance<std::abs(a-b)+.00001F)return false;
    const auto direction=unit(ray);auto pole=minus(knee_poles[side],start);pole=minus(pole,times(direction,product(pole,direction)));
    if(magnitude(pole)<.0001F) {pole=minus(Vec3{0,0,1},times(direction,direction[2]));if(magnitude(pole)<.0001F)pole={1,0,0};}
    const float d=std::min(distance,a+b-.000001F),along=(a*a-b*b+d*d)/(2*d);
    const auto knee=plus(start,plus(times(direction,along),times(unit(pole),std::sqrt(std::max(0.F,a*a-along*along)))));
    if(!orient(asset,candidate,thigh,multiply(between(minus(old_knee,start),minus(knee,start)),rotation(candidate.world[thigh]))))return false;
    const auto current_knee=position(candidate.world[shin]);
    if(!orient(asset,candidate,shin,multiply(between(minus(position(candidate.world[foot]),current_knee),minus(end,current_knee)),rotation(candidate.world[shin]))) ||
        !orient(asset,candidate,foot,rotation(targets[side])))return false;
    if(weight<1)for(const auto node:{thigh,shin,foot})candidate.local[node]=detail::interpolate(base.local[node],candidate.local[node],weight);
  }
  if(!detail::compose(asset,candidate))return false;
  output=candidate;return true;
}
bool apply_infantry_gait(const Asset& asset, const Pose& base, const Pose& gait, Pose& output) {
  if (base.node_count != asset.nodes().size() || gait.node_count != base.node_count) return false;
  Pose candidate = base;
  for (const auto& chain : legs) for (const auto name : chain) {
    const auto node = asset.find_node(name);
    if (node >= base.node_count || !detail::valid_transform(gait.local[node])) return false;
    candidate.local[node] = gait.local[node];
  }
  if (!detail::compose(asset, candidate)) return false;
  output = candidate;
  return true;
}
bool Player::adopt_infantry_legs(const Pose& visible) {
  if(!asset_ || visible.node_count!=pose_.node_count)return false;
  for(const auto& chain:legs)for(const auto name:chain) {
    const auto node=asset_->find_node(name);if(node>=pose_.node_count || !detail::valid_transform(visible.local[node]))return false;
  }
  for(const auto& chain:legs)for(const auto name:chain) {const auto node=asset_->find_node(name);pose_.local[node]=visible.local[node];}
  return detail::compose(*asset_,pose_);
}
bool aim_infantry_pose(const Asset& asset, const Pose& base, const Pose& reference, Vec3 target, Pose& output, float weight) {
  const auto count = asset.nodes().size(), chest = asset.find_node("chest"), weapon = asset.find_node("socket_weapon");
  if (!std::isfinite(weight) || weight < 0 || weight > 1 || base.node_count != count || reference.node_count != count || chest >= count || weapon >= count ||
      !std::all_of(target.begin(), target.end(), [](float v) { return std::isfinite(v) && std::abs(v) < 1000; }))
    return false;
  std::array<std::size_t, 2> muzzles{asset.find_node("socket_muzzle"), count};
  if (muzzles[0] >= count) {
    muzzles = {asset.find_node("socket_muzzle_left"), asset.find_node("socket_muzzle_right")};
    if (muzzles[0] >= count)
      muzzles = {asset.find_node("socket_nose"), count};
  }
  if (muzzles[0] >= count)
    return false;
  const auto descendant = [&](std::size_t node) {
    for (std::size_t depth = 0; depth < max_nodes; ++depth) {
      if (node == chest) return true;
      const auto parent = asset.nodes()[node].parent;
      if (parent < 0) return false;
      node = static_cast<std::size_t>(parent);
    }
    return false;
  };
  if (!descendant(weapon) || !descendant(muzzles[0]) || (muzzles[1] < count && !descendant(muzzles[1])))
    return false;
  Vec3 muzzle{}, forward{}, pivot{};
  for (std::size_t i = 0; i < 3; ++i) {
    muzzle[i] = reference.world[muzzles[0]][12 + i];
    if (muzzles[1] < count) muzzle[i] = (muzzle[i] + reference.world[muzzles[1]][12 + i]) * .5F;
    forward[i] = reference.world[weapon][8 + i];
    pivot[i] = reference.world[chest][12 + i];
  }
  const auto dot = [](Vec3 a, Vec3 b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
  const float length = std::sqrt(dot(forward, forward)), horizontal = std::hypot(forward[0], forward[2]);
  if (length < .001F || horizontal < .001F)
    return false;
  for (float& v : forward) v /= length;
  const Vec3 axis{-reference.world[weapon][10] / horizontal, 0, reference.world[weapon][8] / horizontal};
  const Vec3 up{-axis[2] * forward[1], axis[2] * forward[0] - axis[0] * forward[2], axis[0] * forward[1]};
  const Vec3 ray{target[0] - pivot[0], target[1] - pivot[1], target[2] - pivot[2]};
  const Vec3 offset{muzzle[0] - pivot[0], muzzle[1] - pivot[1], muzzle[2] - pivot[2]};
  const float f = dot(ray, forward), v = dot(ray, up), h = dot(offset, up), distance = std::hypot(f, v);
  if (!std::isfinite(distance) || distance < std::abs(h) + .001F)
    return false;
  const float pitch = std::clamp(std::atan2(v, f) - std::asin(std::clamp(h / distance, -1.F, 1.F)), -.35F, .45F) * weight;
  Vec3 local_axis = axis;
  const auto parent = asset.nodes()[chest].parent;
  if (parent >= 0) {
    const auto& frame = base.world[static_cast<std::size_t>(parent)];
    for (std::size_t column = 0; column < 3; ++column) {
      const float norm = std::sqrt(frame[column * 4] * frame[column * 4] + frame[column * 4 + 1] * frame[column * 4 + 1] + frame[column * 4 + 2] * frame[column * 4 + 2]);
      if (std::abs(norm - 1) > .001F) return false;
      local_axis[column] = frame[column * 4] * axis[0] + frame[column * 4 + 1] * axis[1] + frame[column * 4 + 2] * axis[2];
    }
  }
  const float sine = std::sin(pitch * .5F), cosine = std::cos(pitch * .5F);
  const Quaternion turn{local_axis[0] * sine, local_axis[1] * sine, local_axis[2] * sine, cosine};
  const auto& q = base.local[chest].rotation;
  Pose candidate = base;
  candidate.local[chest].rotation = {
      turn[3] * q[0] + turn[0] * q[3] + turn[1] * q[2] - turn[2] * q[1],
      turn[3] * q[1] - turn[0] * q[2] + turn[1] * q[3] + turn[2] * q[0],
      turn[3] * q[2] + turn[0] * q[1] - turn[1] * q[0] + turn[2] * q[3],
      turn[3] * q[3] - turn[0] * q[0] - turn[1] * q[1] - turn[2] * q[2]};
  if (!detail::compose(asset, candidate)) return false;
  output = candidate;
  return true;
}
} // namespace human_resources::animation
