#include "test_support.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <human_resources/presentation/scenery.h>
#include <iostream>
#include <map>
#include <set>
#include <string>
using namespace human_resources::presentation;
using test_support::require;
namespace {
struct Metrics {
  std::size_t frames{}, max_instances{}, max_vertices{}, sites{}, max_farmland_crowns{}, overhanging_crowns{};
  std::array<unsigned, 2> canonical_fringe_props{};
  float min_root_edge{100}, min_crown_edge{100};
  float road_min{100}, road_max{-100};
} metrics;
void farmland_opaque_clearance(const StyleInstance& i, float clearance = 6.49F) {
  // Independent conservative XZ bounds from the cooked source meshes. Flat
  // decals and tall rotor overhangs are not opaque ground footprints.
  struct Bounds final {
    std::string_view mesh;
    float x, z;
  };
  constexpr std::array<Bounds, 10> bounds{{{"cube", 1, 1}, {"barn", 2.152F, 2.051F},
      {"silo", 1.101F, 1.093F}, {"windmill-mast", .826F, .944F}, {"hay-bale", .439F, .455F},
      {"bush-low", .527F, .494F}, {"field-rock-a", .326F, .320F}, {"field-rock-b", .290F, .267F},
      {"crop-seedling", .159F, .177F}, {"grass-tuft", .127F, .153F}}};
  for (const auto& shape : bounds)
    if (i.mesh == shape.mesh) {
      const auto m = model_matrix(i);
      const float extent = std::abs(m[0]) * shape.x + std::abs(m[2]) * shape.z;
      require(std::abs(i.position.x) - extent >= clearance,
          "rotated opaque farmland props preserve the full thirteen-metre ground clearing");
    }
}
void verify_window(const SceneryQuery& q, const SceneryGeneration& g) {
  require(!g.scene.show_overlay, "scenery review excludes fake gameplay HUD");
  require(g.last_sector - g.first_sector <= 3, "generation window has bounded sectors");
  require(g.scene.instances.size() < 2400, "bounded prop population");
  require(g.scene.surfaces.size() < 32, "bounded surface materials");
  std::size_t vertices = 0;
  std::size_t crowns = 0;
  std::set<std::uint64_t> ids;
  const auto allowed = scenery_materials(q.theme);
  for (const auto& i : g.scene.instances) {
    if (q.theme == SceneryTheme::farmland)
      farmland_opaque_clearance(i);
    require(std::find(allowed.begin(), allowed.end(), i.mesh) != allowed.end(), "every generated asset preloads before rendering");
    require(i.stable_id != 0 && ids.insert(i.stable_id).second, "generated instances have unique stable identities");
    for (float v : model_matrix(i))
      require(std::isfinite(v), "finite rebased transforms");
    require(std::abs(i.position.z) <= 51.01F, "props stay in bounded local coordinates");
    // Buildings are solid core footprints. Curbs, foliage shadows and driveway
    // surfaces may border a road; houses/canopies may never occupy its tarmac.
    if (i.mesh == "tree-round-large" || i.mesh == "tree-round-small") {
      ++crowns;
      if (q.theme == SceneryTheme::farmland) {
        const auto m = model_matrix(i);
        const bool large = i.mesh == "tree-round-large";
        // The rejected whole-crown 13m rule kept actors clear by leaving only
        // slivers visible. Its ground-safety role now belongs to the measured
        // lower band; its canopy role becomes a bounded permitted overhang.
        // Shared product projection tests separately protect ship/threat/fans.
        require(i.scale.x >= 1 && i.scale.x == i.scale.y && i.scale.y == i.scale.z && i.position.y == 0 && i.rotation.x == 0 && i.rotation.z == 0,
            "source lower-band bounds cover the world lower band for grounded unpitched uniformly scaled trees");
        // Triangle-clipped source 0..0.6m band, independently rounded from
        // the HRM measurement, including edges that intersect the band plane.
        const float ground = std::abs(m[0]) * (large ? .636F : .862F) + std::abs(m[2]) * (large ? .605F : .610F);
        metrics.min_root_edge = std::min(metrics.min_root_edge, std::abs(i.position.x) - ground);
        require(std::abs(i.position.x) - ground >= 6.49F,
            "farmland lower trunk/root envelopes preserve the thirteen-metre ground clearing");
        const float extent = std::abs(m[0]) * (large ? 1.432F : 1.077F) + std::abs(m[2]) * (large ? 1.513F : .833F);
        const float crown_edge = std::abs(i.position.x) - extent;
        metrics.min_crown_edge = std::min(metrics.min_crown_edge, crown_edge);
        require(crown_edge >= 4.79F, "tall canopy overhang is bounded independently of ground clearance");
        metrics.overhanging_crowns += crown_edge < 6.49F ? 1U : 0U;
      }
      const auto r = scenery_road(q.theme, q.seed, q.distance - static_cast<double>(i.position.z));
      if (!(i.position.x + .1F < r.center - r.half_width || i.position.x - .1F > r.center + r.half_width))
        std::cerr << "tree overlap theme=" << static_cast<unsigned>(q.theme) << " seed=" << q.seed << " dist=" << q.distance << " x=" << i.position.x << " road=" << r.center << " half=" << r.half_width << '\n';
      require(i.position.x + .1F < r.center - r.half_width || i.position.x - .1F > r.center + r.half_width, "tree trunks avoid the road; canopies may overhang it");
    }
    const bool house = i.mesh == "house-coral" || i.mesh == "house-teal";
    if (house || i.mesh == "service-canopy") {
      const double s = q.distance - static_cast<double>(i.position.z);
      const float half = house ? 2.65F : 2.1F;
      for (double d = -3; d <= 3; d += .5) {
        const auto r = scenery_road(q.theme, q.seed, s + d);
        const bool left = i.position.x + half < r.center - r.half_width - .1F;
        const bool right = i.position.x - half > r.center + r.half_width + .1F;
        if (!left && !right) {
          std::cerr << "road intersection theme=" << static_cast<unsigned>(q.theme) << " seed=" << q.seed << " station=" << s << " mesh=" << i.mesh << '\n';
        }
        require(left || right, "solid building footprint clear of road");
      }
    }
  }
  for (const auto& surface : g.scene.surfaces) {
    require(std::find(allowed.begin(), allowed.end(), surface.material) != allowed.end(), "every generated surface material belongs to the theme preload");
    vertices += surface.vertices.size();
    require(surface.vertices.size() % 3 == 0, "complete surface triangles");
    for (const auto& v : surface.vertices)
      for (float f : {v.position.x, v.position.y, v.position.z, v.uv[0], v.uv[1]})
        require(std::isfinite(f), "finite road vertices and UVs");
  }
  require(vertices < 65536, "streamed geometry fits GPU upload budget");
  const auto r = scenery_road(q.theme, q.seed, q.distance);
  metrics.road_min = std::min(metrics.road_min, r.center);
  metrics.road_max = std::max(metrics.road_max, r.center);
  ++metrics.frames;
  metrics.max_instances = std::max(metrics.max_instances, g.scene.instances.size());
  metrics.max_vertices = std::max(metrics.max_vertices, vertices);
  if (q.theme == SceneryTheme::farmland)
    metrics.max_farmland_crowns = std::max(metrics.max_farmland_crowns, crowns);
  metrics.sites += g.sites.size();
}
void widened_playfield_preserves_authored_assets() {
  for (auto theme : {SceneryTheme::farmland, SceneryTheme::suburbia, SceneryTheme::desert})
    for (double station : {0., 33.6, 64., 128.}) {
      const auto original = generate_scenery({theme, 514, station});
      const auto wide = generate_scenery({theme, 514, station, 390.F / 844.F, 9.7F});
      require(original.scene.instances.size() == wide.scene.instances.size() && original.scene.surfaces.size() == wide.scene.surfaces.size(), "widened field reuses the same bounded authored population");
      for (std::size_t k = 0; k < wide.scene.instances.size(); ++k) {
        const auto& a = original.scene.instances[k];
        const auto& b = wide.scene.instances[k];
        require(a.mesh == b.mesh && a.stable_id == b.stable_id && a.scale.x == b.scale.x && a.scale.y == b.scale.y && a.scale.z == b.scale.z && a.position.z == b.position.z, "corridor widening preserves asset identity, dimensions, and station continuity");
        if (theme == SceneryTheme::farmland)
          farmland_opaque_clearance(b, 9.64F);
      }
      for (std::size_t k = 0; k < wide.scene.surfaces.size(); ++k) {
        const auto& a = original.scene.surfaces[k];
        const auto& b = wide.scene.surfaces[k];
        require(a.material == b.material && a.vertices.size() == b.vertices.size(), "ground patches retain their existing material and topology");
        for (std::size_t v = 0; v < a.vertices.size(); ++v) {
          require(a.vertices[v].position.z == b.vertices[v].position.z, "widening preserves every ground surface's longitudinal placement");
          // Existing roads/ground retain exact authored UVs. New tonal meadow
          // overlays instead match the widened base's interpolated UV at each
          // vertex, checked directly by meadow_features_match_base_ground().
          const bool meadow = a.material == "ground-grass" && a.tint != std::array<float, 4>{1, 1, 1, 1};
          if (!meadow)
            require(a.vertices[v].uv == b.vertices[v].uv, "widening preserves every existing ground texture's UVs");
        }
      }
    }
}
void farmland_grouping_and_openings() {
  for (unsigned sample = 0; sample < 49; ++sample) {
    const std::uint64_t seed = sample == 48 ? 514 : sample;
    for (double station : {0., 33.6, 64., 128.}) {
      const auto g = generate_scenery({SceneryTheme::farmland, seed, station});
      std::map<std::int64_t, unsigned> groves;
      for (const auto& site : g.sites)
        if (site.kind == SiteKind::grove)
          ++groves[site.sector];
      for (const auto& [sector, count] : groves) {
        (void)sector;
        require(count == 6, "each farmland sector has six unequal groups rather than repeated wall segments");
      }
      // Canonical sector-zero openings stay clear even when the window crosses
      // into its neighbour. Crown envelopes, not roots, must miss these spans.
      for (const auto& tree : g.scene.instances) {
        farmland_opaque_clearance(tree);
        const bool large = tree.mesh == "tree-round-large";
        if (!large && tree.mesh != "tree-round-small")
          continue;
        const auto m = model_matrix(tree);
        const float extent = std::abs(m[8]) * (large ? 1.431F : 1.076F) + std::abs(m[10]) * (large ? 1.512F : .832F);
        const double along = station - tree.position.z;
        for (const auto opening : std::array<std::array<float, 3>, 4>{{{-1, 14.5F, 23.5F}, {1, 15.5F, 24.5F}, {-1, -17.5F, -14.5F}, {1, -16.F, -10.F}}})
          if (tree.position.x * opening[0] > 0)
            require(along + extent <= opening[1] || along - extent >= opening[2],
                "full crowns preserve canonical barn, crop, silo and windmill openings across queries");
      }
    }
  }
  require(metrics.max_instances <= 577 && metrics.max_vertices <= 4608,
      "composition stays within the preceding verified prop and surface budgets");
  require(metrics.max_farmland_crowns <= 60, "streamed crown population respects the reduced sector budget");
  require(metrics.max_instances <= 417, "overhang revision does not grow the frozen preceding composition population");
  require(metrics.overhanging_crowns > 0, "ground-safe tall overhang is exercised rather than reverting to the rejected full-crown rule");
  // Positions inside the native 17m frustum are an availability check; only the
  // targeted native comparison can judge layering and projected occlusion.
  for (const auto station : {0., 33.6}) {
    const auto g = generate_scenery({SceneryTheme::farmland, 514, station});
    unsigned count = 0;
    for (const auto& prop : g.scene.instances) {
      const bool understory = prop.mesh == "bush-low" || prop.mesh == "field-rock-a" || prop.mesh == "field-rock-b" || prop.mesh == "grass-tuft";
      if (understory && std::abs(prop.position.x) <= 8.1F && std::abs(prop.position.z) < 22)
        ++count;
    }
    metrics.canonical_fringe_props[station == 0 ? 0U : 1U] = count;
    require(count >= 6, "canonical 17m windows contain low dressing inside the visible fringe");
  }
}
void meadow_features_match_base_ground() {
  for (const float clearance : {6.55F, 9.7F, 12.F})
    for (const double station : {0., 31.99, 32.01, 543.99, 544.01, 100000032.}) {
      const auto g = generate_scenery({SceneryTheme::farmland, 514, station, 390.F / 844.F, clearance});
      const auto& base = g.scene.surfaces.front();
      const auto a = base.vertices[0], b = base.vertices[1], c = base.vertices[2];
      std::size_t compared = 0;
      for (const auto& surface : g.scene.surfaces) {
        if (surface.material != "ground-grass" || surface.tint == base.tint)
          continue;
        for (const auto& vertex : surface.vertices) {
          const float u = std::lerp(a.uv[0], b.uv[0], (vertex.position.x - a.position.x) / (b.position.x - a.position.x));
          const float v = std::lerp(a.uv[1], c.uv[1], (vertex.position.z - a.position.z) / (c.position.z - a.position.z));
          test_support::require_near(vertex.uv[0], u, .00002F, "tonal meadow samples the same source feature as the base after lateral widening");
          test_support::require_near(vertex.uv[1], v, .00002F, "tonal meadow samples the same source feature through station/UV rebases");
          ++compared;
        }
      }
      require(compared > 0, "shared ground-feature checks exercise actual tonal meadow geometry");
    }
}
void moving_window_continuity() {
  for (auto theme : {SceneryTheme::farmland, SceneryTheme::suburbia, SceneryTheme::desert})
    for (double s : {-32.01, 0., 31.99, 63.99, 95.99, 127.99, 159.99, 543.99, 100000031.99}) {
      const auto a = generate_scenery({theme, 514, s}), b = generate_scenery({theme, 514, s + .04});
      std::map<std::uint64_t, StyleInstance> next;
      for (const auto& i : b.scene.instances)
        next.emplace(i.stable_id, i);
      std::size_t compared = 0;
      for (const auto& i : a.scene.instances)
        if (std::abs(i.position.z) < 26) {
          const auto found = next.find(i.stable_id);
          require(found != next.end(), "visible objects never pop at sector boundaries");
          const auto& j = found->second;
          require(i.mesh == j.mesh && i.position.x == j.position.x && i.scale.x == j.scale.x && i.rotation.y == j.rotation.y, "world placement remains stationary while scrolling");
          test_support::require_near(j.position.z - i.position.z, .04F, .00002F, "all visible props scroll by exactly camera delta");
          ++compared;
        }
      require(compared > 10, "continuity checks exercise real visible scenery");
      for (const auto& surface : a.scene.surfaces)
        if (surface.material == "ground-grass" || surface.material == "ground-sand") {
          const auto& target = b.scene.surfaces.front();
          // A floating UV origin shifts by a whole number of tile repeats (480/6).
          const auto& v = surface.vertices[0];
          const auto& w = target.vertices[0];
          const double abs_a = s - v.position.z, abs_b = s + .04 - w.position.z;
          const float expected = static_cast<float>((abs_b - abs_a) / 6);
          const float diff = w.uv[1] - v.uv[1] - expected;
          require(std::abs(diff - std::round(diff)) < .0001F, "ground UVs remain world anchored through origin rebasing");
        }
      // Tonal meadow margins must remain part of the same world floor. Test
      // their actual shared vertices through moving windows and UV rebases,
      // independently of the main large base quad's changing window edges.
      if (theme == SceneryTheme::farmland)
        for (const auto& surface : a.scene.surfaces) {
          if (surface.material != "ground-grass" || surface.tint == std::array<float, 4>{1, 1, 1, 1})
            continue;
          const auto target = std::find_if(b.scene.surfaces.begin(), b.scene.surfaces.end(), [&](const auto& value) {
            return value.material == surface.material && value.tint == surface.tint;
          });
          require(target != b.scene.surfaces.end(), "visible meadow tones persist across generation windows");
          unsigned matched = 0;
          for (const auto& vertex : surface.vertices) {
            require(vertex.position.y < 0, "meadow variation remains below roads, actors and warnings");
            if (std::abs(vertex.position.z) > 26)
              continue;
            const auto next_vertex = std::find_if(target->vertices.begin(), target->vertices.end(), [&](const auto& value) {
              return value.position.x == vertex.position.x && std::abs(value.position.z - vertex.position.z - .04F) < .00002F;
            });
            require(next_vertex != target->vertices.end(), "visible meadow shape does not pop or slide at sector boundaries");
            const float u_delta = next_vertex->uv[0] - vertex.uv[0], v_delta = next_vertex->uv[1] - vertex.uv[1];
            require(std::abs(u_delta) < .00001F && std::abs(v_delta - std::round(v_delta)) < .0001F,
                "identical meadow features retain their source UV through floating-origin changes");
            ++matched;
          }
          require(matched > 0, "meadow continuity exercises real visible ground vertices");
        }
    }
}
void road_continuity_and_variety() {
  for (auto theme : {SceneryTheme::farmland, SceneryTheme::suburbia, SceneryTheme::desert}) {
    float min_x = 100, max_x = -100, min_w = 100, max_w = -100;
    for (int i = 0; i < 600; ++i) {
      const double s = static_cast<double>(i) * 1.3;
      const auto a = scenery_road(theme, 514, s), b = scenery_road(theme, 514, s + .0001);
      require(std::abs(a.center - b.center) < .001F && std::abs(a.half_width - b.half_width) < .001F, "road has no section seams");
      min_x = std::min(min_x, a.center);
      max_x = std::max(max_x, a.center);
      min_w = std::min(min_w, a.half_width);
      max_w = std::max(max_w, a.half_width);
    }
    require(max_x - min_x > 1.7F, "road silhouette varies across a long generated run");
    if (theme == SceneryTheme::suburbia)
      require(max_w - min_w > 1.7F, "suburban street includes narrows and broad bays");
    for (int section = -3; section < 8; ++section) {
      const double join = section * 64. + 32;
      const auto before = scenery_road(theme, 514, join - .001), after = scenery_road(theme, 514, join + .001);
      require(std::abs(before.center - after.center) < .001F && std::abs(before.half_width - after.half_width) < .001F, "authored section ports share position and width");
      require(std::abs(before.slope - after.slope) < .002F, "road heading is continuous through authored joins");
    }
  }
}
void reference_landmarks_are_visible() {
  const auto portrait = generate_scenery({SceneryTheme::farmland, 514, 0, .9999F});
  const auto landscape = generate_scenery({SceneryTheme::farmland, 514, 0, 1.0001F});
  for (std::size_t i = 0; i < portrait.scene.camera.size(); ++i)
    require(std::abs(portrait.scene.camera[i] - landscape.scene.camera[i]) < .001F, "camera scale changes continuously across portrait/landscape resize");
  const auto suburb = generate_scenery({SceneryTheme::suburbia, 514, 0});
  unsigned visible_homes = 0;
  for (const auto& instance : suburb.scene.instances)
    if (instance.mesh == "house-coral" || instance.mesh == "house-teal") {
      // A road-clear house can still fail the composition by sitting entirely
      // outside the reference camera. Require substantial roof frontage.
      if (std::abs(instance.position.z) < 29 && std::abs(instance.position.x) - 2.5F < 9.75F)
        ++visible_homes;
    }
  require(visible_homes >= 5, "canonical street reveals its homes rather than only hedges/mailboxes");
  const auto desert = generate_scenery({SceneryTheme::desert, 514, 0});
  bool visible_service = false;
  for (const auto& instance : desert.scene.instances)
    if (instance.mesh == "service-canopy")
      visible_service |= std::abs(instance.position.z) < 26 && instance.position.x + 2.F > -9.5F;
  require(visible_service, "canonical desert reveals a service forecourt beside the lower bend");
}
void neighbouring_compositions_differ() {
  for (std::uint64_t seed = 0; seed < 48; ++seed)
    for (auto theme : {SceneryTheme::farmland, SceneryTheme::suburbia, SceneryTheme::desert})
      for (int section = -8; section < 12; ++section) {
        float difference = 0;
        for (double offset : {-24., -16., -8., 0., 8., 16., 24.}) {
          const auto a = scenery_road(theme, seed, section * 64. + offset);
          const auto b = scenery_road(theme, seed, (section + 1) * 64. + offset);
          difference += std::abs(a.center - b.center) + std::abs(a.half_width - b.half_width);
        }
        require(difference > 1.F, "adjacent sections do not repeat the same road composition");
      }
}
} // namespace
int main(int argc, char** argv) {
  widened_playfield_preserves_authored_assets();
  meadow_features_match_base_ground();
  for (std::uint64_t seed = 0; seed < 48; ++seed)
    for (auto theme : {SceneryTheme::farmland, SceneryTheme::suburbia, SceneryTheme::desert})
      for (double s : {0., 16., 31.99, 32.01, 64., 128., 257., 543.99, 544.01, 1024., 65536., 100000032.}) {
        const SceneryQuery q{theme, seed, s};
        const auto g = generate_scenery(q);
        verify_window(q, g);
      }
  moving_window_continuity();
  road_continuity_and_variety();
  reference_landmarks_are_visible();
  neighbouring_compositions_differ();
  farmland_grouping_and_openings();
  if (argc == 2) {
    std::ofstream out(argv[1]);
    out << "{\"windows\":" << metrics.frames << ",\"max_instances\":" << metrics.max_instances << ",\"max_surface_vertices\":" << metrics.max_vertices << ",\"max_farmland_crowns\":" << metrics.max_farmland_crowns
        << ",\"min_root_edge\":" << metrics.min_root_edge << ",\"min_crown_edge\":" << metrics.min_crown_edge << ",\"overhang_samples\":" << metrics.overhanging_crowns
        << ",\"canonical_fringe_props\":[" << metrics.canonical_fringe_props[0] << ',' << metrics.canonical_fringe_props[1] << "],\"sites_checked\":" << metrics.sites << ",\"result\":\"pass\"}\n";
    require(out.good(), "report saved");
  }
  std::cout << metrics.frames << " generated windows checked; max " << metrics.max_instances << " props, " << metrics.max_vertices << " surface vertices\n";
}
