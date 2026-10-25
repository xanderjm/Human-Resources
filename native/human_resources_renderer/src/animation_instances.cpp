#include <algorithm>
#include <cmath>
#include <human_resources/renderer/animation_instances.h>
#include <map>
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
  return i.speed > .04F ? (i.warning ? "run" : "walk") : "idle";
}
} // namespace
struct AnimationInstances::State final {
  struct Loaded final {
    animation::Asset asset;
    std::vector<assets::PreviewVertex> vertices;
    animation::Matrix inverse_socket{identity};
    animation::Matrix inverse_weapon{identity};
    animation::Pose aim_reference;
    animation::Pose fire_reference;
    bool has_head{};
    bool has_weapon{};
  };
  struct Instance final {
    const Loaded* source{};
    animation::Player player;
    std::vector<animation::DeformedVertex> skin;
    std::vector<assets::PreviewVertex> vertices;
    AnimatedActor output;
    animation::Pose aimed_pose;
    animation::Pose capture_pose;
    AnimationIntent previous;
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
  if (id == 0 || !std::isfinite(dt) || dt < 0 || dt > .25F || !std::isfinite(intent.speed) || intent.speed < 0 || intent.speed > 20)
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
    const auto initial = intent.capturing ? "beam_lift" : firing ? "fire" : intent.warning ? (a.source->has_weapon ? "aim_enter" : "startle")
                                                                         : locomotion(intent);
    if (!a.player.bind(a.source->asset, initial)) {
      state_->actors.erase(found);
      return nullptr;
    }
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
  }
  auto& a = found->second;
  if (a.seen)
    return nullptr; // duplicate IDs are an error, never advance twice
  a.seen = true;
  bool changed = fresh;
  if (dt > 0) {
    const auto current = a.player.clip_name();
    std::string_view next = current;
    float fade = .18F;
    bool new_fire = false;
    if (intent.capturing) {
      if (!a.previous.capturing) {
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
    float rate = 1;
    if (next == "walk" || next == "run") {
      const auto& clip = a.source->asset.clips()[a.source->asset.find_clip(next)];
      if (clip.stride_m <= 0 || clip.seconds <= 0)
        return nullptr;
      rate = intent.speed / (clip.stride_m / clip.seconds);
    }
    // Advance the gait clock at stride-matched speed, but blend in active
    // seconds. One pose evaluation regardless of movement speed.
    if (!new_fire && !a.player.advance(dt, rate))
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
