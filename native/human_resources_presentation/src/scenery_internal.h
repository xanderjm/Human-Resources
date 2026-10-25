#pragma once
#include <array>
#include <human_resources/presentation/scenery.h>
namespace human_resources::presentation::scenery_detail {
constexpr double sector_length = 64;
constexpr float pi = 3.14159265358979323846F;
using Color = std::array<float, 4>;
[[nodiscard]] std::uint64_t key(std::uint64_t seed, std::int64_t index, std::uint64_t channel);
[[nodiscard]] float random(std::uint64_t seed, std::int64_t index, std::uint64_t channel);
struct Builder final {
  SceneryQuery query;
  SceneryGeneration generation;
  double uv_origin{};
  std::uint64_t host{}, part{};
  std::int64_t sector{};
  explicit Builder(SceneryQuery value);
  float local(double along) const;
  std::uint64_t site(SiteKind kind, float x, double along, float radius, std::uint64_t channel);
  void prop(std::string_view mesh, float x, double along, float scale = 1, float yaw = 0, float y = 0, Color tint = {1, 1, 1, 1});
  void box(float x, double along, float y, Vec3 half, Color tint, float yaw = 0);
  StyleSurface& surface(std::string_view material, Color tint = {1, 1, 1, 1}, float detail = .24F);
  void quad(StyleSurface& surface, std::array<Vec3, 4> points, std::array<std::array<float, 2>, 4> uv);
  void patch(std::string_view material, float x, double along, float half_x, float half_z, float yaw = 0, float height = .006F, Color tint = {1, 1, 1, 1});
  void path(std::string_view material, float x0, double s0, float x1, double s1, float width, float height = .02F);
  void understory(float x, double s, bool desert, float spread = 1);
};
void roads(Builder& b);
void farmland_sector(Builder& b);
void suburb_sector(Builder& b);
void desert_sector(Builder& b);
} // namespace human_resources::presentation::scenery_detail
