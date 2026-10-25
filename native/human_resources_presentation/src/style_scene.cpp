#include <human_resources/presentation/style_scene.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace human_resources::presentation {
namespace {
constexpr float pi = std::numbers::pi_v<float>;
Vec3 sub(Vec3 a, Vec3 b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
float dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 unit(Vec3 v) {
  const float n = std::sqrt(dot(v, v));
  return {v.x / n, v.y / n, v.z / n};
}
Matrix view(Vec3 eye, Vec3 target) {
  const auto z = unit(sub(eye, target));
  const auto x = unit(cross({0, 1, 0}, z));
  const auto y = cross(z, x);
  return {x.x, x.y, x.z, -dot(x, eye), y.x, y.y, y.z, -dot(y, eye), z.x, z.y, z.z, -dot(z, eye), 0, 0, 0, 1};
}
Matrix ortho(float w, float h, float near, float far) {
  return {2 / w, 0, 0, 0, 0, 2 / h, 0, 0, 0, 0, 1 / (near - far), near / (near - far), 0, 0, 0, 1};
}
void add(StyleFrame& f, std::string_view mesh, Vec3 pos, float size = 1, float yaw = 0) {
  f.instances.push_back({.mesh = mesh, .position = pos, .scale = {size, size, size}, .rotation = {0, yaw, 0}});
}
void box(StyleFrame& f, Vec3 pos, Vec3 scale, std::array<float, 4> color, float yaw = 0) {
  f.instances.push_back({.mesh = "cube", .position = pos, .scale = scale, .rotation = {0, yaw, 0}, .tint = color});
}
void ground(StyleFrame& f, std::string_view mesh, Vec3 pos, Vec3 scale, float repeat = 1, std::array<float, 4> tint = {1, 1, 1, 1}) {
  f.instances.push_back({.mesh = mesh, .position = pos, .scale = scale, .tint = tint, .texture_repeat = repeat, .casts_shadow = false});
}
void effect(StyleFrame& f, std::string_view mesh, Vec3 pos, Vec3 scale, std::array<float, 4> color, Vec3 rotation = {}) {
  f.instances.push_back({.mesh = mesh, .position = pos, .scale = scale, .rotation = rotation, .tint = color, .effect = true, .casts_shadow = false});
}
void farmer(StyleFrame& f, float x, float z, float yaw = 0, bool resident = false) {
  add(f, resident ? "resident-body" : "farmer-body", {x, 0, z}, .88F, yaw);
  add(f, resident ? "nightcap" : "straw-hat", {x, 1.34F, z}, .95F, yaw);
}
void tank(StyleFrame& f, float x, float z, float yaw) {
  add(f, "tank-hull", {x, 0, z}, 1, yaw);
  add(f, "tank-turret", {x, .87F, z}, 1, yaw);
}
void jeep(StyleFrame& f, float x, float z, float yaw) {
  add(f, "jeep-chassis", {x, 0, z}, 1, yaw);
  for (float side : {-.79F, .79F})
    for (float along : {-.97F, .97F}) {
      const float px = x + side * std::cos(yaw) + along * std::sin(yaw);
      const float pz = z - side * std::sin(yaw) + along * std::cos(yaw);
      add(f, "jeep-wheel", {px, .3F, pz}, 1, yaw);
    }
}
void fence(StyleFrame& f, float x, float z, float length, float yaw) {
  constexpr std::array<float, 4> wood{.64F, .38F, .19F, 1};
  for (float p = -length / 2; p <= length / 2 + .01F; p += 2) {
    box(f, {x + p * std::cos(yaw), .57F, z - p * std::sin(yaw)}, {.075F, .57F, .075F}, wood, yaw);
  }
  for (float h : {.42F, .87F})
    box(f, {x, h, z}, {length / 2, .055F, .055F}, wood, yaw);
}
void vegetation(StyleFrame& f, bool desert = false) {
  for (int side : {-1, 1})
    for (int i = 0; i < 11; ++i) {
      const float z = -20 + static_cast<float>(i) * 4;
      const float x = static_cast<float>(side) * (9.2F + .45F * std::sin(static_cast<float>(i) * 2.7F));
      if (desert) {
        add(f, i % 3 == 0 ? "cliff-module-a" : "cliff-module-b", {x + static_cast<float>(side) * 1.4F, 0, z}, 1.2F, static_cast<float>(i));
        add(f, i % 2 == 0 ? "cactus-tall" : "cactus-short", {x - static_cast<float>(side) * 1.1F, 0, z + 1.3F}, .9F, static_cast<float>(i));
        add(f, "desert-boulder-a", {x - static_cast<float>(side) * .9F, 0, z - 1.3F}, .8F, static_cast<float>(i));
        add(f, "desert-scrub", {x - static_cast<float>(side) * 2, 0, z + 2}, .7F, static_cast<float>(i));
      } else {
        add(f, i % 3 == 0 ? "tree-round-large" : "tree-round-small", {x, 0, z}, i % 3 == 0 ? 1.15F : 1.05F, static_cast<float>(i));
        add(f, "bush-low", {x - static_cast<float>(side) * .65F, 0, z + 1.6F}, .85F, static_cast<float>(i));
        add(f, i % 2 == 0 ? "field-rock-a" : "field-rock-b", {x - static_cast<float>(side) * 1.1F, 0, z + .9F}, .85F, static_cast<float>(i));
      }
    }
}
void farmland(StyleFrame& f) {
  ground(f, "ground-grass", {0, -.035F, 0}, {36, 1, 36}, 10, {1, 1, 1, 1});
  ground(f, "ground-dirt", {10, -.012F, 0}, {1.4F, 1, 32}, 7);
  vegetation(f);
  for (int side : {-1, 1})
    for (float z : {-15.F, -5.F, 5.F, 15.F})
      fence(f, static_cast<float>(side) * 8.8F, z, 8, pi / 2);
  add(f, "barn", {-8.3F, 0, -11}, 1.15F, .2F);
  add(f, "silo", {-9.1F, 0, 14}, .9F);
  add(f, "windmill-mast", {8.1F, 0, 13}, 1);
  add(f, "windmill-rotor", {8.1F, 3.1F, 12.8F}, 1);
  for (const auto p : std::array<Vec3, 7>{{{-6, 0, -9}, {7, 0, -4}, {6, 0, 5}, {-6.6F, 0, 10}, {6, 0, 14}, {-6, 0, -17}, {-7, 0, 2}}})
    add(f, "hay-bale", p, .9F, p.z);
  for (int bed = 0; bed < 4; ++bed) {
    const float x = 5.4F + static_cast<float>(bed) * .72F;
    ground(f, "ground-soil", {x, .005F, -14}, {.29F, 1, 3.1F}, 2, {.9F, .9F, .8F, 1});
    for (int row = 0; row < 10; ++row)
      add(f, "crop-seedling", {x, .015F, -16.6F + static_cast<float>(row) * .58F}, 1);
  }
  for (float r : {1.4F, 2.F, 2.65F})
    effect(f, "ring", {-3, .008F, -12}, {r, 1, r}, {.43F, .49F, .21F, .2F});
  for (int i = 0; i < 32; ++i) {
    const float a = static_cast<float>(i) * 2.39996F;
    const float r = 2.5F + std::fmod(static_cast<float>(i) * 1.63F, 5.F);
    add(f, "grass-tuft", {std::sin(a) * r, 0, std::cos(a) * r * 2}, .45F, a);
  }
}
void suburbia(StyleFrame& f) {
  ground(f, "ground-grass", {0, -.04F, 0}, {36, 1, 36}, 10, {.73F, .88F, .82F, 1});
  ground(f, "ground-asphalt", {0, -.01F, 0}, {7.1F, 1, 35}, 3.5F, {.93F, 1, 1.07F, 1});
  vegetation(f);
  for (int side : {-1, 1}) {
    const float s = static_cast<float>(side);
    ground(f, "ground-pavement", {s * 7.95F, .005F, 0}, {.85F, 1, 35}, .4F);
    for (int i = 0; i < 5; ++i) {
      const float z = -18 + static_cast<float>(i) * 8.4F;
      add(f, i % 2 == 0 ? "house-coral" : "house-teal", {s * 11.3F, 0, z}, 1.1F, side < 0 ? pi / 2 : -pi / 2);
      add(f, i % 2 == 0 ? "mailbox-red" : "mailbox-teal", {s * 7.9F, .02F, z + 2.1F}, .9F, side < 0 ? pi / 2 : -pi / 2);
      add(f, "streetlamp", {s * 8.3F, .02F, z - 2.8F}, .9F);
      add(f, "wheelie-bin", {s * 7.7F, .02F, z - 1.5F}, .8F, static_cast<float>(i));
      add(f, "hedge-module", {s * 9.8F, 0, z + 3}, 1, pi / 2);
    }
    for (int i = 0; i < 38; ++i) {
      const float z = -28 + static_cast<float>(i) * 1.5F;
      box(f, {s * 7.15F, .025F, z}, {.055F, .055F, .72F}, {.62F, .61F, .54F, 1});
    }
  }
}
void desert(StyleFrame& f) {
  ground(f, "ground-sand", {0, -.03F, 0}, {36, 1, 36}, 10, {1, .95F, .92F, 1});
  ground(f, "ground-asphalt", {-9.3F, -.005F, 0}, {2, 1, 35}, 1.F, {1.12F, .88F, 1.04F, 1});
  for (int i = 0; i < 22; ++i) {
    const float z = -30 + static_cast<float>(i) * 3;
    ground(f, "plane", {-9.3F, .01F, z}, {.06F, 1, .7F}, 1, {.73F, .68F, .62F, 1});
    add(f, "road-bollard", {-6.9F, 0, z}, .9F);
  }
  vegetation(f, true);
  add(f, "service-canopy", {-9.2F, 0, 12}, 1, -.12F);
  add(f, "fuel-pump-red", {-8.4F, 0, 13}, 1, pi / 2);
  add(f, "fuel-pump-blue", {-10.1F, 0, 13}, 1, pi / 2);
  for (const auto p : std::array<Vec3, 6>{{{-4, 0, -15}, {6, 0, -7}, {6.2F, 0, 8}, {-4.8F, 0, 10}, {6.6F, 0, 15}, {-5, 0, 1}}})
    add(f, "desert-boulder-b", p, .8F, p.z);
}
void cast(StyleFrame& f, float t) {
  const float drift = std::sin(t * .5F) * .42F;
  const Vec3 ship{drift, 2.55F + .08F * std::sin(t * 2), 2.2F - .15F * t};
  add(f, "ufo-modular", ship, 1.36F, -.12F);
  const float lift = std::clamp((t - 2) / 5, 0.F, 1.F);
  const float ease = lift * lift * (3 - 2 * lift);
  StyleInstance cow{.mesh = "cow", .position = {-.65F * (1 - ease) + ship.x * ease, .05F + ease * 1.9F, 5.7F * (1 - ease) + ship.z * ease}, .scale = {1, 1, 1}, .rotation = {ease * 1.3F, .9F + t * .12F, ease * .55F}};
  f.instances.push_back(cow);
  add(f, "cow", {-4.1F, 0, -5.4F}, 1, 1.15F);
  add(f, "cow", {4.4F, 0, -7.5F}, .95F, -1.2F);
  add(f, "cow", {4.3F, 0, 12.6F}, 1, -.6F);
  add(f, "sheep", {-2.1F, 0, -2.7F}, 1, .4F);
  add(f, "sheep", {4, 0, 0}, 1, -1.2F);
  add(f, "sheep", {-5.1F, 0, 3.6F}, .95F, 1.8F);
  add(f, "pig", {5.6F, 0, -4.2F}, 1.1F, -.5F);
  add(f, "pig", {-5.6F, 0, 7.2F}, 1.1F, 1.1F);
  farmer(f, -5.2F, -2.2F, .3F, f.location == 1);
  farmer(f, .3F, -7.8F, -.4F);
  farmer(f, 5.6F, 2.9F, -.5F);
  farmer(f, -4, 11.2F, .6F, f.location == 1);
  tank(f, 3.6F, -11.7F + std::sin(t * .3F) * .25F, pi + .1F);
  jeep(f, -5.3F, -.1F, pi / 2 - .25F);
  constexpr std::array<float, 4> mint{.35F, 1, .76F, .85F};
  effect(f, "beam", {ship.x, .08F, ship.z}, {1.45F, 2.5F, 1.45F}, {.35F, 1, .76F, .09F});
  for (int i = 0; i < 3; ++i) {
    const float phase = std::fmod(t * .5F + static_cast<float>(i) / 3, 1.F);
    const float r = 1.28F * (1 - phase) + .17F;
    effect(f, "ring", {ship.x, .06F + phase * 2.35F, ship.z}, {r, 1, r}, {.35F, 1, .76F, (1 - phase) * .75F});
  }
  effect(f, "ring", {ship.x, .025F, ship.z}, {1.5F, 1, 1.5F}, mint);
  for (int i = 0; i < 24; ++i) {
    const float u = static_cast<float>(i) / 23;
    for (int side : {-1, 1})
      effect(f, "cube", {ship.x + static_cast<float>(side) * std::sin(u * pi) * 1.55F, .09F, ship.z + u * 6.5F},
          {.032F, .025F, .09F}, {.35F, 1, .76F, .7F * (1 - u)});
  }
  // Scripted threat marks and tracers; no combat simulation.
  for (int i = 0; i < 7; ++i) {
    const float z = -9.5F + static_cast<float>(i) * .77F + std::fmod(t, 1.F) * .77F;
    effect(f, "cube", {3.5F + (z + 9.5F) * -.27F, .17F, z}, {.035F, .035F, .15F}, {1, .17F, .07F, 1});
  }
  if (std::fmod(t, 3.F) > 1.25F) {
    effect(f, "cube", {ship.x, .8F, ship.z - 3.5F}, {.018F, .018F, 2.8F}, {.35F, 1, .83F, .9F}, {.28F, 0, 0});
  }
}
} // namespace
Matrix multiply(const Matrix& a, const Matrix& b) {
  Matrix out{};
  for (unsigned r = 0; r < 4; ++r)
    for (unsigned c = 0; c < 4; ++c)
      for (unsigned k = 0; k < 4; ++k)
        out[r * 4 + c] += a[r * 4 + k] * b[k * 4 + c];
  return out;
}
Matrix model_matrix(const StyleInstance& i) {
  const float x = i.rotation.x, y = i.rotation.y, z = i.rotation.z;
  const Matrix rx{1, 0, 0, 0, 0, std::cos(x), -std::sin(x), 0, 0, std::sin(x), std::cos(x), 0, 0, 0, 0, 1};
  const Matrix ry{std::cos(y), 0, std::sin(y), 0, 0, 1, 0, 0, -std::sin(y), 0, std::cos(y), 0, 0, 0, 0, 1};
  const Matrix rz{std::cos(z), -std::sin(z), 0, 0, std::sin(z), std::cos(z), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  Matrix m = multiply(multiply(ry, rx), rz);
  for (unsigned r = 0; r < 3; ++r) {
    m[r * 4] *= i.scale.x;
    m[r * 4 + 1] *= i.scale.y;
    m[r * 4 + 2] *= i.scale.z;
  }
  m[3] = i.position.x;
  m[7] = i.position.y;
  m[11] = i.position.z;
  return m;
}
bool bounds_visible(const Matrix& m, Vec3 minimum, Vec3 maximum, float margin) {
  std::array<bool, 6> outside{true, true, true, true, true, true};
  for (float x : {minimum.x, maximum.x})
    for (float y : {minimum.y, maximum.y})
      for (float z : {minimum.z, maximum.z}) {
        const float cx = m[0] * x + m[1] * y + m[2] * z + m[3];
        const float cy = m[4] * x + m[5] * y + m[6] * z + m[7];
        const float cz = m[8] * x + m[9] * y + m[10] * z + m[11];
        const float w = m[12] * x + m[13] * y + m[14] * z + m[15];
        outside[0] = outside[0] && cx < -margin * w;
        outside[1] = outside[1] && cx > margin * w;
        outside[2] = outside[2] && cy < -margin * w;
        outside[3] = outside[3] && cy > margin * w;
        outside[4] = outside[4] && cz < 0;
        outside[5] = outside[5] && cz > w;
      }
  return std::none_of(outside.begin(), outside.end(), [](bool value) { return value; });
}
void set_overhead_camera(StyleFrame& f, float aspect, float width, Vec3 focus, float setback) {
  f.camera = multiply(ortho(width, width / aspect, .1F, 150), view({focus.x, 29 + focus.y, focus.z + setback}, focus));
  f.light = multiply(ortho(80, 80, 1, 160), view({27, 43, -25}, {0, 0, 0}));
}
StyleFrame style_frame(float seconds, float aspect) {
  StyleFrame f;
  if (!std::isfinite(seconds) || seconds < 0)
    seconds = 0;
  if (!std::isfinite(aspect) || aspect <= 0)
    aspect = 9.F / 16;
  seconds = std::fmod(seconds, 30.F);
  f.location = std::min(static_cast<unsigned>(seconds / 10), 2U);
  const float t = std::fmod(seconds, 10.F);
  const float w = (aspect < 1 ? 19.5F : 32.F) * (1.035F - .045F * std::sin(t * pi / 10));
  f.camera = multiply(ortho(w, w / aspect, .1F, 100), view({.35F * std::sin(t * .12F), 28, 27 - .2F * t}, {0, 0, -.2F * t}));
  f.light = multiply(ortho(70, 70, 1, 120), view({27, 43, -25}, {0, 0, 0}));
  f.ring_angle = t * pi / 4;
  if (f.location == 0)
    farmland(f);
  else if (f.location == 1)
    suburbia(f);
  else
    desert(f);
  cast(f, t);
  for (auto& i : f.instances) {
    i.position.z *= 1.14F;
    if (!i.casts_shadow && !i.effect)
      i.scale.z *= 1.14F;
  }
  return f;
}
} // namespace human_resources::presentation
