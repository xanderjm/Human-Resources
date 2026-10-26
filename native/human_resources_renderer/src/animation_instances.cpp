#include <algorithm>
#include <cmath>
#include <human_resources/renderer/animation_instances.h>
#include <map>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

namespace human_resources::renderer {
namespace {
constexpr animation::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
animation::Matrix multiply(const animation::Matrix& a, const animation::Matrix& b) {
  animation::Matrix c{};
  for (std::size_t col = 0; col < 4; ++col)
    for (std::size_t row = 0; row < 4; ++row)
      for (std::size_t k = 0; k < 4; ++k)
        c[col * 4 + row] += a[k * 4 + row] * b[col * 4 + k];
  return c;
}
// Rest sockets are rigid authoring transforms. Rest pose is obtained through
// Player to preserve the complete glTF parent hierarchy, including root scale.
bool inverse_affine(const animation::Matrix& m, animation::Matrix& out) {
  const float a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9], g = m[2], h = m[6], i = m[10];
  const float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  if (!std::isfinite(det) || std::abs(det) < 1e-10F)
    return false;
  out = identity;
  out[0] = (e * i - f * h) / det;
  out[4] = (c * h - b * i) / det;
  out[8] = (b * f - c * e) / det;
  out[1] = (f * g - d * i) / det;
  out[5] = (a * i - c * g) / det;
  out[9] = (c * d - a * f) / det;
  out[2] = (d * h - e * g) / det;
  out[6] = (b * g - a * h) / det;
  out[10] = (a * e - b * d) / det;
  for (std::size_t row = 0; row < 3; ++row)
    out[12 + row] = -(out[row] * m[12] + out[4 + row] * m[13] + out[8 + row] * m[14]);
  return true;
}
animation::Matrix rest_matrix(const animation::Transform& t) {
  const float x = t.rotation[0], y = t.rotation[1], z = t.rotation[2], w = t.rotation[3];
  return {(1 - 2 * y * y - 2 * z * z) * t.scale[0], (2 * x * y + 2 * z * w) * t.scale[0], (2 * x * z - 2 * y * w) * t.scale[0], 0,
      (2 * x * y - 2 * z * w) * t.scale[1], (1 - 2 * x * x - 2 * z * z) * t.scale[1], (2 * y * z + 2 * x * w) * t.scale[1], 0,
      (2 * x * z + 2 * y * w) * t.scale[2], (2 * y * z - 2 * x * w) * t.scale[2], (1 - 2 * x * x - 2 * y * y) * t.scale[2], 0,
      t.translation[0], t.translation[1], t.translation[2], 1};
}
std::string_view locomotion(const AnimationIntent& i) {
  return i.speed > .04F ? (i.warning || i.fleeing ? "run" : "walk") : "idle";
}
std::string_view capture_clip(float progress) {
  return progress < presentation::capture_lift_fraction ? "beam_lift" : "beam_struggle";
}
float capture_clip_time(const animation::Asset& asset, float progress) {
  const auto name = capture_clip(progress);
  const float lift = presentation::capture_lift_fraction;
  const float fraction = progress < lift ? progress / lift : (progress - lift) / (1 - lift);
  return fraction * asset.clips()[asset.find_clip(name)].seconds;
}
std::string_view aim_clip(const animation::Asset& asset, float seconds) {
  return seconds < asset.clips()[asset.find_clip("aim_enter")].seconds ? "aim_enter" : "aim";
}
float aim_clip_time(const animation::Asset& asset, float seconds) {
  const float enter = asset.clips()[asset.find_clip("aim_enter")].seconds;
  return seconds < enter ? seconds : seconds - enter;
}
float foot_floor(const animation::Matrix& frame,const std::vector<animation::Vec3>& surface) {
  float floor=std::numeric_limits<float>::max();
  for(const auto& p:surface)floor=std::min(floor,frame[1]*p[0]+frame[5]*p[1]+frame[9]*p[2]+frame[13]);
  return floor;
}
void rotate_foot(animation::Matrix& frame,float yaw) {
  const float c=std::cos(yaw),s=std::sin(yaw);
  for(std::size_t col=0;col<3;++col) {const auto at=col*4;const float x=frame[at],z=frame[at+2];frame[at]=c*x+s*z;frame[at+2]=-s*x+c*z;}
}
} // namespace
struct AnimationInstances::State final {
  struct Loaded final {
    struct Foot final {
      std::size_t thigh{},shin{},node{};
      animation::Matrix neutral{};
      std::vector<animation::Vec3> surface;
      float floor{},walk_floor{},length{};
    };
    animation::Asset asset;
    std::vector<assets::PreviewVertex> vertices;
    animation::Matrix inverse_socket{identity};
    animation::Matrix inverse_weapon{identity};
    animation::Pose aim_reference;
    animation::Pose fire_reference;
    bool has_head{};
    bool has_weapon{};
    std::array<Foot,2> feet;
  };
  struct Instance final {
    const Loaded* source{};
    animation::Player player;
    animation::Player foot_player;
    std::vector<animation::DeformedVertex> skin;
    std::vector<assets::PreviewVertex> vertices;
    AnimatedActor output;
    animation::Pose aimed_pose;
    animation::Pose capture_pose;
    AnimationIntent previous;
    std::array<animation::Matrix,2> visible_feet{};
    std::array<animation::Vec3,2> foot_anchors{},visible_knees{};
    std::array<float,2> foot_yaws{};
    float foot_phase{},foot_weight{},last_gait_phase{};
    bool feet_valid{};
    bool seen{};
  };
  std::map<std::string, Loaded, std::less<>> assets;
  std::map<std::uint64_t, Instance> actors;
};
AnimationInstances::AnimationInstances() : state_(std::make_unique<State>()) {}
AnimationInstances::~AnimationInstances() = default;
presentation::Matrix head_attachment_matrix(const AnimatedActor& actor) {
  presentation::Matrix result{};
  for (std::size_t row = 0; row < 4; ++row)
    for (std::size_t column = 0; column < 4; ++column)
      result[row * 4 + column] = actor.head_delta[column * 4 + row];
  return result;
}
presentation::Matrix weapon_attachment_matrix(const AnimatedActor& actor) {
  presentation::Matrix result{};
  for (std::size_t row = 0; row < 4; ++row)
    for (std::size_t column = 0; column < 4; ++column)
      result[row * 4 + column] = actor.weapon_delta[column * 4 + row];
  return result;
}
bool AnimationInstances::load(std::string_view id, std::span<const std::byte> hra, std::span<const assets::PreviewVertex> vertices) {
  if (id.empty() || id.size() > 64 || state_->assets.contains(id) || state_->assets.size() >= asset_capacity)
    return false;
  State::Loaded source;
  if (!animation::decode(hra, source.asset).valid() || source.asset.vertices().size() != vertices.size())
    return false;
  for (std::string_view clip : {"idle", "walk", "run", "startle", "beam_lift", "beam_struggle", "beam_release"})
    if (source.asset.find_clip(clip) >= source.asset.clips().size())
      return false;
  for (std::size_t v = 0; v < vertices.size(); ++v)
    for (std::size_t c = 0; c < 3; ++c)
      if (std::abs(vertices[v].position[c] - source.asset.vertices()[v].position[c]) > 1e-4F)
        return false;
  source.vertices.assign(vertices.begin(), vertices.end());
  const auto socket = source.asset.find_node("socket_head");
  source.has_head = socket < source.asset.nodes().size();
  const auto weapon = source.asset.find_node("socket_weapon");
  source.has_weapon = weapon < source.asset.nodes().size();
  if (source.has_head || source.has_weapon) {
    std::array<animation::Matrix, animation::max_nodes> world{};
    const auto nodes = source.asset.nodes();
    for (std::size_t n = 0; n < nodes.size(); ++n) {
      world[n] = rest_matrix(nodes[n].rest);
      if (nodes[n].parent >= 0)
        world[n] = multiply(world[static_cast<std::size_t>(nodes[n].parent)], world[n]);
    }
    if (source.has_head && !inverse_affine(world[socket], source.inverse_socket))
      return false;
    if (source.has_weapon) {
      for (std::size_t i = 0; i < 16; ++i)
        if (std::abs(world[weapon][i] - identity[i]) > 1e-4F) return false;
      for (std::string_view clip : {"aim_enter", "aim", "fire", "aim_exit"})
        if (source.asset.find_clip(clip) >= source.asset.clips().size()) return false;
      if (!inverse_affine(world[weapon], source.inverse_weapon)) return false;
      animation::Player reference;
      if (!reference.bind(source.asset, "aim")) return false;
      source.aim_reference = reference.pose();
      if (!reference.play("fire",0)) return false;
      source.fire_reference = reference.pose();
      animation::Pose probe;
      const auto& w = source.aim_reference.world[weapon];
      if (!animation::aim_infantry_pose(source.asset, source.aim_reference, source.aim_reference,
              {w[12] + w[8] * 6, 2.5F, w[14] + w[10] * 6}, probe)) return false;
      animation::Player walk;if(!walk.bind(source.asset,"walk"))return false;
      for(std::size_t side=0;side<2;++side) {
        auto& foot=source.feet[side];const auto suffix=side==0 ? ".L":".R";
        foot.thigh=source.asset.find_node(std::string("thigh")+suffix);foot.shin=source.asset.find_node(std::string("shin")+suffix);
        foot.node=source.asset.find_node(std::string("foot")+suffix);
        if(foot.thigh>=nodes.size() || foot.shin>=nodes.size() || foot.node>=nodes.size())return false;
        foot.neutral=source.aim_reference.world[foot.node];animation::Matrix inverse{};
        if(!inverse_affine(world[foot.node],inverse))return false;
        for(const auto& vertex:source.asset.vertices())for(std::size_t slot=0;slot<4;++slot)
          if(vertex.weights[slot]>.99F && source.asset.joints()[vertex.joints[slot]].node==foot.node) {
            const auto& p=vertex.position;foot.surface.push_back({inverse[0]*p[0]+inverse[4]*p[1]+inverse[8]*p[2]+inverse[12],
                inverse[1]*p[0]+inverse[5]*p[1]+inverse[9]*p[2]+inverse[13],inverse[2]*p[0]+inverse[6]*p[1]+inverse[10]*p[2]+inverse[14]});
          }
        if(foot.surface.empty())return false;
        foot.floor=foot_floor(foot.neutral,foot.surface);foot.walk_floor=foot_floor(walk.pose().world[foot.node],foot.surface);
        const auto distance=[](const auto& a,const auto& b){return std::sqrt(std::pow(a[12]-b[12],2.F)+std::pow(a[13]-b[13],2.F)+std::pow(a[14]-b[14],2.F));};
        foot.length=distance(source.aim_reference.world[foot.thigh],source.aim_reference.world[foot.shin])+distance(source.aim_reference.world[foot.shin],foot.neutral);
      }
    }
  }
  state_->assets.emplace(std::string(id), std::move(source));
  return true;
}
bool AnimationInstances::contains(std::string_view id) const {
  return state_->assets.contains(id);
}
void AnimationInstances::begin_frame() {
  for (auto& [id, actor] : state_->actors) {
    (void)id;
    actor.seen = false;
  }
}
const AnimatedActor* AnimationInstances::update(std::uint64_t id, std::string_view name, const AnimationIntent& intent, float dt) {
  if (id == 0 || !std::isfinite(dt) || dt < 0 || dt > .25F || !std::isfinite(intent.speed) || intent.speed < 0 || intent.speed > 20 ||
      !std::isfinite(intent.capture_progress) || intent.capture_progress < 0 || intent.capture_progress > 1)
    return nullptr;
  if ((intent.aim_pose_seconds && (!std::isfinite(*intent.aim_pose_seconds) || *intent.aim_pose_seconds < 0)) ||
      (intent.preparing && !intent.aim_pose_seconds) || (intent.body_heading && !std::isfinite(*intent.body_heading)))
    return nullptr;
  const auto source = state_->assets.find(name);
  if (source == state_->assets.end())
    return nullptr;
  auto found = state_->actors.find(id);
  if (found != state_->actors.end() && found->second.source != &source->second) {
    if (found->second.seen)
      return nullptr;
    // Run restart may reuse an entity ID for a different kind. This is a new
    // presentation actor, not an invalid player rebinding into moved storage.
    state_->actors.erase(found);
    found = state_->actors.end();
  }
  bool fresh = found == state_->actors.end();
  if (fresh) {
    if (state_->actors.size() >= actor_capacity)
      return nullptr;
    found = state_->actors.try_emplace(id).first;
    auto& a = found->second;
    a.source = &source->second;
    const bool firing = a.source->has_weapon && (intent.fire_sequence > 0 || intent.recovering) && intent.fire_age_seconds >= 0 && intent.fire_age_seconds < .6F;
    const bool authored_aim = a.source->has_weapon && (intent.preparing || intent.warning) && intent.aim_pose_seconds.has_value();
    const auto initial = intent.capturing ? (intent.product_capture_transport ? capture_clip(intent.capture_progress) : "beam_lift") : firing       ? "fire"
                                                                                                                                   : authored_aim ? aim_clip(a.source->asset, *intent.aim_pose_seconds)
                                                                                                                                   : intent.warning ? (a.source->has_weapon ? "aim_enter" : "startle")
                                                                                                                                                    : locomotion(intent);
    if (!a.player.bind(a.source->asset, initial)) {
      state_->actors.erase(found);
      return nullptr;
    }
    if(a.source->has_weapon && !a.foot_player.bind(a.source->asset,"walk"))return nullptr;
    if (initial == std::string_view("walk") || initial == std::string_view("run") || initial == std::string_view("idle")) {
      const auto clip = a.source->asset.find_clip(initial);
      const float phase = static_cast<float>((id * 2654435761ULL) % 65536ULL) / 65536.F;
      if (!a.player.seek(phase * a.source->asset.clips()[clip].seconds)) {
        state_->actors.erase(found);
        return nullptr;
      }
    }
    a.skin.resize(a.source->vertices.size());
    a.vertices = a.source->vertices;
    a.previous = intent;
    a.output.head_delta = identity;
    a.output.weapon_delta = identity;
    a.output.launch_weapon_delta = identity;
    if (firing && !intent.capturing && !a.player.seek(intent.fire_age_seconds)) return nullptr;
    if (authored_aim && !firing && !intent.capturing && !a.player.seek(aim_clip_time(a.source->asset, *intent.aim_pose_seconds)))
      return nullptr;
    if (intent.capturing && intent.product_capture_transport && !a.player.seek(capture_clip_time(a.source->asset, intent.capture_progress)))
      return nullptr;
  }
  auto& a = found->second;
  if (a.seen)
    return nullptr; // duplicate IDs are an error, never advance twice
  a.seen = true;
  const auto previous=a.previous;
  bool changed = fresh;
  if (dt > 0) {
    const auto current = a.player.clip_name();
    std::string_view next = current;
    float fade = .18F;
    bool new_fire = false;
    bool sample_aim = false;
    if (intent.capturing) {
      if (intent.product_capture_transport) {
        // Run captures can finish in a fraction of the authored clip's normal
        // duration. Fit its full lift/struggle to real progress, while the pose
        // crossfade and canonical product root remain on active time.
        next = capture_clip(intent.capture_progress);
        fade = .06F;
      } else if (!a.previous.capturing) {
        next = "beam_lift";
        fade = .08F;
      } else if (current == "beam_lift" && a.player.finished()) {
        next = "beam_struggle";
        fade = 0;
      }
    } else if (a.previous.capturing) {
      next = "beam_release";
      fade = intent.product_capture_transport ? .08F : 0;
    } else if (current == "beam_release" && !a.player.finished()) {
    } else if (a.source->has_weapon) {
      if (intent.fire_sequence != a.previous.fire_sequence && intent.fire_sequence > 0 &&
          intent.fire_age_seconds >= 0 && intent.fire_age_seconds < .6F) {
        next = "fire"; fade = 0; new_fire = true;
      } else if (current == "fire" && !a.player.finished()) {
      } else if ((intent.preparing || intent.warning) && intent.aim_pose_seconds) {
        next = aim_clip(a.source->asset, *intent.aim_pose_seconds);
        // The authored enter already raises from its known carry pose. A fade
        // from cache history would make live and restored preparation differ.
        fade = 0;
        sample_aim = true;
      } else if (intent.warning) {
        if (!a.previous.warning || (current != "aim_enter" && current != "aim")) {
          next = "aim_enter"; fade = .08F;
        } else if (current == "aim_enter" && a.player.finished()) next = "aim";
      } else if (current == "aim" || current == "aim_enter" || current == "fire") {
        next = intent.speed > .04F ? locomotion(intent) : "aim_exit";
        fade = intent.speed > .04F ? .18F : 0;
      } else if (current == "aim_exit" && !a.player.finished()) {
        if(intent.speed>.04F) {next=locomotion(intent);fade=.12F;}
      } else next = locomotion(intent);
    } else if (intent.warning && !a.previous.warning) {
      next = "startle";
      fade = .08F;
    } else if (current == "startle" && !a.player.finished()) {
    } else
      next = locomotion(intent);
    if ((next != current || new_fire) && !a.player.play(next, fade, new_fire))
      return nullptr;
    if (new_fire && !a.player.seek(intent.fire_age_seconds)) return nullptr;
    if (sample_aim && !a.player.seek(aim_clip_time(a.source->asset, *intent.aim_pose_seconds))) return nullptr;
    float rate = 1;
    if (intent.capturing && intent.product_capture_transport) {
      const float target = capture_clip_time(a.source->asset, intent.capture_progress);
      rate = std::clamp((target - a.player.time()) / dt, 0.F, 256.F);
    } else if (next == "walk" || next == "run") {
      const auto& clip = a.source->asset.clips()[a.source->asset.find_clip(next)];
      if (clip.stride_m <= 0 || clip.seconds <= 0)
        return nullptr;
      rate = intent.speed / (clip.stride_m / clip.seconds);
    }
    // Advance the gait clock at stride-matched speed, but blend in active
    // seconds. One pose evaluation regardless of movement speed.
    if (!new_fire && !sample_aim && !a.player.advance(dt, rate))
      return nullptr;
    changed = true;
    a.previous = intent;
  }
  if (changed) {
    const animation::Pose* pose = &a.player.pose();
    const auto clip = a.player.clip_name();
    if (intent.product_capture_transport && (clip == "beam_lift" || clip == "beam_struggle" || clip == "beam_release")) {
      if (!animation::normalize_capture_root(a.source->asset, *pose, a.capture_pose)) return nullptr;
      pose = &a.capture_pose;
    }
    if (a.source->has_weapon && !intent.capturing && (clip == "aim_enter" || clip == "aim" || clip == "fire" || clip == "aim_exit")) {
      float weight = 1;
      if (clip == "aim_enter" || clip == "aim_exit") {
        const auto duration = a.source->asset.clips()[a.source->asset.find_clip(clip)].seconds;
        const float u = std::clamp(a.player.time() / duration, 0.F, 1.F);
        weight = u * u * (3 - 2 * u);
        if (clip == "aim_exit") weight = 1 - weight;
      }
      if (!animation::aim_infantry_pose(a.source->asset, a.player.pose(), a.source->aim_reference,
              intent.aim_target, a.aimed_pose, weight)) return nullptr;
      pose = &a.aimed_pose;
    }
    if(a.source->has_weapon) {
      const bool ground=clip=="idle" || clip=="walk" || clip=="run" || clip=="aim_enter" || clip=="aim" || clip=="fire" || clip=="aim_exit";
      const float angle=intent.body_heading && previous.body_heading ? std::remainder(*intent.body_heading-*previous.body_heading,2*std::numbers::pi_v<float>):0;
      const bool turning=ground && !intent.capturing && intent.speed<=.04F && std::abs(angle)>.0001F;
      const bool entry=ground && intent.aim_pose_seconds && (intent.preparing || intent.warning) &&
          !(previous.aim_pose_seconds && (previous.preparing || previous.warning));
      if(!ground || intent.capturing || intent.speed>.04F) {
        a.foot_weight=0;
        a.feet_valid=false;
      }
      if(ground && !intent.capturing && intent.speed<=.04F && dt>0 && a.feet_valid && intent.body_heading) {
        bool seeded=false;
        if(turning || entry) {
          if(a.foot_weight<=0) {
            seeded=true;
            a.foot_phase=a.last_gait_phase;
            for(std::size_t side=0;side<2;++side) {const auto& foot=a.visible_feet[side];a.foot_anchors[side]={foot[12],foot[13],foot[14]};a.foot_yaws[side]=0;}
          }
          a.foot_weight=1;
        } else a.foot_weight=std::max(0.F,a.foot_weight-dt/.12F);
        if(a.foot_weight>0) {
          const float previous_phase=a.foot_phase;
          if(!seeded)a.foot_phase+=turning ? std::abs(angle)/std::numbers::pi_v<float>:dt*4;
          a.foot_phase=std::fmod(a.foot_phase,1.F);
          const auto seconds=a.source->asset.clips()[a.source->asset.find_clip("walk")].seconds;
          if(!a.foot_player.seek(a.foot_phase*seconds))return nullptr;
          std::array<animation::Matrix,2> targets;
          auto knee_poles=a.visible_knees;
          for(std::size_t side=0;side<2;++side) {
            const auto& foot=a.source->feet[side];auto& anchor=a.foot_anchors[side];
            const float c=std::cos(-angle),s=std::sin(-angle),x=anchor[0],z=anchor[2];anchor[0]=c*x+s*z;anchor[2]=-s*x+c*z;
            a.foot_yaws[side]-=angle;
            auto target=a.foot_player.pose().world[foot.node];
            const float lift=std::max(0.F,foot_floor(target,foot.surface)-foot.walk_floor);
            // The existing infantry walk authors 60% stance, with the
            // other foot half a cycle apart. Finish the replant throughout
            // that swing, rather than snapping at a height threshold.
            const auto swing=[side](float phase) {
              const float u=std::clamp((std::fmod(phase+static_cast<float>(side)*.5F,1.F)-.6F)/.4F,0.F,1.F);
              return u*u*(3-2*u);
            };
            const float from=swing(previous_phase),to=swing(a.foot_phase);
            const float blend=seeded ? 0.F : !turning ? std::min(1.F,dt/.12F) :
                to<from ? 1.F : std::clamp((to-from)/std::max(.000001F,1-from),0.F,1.F);
            anchor[0]+=(foot.neutral[12]-anchor[0])*blend;anchor[2]+=(foot.neutral[14]-anchor[2])*blend;
            a.foot_yaws[side]*=1-blend;
            rotate_foot(target,a.foot_yaws[side]);target[12]=anchor[0];target[14]=anchor[2];target[13]=0;
            target[13]=foot.floor+lift-foot_floor(target,foot.surface);
            if(seeded) {
              target=a.visible_feet[side];rotate_foot(target,-angle);target[12]=anchor[0];target[14]=anchor[2];
            }
            const auto& hip=pose->world[foot.thigh];
            target[13]=std::max(target[13],hip[13]-foot.length*.999F);
            const float vertical=target[13]-hip[13];
            const float maximum=std::sqrt(std::max(.000001F,foot.length*foot.length*.998F-vertical*vertical));
            const float dx=target[12]-hip[12],dz=target[14]-hip[14],horizontal=std::hypot(dx,dz);
            if(horizontal>maximum) {target[12]=hip[12]+dx*maximum/horizontal;target[14]=hip[14]+dz*maximum/horizontal;}
            const float kx=knee_poles[side][0],kz=knee_poles[side][2];
            knee_poles[side][0]=c*kx+s*kz;knee_poles[side][2]=-s*kx+c*kz;
            targets[side]=target;
          }
          if(!animation::place_infantry_feet(a.source->asset,*pose,targets,knee_poles,a.capture_pose,a.foot_weight))return nullptr;
          // Interpolated joint arcs can sag below a rigid sole. Correct only
          // the affected ankle target, never the pelvis/root/weapon frame.
          bool floor_fix=false;
          for(std::size_t side=0;side<2;++side) {
            const auto& foot=a.source->feet[side];targets[side]=a.capture_pose.world[foot.node];
            const float below=foot.floor-foot_floor(targets[side],foot.surface);
            if(below>0) {targets[side][13]+=below;floor_fix=true;}
          }
          if(floor_fix && !animation::place_infantry_feet(a.source->asset,a.capture_pose,targets,knee_poles,a.capture_pose))return nullptr;
          pose=&a.capture_pose;
          if(!a.player.adopt_infantry_legs(*pose))return nullptr;
        }
      }
      if(ground && !intent.capturing && (dt>0 || fresh)) {
        for(std::size_t side=0;side<2;++side) {
          a.visible_feet[side]=pose->world[a.source->feet[side].node];
          const auto& knee=pose->world[a.source->feet[side].shin];a.visible_knees[side]={knee[12],knee[13],knee[14]};
        }
        a.feet_valid=true;
        if(clip=="walk" || clip=="run")a.last_gait_phase=a.player.time()/a.source->asset.clips()[a.source->asset.find_clip(clip)].seconds;
      }
    }
    if (!animation::skin_vertices(a.source->asset, *pose, a.skin))
      return nullptr;
    a.output.minimum = a.output.maximum = a.skin.front().position;
    for (std::size_t v = 0; v < a.vertices.size(); ++v) {
      a.vertices[v].position = a.skin[v].position;
      a.vertices[v].normal = a.skin[v].normal;
      for (std::size_t c = 0; c < 3; ++c) {
        a.output.minimum[c] = std::min(a.output.minimum[c], a.skin[v].position[c]);
        a.output.maximum[c] = std::max(a.output.maximum[c], a.skin[v].position[c]);
      }
    }
    a.output.has_head = a.source->has_head;
    if (a.output.has_head) {
      const auto socket = a.source->asset.find_node("socket_head");
      a.output.head_delta = multiply(pose->world[socket], a.source->inverse_socket);
    }
    a.output.has_weapon = a.source->has_weapon;
    if (a.output.has_weapon) {
      const auto socket = a.source->asset.find_node("socket_weapon");
      a.output.weapon_delta = multiply(pose->world[socket], a.source->inverse_weapon);
      if (!intent.capturing && intent.fire_sequence > 0 && intent.fire_sequence != a.output.launch_sequence &&
          intent.fire_age_seconds >= 0 && intent.fire_age_seconds < .6F) {
        if (!animation::aim_infantry_pose(a.source->asset,a.source->fire_reference,a.source->aim_reference,
                intent.aim_target,a.aimed_pose)) return nullptr;
        a.output.launch_weapon_delta=multiply(a.aimed_pose.world[socket],a.source->inverse_weapon);
        a.output.launch_sequence=intent.fire_sequence;
      }
    }
    a.output.vertices = a.vertices;
    a.output.clip = a.player.clip_name();
    a.output.clip_time = a.player.time();
    ++a.output.revision;
  }
  return &a.output;
}
void AnimationInstances::end_frame() {
  for (auto i = state_->actors.begin(); i != state_->actors.end();)
    if (!i->second.seen)
      i = state_->actors.erase(i);
    else
      ++i;
}
std::size_t AnimationInstances::size() const {
  return state_->actors.size();
}
void AnimationInstances::clear_actors() {
  state_->actors.clear();
}
} // namespace human_resources::renderer
