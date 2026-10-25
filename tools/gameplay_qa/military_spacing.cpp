#include <algorithm>
#include <cmath>
#include <fstream>
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/gameplay/tuning.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
namespace gp = human_resources::gameplay;
gp::Control policy(const gp::Snapshot& r, bool greedy) {
  if (!greedy)
    return {};
  gp::Control c;
  const gp::EntityView* target = nullptr;
  float best = 1.e9F;
  for (const auto& e : r.entities)
    if (e.kind < gp::EntityKind::soldier) {
      float d = std::hypot(e.position.x - r.player.x, e.position.z - r.player.z);
      if (d < best) {
        best = d;
        target = &e;
      }
    }
  if (target) {
    const float s = std::max(1.F, r.movement_speed);
    c.movement = {(target->position.x - r.player.x) * 2 / s, ((target->position.z - r.player.z) * 2 - r.forward_speed) / s};
  }
  bool danger = false;
  for (const auto& e : r.entities)
    if (e.warning_seconds > 0 && std::hypot(e.position.x - r.player.x, e.position.z - r.player.z) < 7)
      danger = true;
  for (const auto& p : r.projectiles)
    if (std::hypot(p.position.x - r.player.x, p.position.z - r.player.z) < 3)
      danger = true;
  c.dash = r.dash_cooldown_seconds <= 0 && (danger || (r.stats.active_seconds >= 4.5 && r.stats.active_seconds < 4.55));
  return c;
}
int main(int argc, char** argv) {
  if (argc != 2)
    return 2;
  std::ofstream events(std::string(argv[1]) + "-pairs.csv"), out(std::string(argv[1]) + "-summary.csv");
  events << "seed,policy,second,a,b,kind_a,kind_b,distance,contact_sum,ax,az,bx,bz,warning_a,warning_b,hull_a,hull_b\n";
  events << std::setprecision(9);
  out << "seed,policy,seconds,captures,level,close_pair_frames,coincident_pair_frames,worst_distance,heavy_pair_frames,body_overlap_pair_frames\n";
  for (unsigned seed : {1U, 514U, 7841U})
    for (bool greedy : {false, true}) {
      gp::Game g;
      g.start({.seed = seed});
      unsigned overlap = 0, zero = 0, heavy = 0, body_overlaps = 0;
      float closest = 1000;
      bool saved = false;
      for (unsigned tick = 0; tick < 36000 && g.snapshot().phase != gp::Phase::dead; ++tick) {
        if (g.snapshot().phase == gp::Phase::choosing_upgrade) {
          if (!g.choose_upgrade(0))
            return 3;
          continue;
        }
        g.advance(1.F / 60, policy(g.snapshot(), greedy));
        const auto& s = g.snapshot();
        for (std::size_t i = 0; i < s.entities.size(); ++i) {
          const auto& a = s.entities[i];
          if (a.kind < gp::EntityKind::soldier || a.capture_progress > 0)
            continue;
          for (std::size_t j = i + 1; j < s.entities.size(); ++j) {
            const auto& b = s.entities[j];
            if (b.kind < gp::EntityKind::soldier || b.capture_progress > 0)
              continue;
            const float radius[]{.75F, 1.35F, 1.7F, 2.F};
            float d = std::hypot(a.position.x - b.position.x, a.position.z - b.position.z), sum = radius[static_cast<unsigned>(a.kind) - 4] + radius[static_cast<unsigned>(b.kind) - 4];
            closest = std::min(closest, d);
            body_overlaps += d + .001F < gp::tuning::military_body_radius[static_cast<unsigned>(a.kind) - 4] + gp::tuning::military_body_radius[static_cast<unsigned>(b.kind) - 4];
            if (d < sum) {
              ++overlap;
              heavy += (a.kind > gp::EntityKind::soldier && b.kind > gp::EntityKind::soldier);
              zero += d < .001F;
              if (tick % 30 == 0 || d < .001F)
                events << seed << ',' << greedy << ',' << s.stats.active_seconds << ',' << a.id << ',' << b.id << ',' << static_cast<unsigned>(a.kind) << ',' << static_cast<unsigned>(b.kind) << ',' << d << ',' << sum << ',' << a.position.x << ',' << a.position.z << ',' << b.position.x << ',' << b.position.z << ',' << a.warning_seconds << ',' << b.warning_seconds << ',' << a.hull << ',' << b.hull << '\n';
              if (!saved && seed == 514 && a.kind == gp::EntityKind::jeep && b.kind == gp::EntityKind::jeep && d < .01F) {
                auto bytes = g.save_state();
                std::ofstream f(std::string(argv[1]) + "-actual-overlap.hrs", std::ios::binary);
                f.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                saved = true;
              }
            }
          }
        }
      }
      const auto& s = g.snapshot();
      out << seed << ',' << greedy << ',' << s.stats.active_seconds << ',' << s.stats.captures << ',' << s.stats.level << ',' << overlap << ',' << zero << ',' << closest << ',' << heavy << ',' << body_overlaps << '\n';
    }
}
