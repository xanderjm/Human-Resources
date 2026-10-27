#include <human_resources/product/gameplay_framing.h>
#include "test_support.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace hr = human_resources;
using test_support::require;
using test_support::require_near;
namespace {
hr::ui::Point project(const hr::product::GameplayFraming& framing, float x, float station) {
  const auto& m = framing.camera;
  const float z = 2.3F - station;
  const float w = m[12]*x + m[13]*2.5F + m[14]*z + m[15];
  return {(m[0]*x + m[1]*2.5F + m[2]*z + m[3])/w,
          (m[4]*x + m[5]*2.5F + m[6]*z + m[7])/w};
}
void every_display_exposes_all_four_edges() {
  for (const auto viewport : {hr::ui::Rect{0,0,320,568}, {0,0,390,844}, {17,29,402,874},
       {0,0,768,1024}, {0,0,1024,768}, {0,0,844,390}, {0,0,320,320}, {0,0,2560,320}}) {
    for (float text : {1.F,2.F,3.4F}) for (unsigned dpi : {1U,2U,3U}) {
      const auto framing = hr::product::gameplay_framing(viewport,{12,44,12,34},text,
          {static_cast<unsigned>(viewport.width)*dpi,static_cast<unsigned>(viewport.height)*dpi});
      const auto area = framing.flight_bounds;
      require(area.left < 0 && area.right > 0 && area.backward < 0 && area.forward > 0,
          "phone/tablet/wide/short displays publish a valid full flight area");
      require(framing.stage.width >= viewport.width-4 && framing.stage.height >= viewport.height-4,
          "world fills the entire output instead of reserving a portrait-only field");
      for (float x : {area.left,area.right}) for (float z : {area.backward,area.forward}) {
        const auto clip = project(framing,x,z);
        require_near(std::abs(clip.x),1,1e-5F,"ship centre reaches left and right camera edges, allowing half the saucer outside");
        require_near(std::abs(clip.y),1,1e-5F,"ship centre reaches top and bottom camera edges at every DPI/text size");
      }
    }
  }
}
void actual_flight_reaches_each_edge_and_survives_cold_resize() {
  for (const auto viewport : {hr::ui::Rect{0,0,320,568},{0,0,390,844},{0,0,768,1024},{0,0,844,390}}) {
    const auto framing = hr::product::gameplay_framing(viewport);
    const auto area = framing.flight_bounds;
    for (const hr::gameplay::Vec2 direction : {hr::gameplay::Vec2{1,0},{-1,0},{0,1},{0,-1}}) {
      hr::gameplay::Game game;
      game.start({.seed=514,.permanent_ranks={5,5,5,5}});
      bool reached = false;
      for (unsigned tick=0;tick<1500 && !reached;++tick) {
        if (game.snapshot().phase == hr::gameplay::Phase::choosing_upgrade)
          require(game.choose_upgrade(0),"edge flight selects an actual earned offer");
        game.advance(1.F/60,{direction,false,{},area});
        const auto& run = game.snapshot();
        const float station = run.player.z-run.runner_distance;
        require(run.player.x >= area.left-.001F && run.player.x <= area.right+.001F &&
            station >= area.backward-.001F && station <= area.forward+.001F,"real movement stays within the supplied visible plane");
        reached = direction.x > 0 ? std::abs(run.player.x-area.right)<.001F :
            direction.x < 0 ? std::abs(run.player.x-area.left)<.001F :
            direction.z > 0 ? std::abs(station-area.forward)<.001F : std::abs(station-area.backward)<.001F;
      }
      require(reached,"real public gameplay reaches every edge on phone/tablet/wide outputs");
      const auto saved = game.save_state();
      hr::gameplay::Game cold;
      require(!saved.empty() && cold.restore_state(saved)==hr::gameplay::SaveStateIssue::none && cold.save_state()==saved,
          "accepted device flight area cold restores exactly");
      game.pause();const auto held=game.save_state();game.advance(10,{{},true,{},area});
      require(game.save_state()==held,"pause/background never consumes or changes device flight area");game.resume();
      // A narrower new display clamps to its own visible edge, never leaves a
      // restored ship stranded beyond the new viewport with input unable to recover it.
      const auto resized=hr::product::gameplay_framing({0,0,390,844}).flight_bounds;
      cold.advance(1.F/60,{{},false,{},resized});
      require(cold.snapshot().player.x>=resized.left && cold.snapshot().player.x<=resized.right,
          "cold flight adopts a new device's horizontal bounds on its first active tick");
    }
  }
}
void malformed_display_area_cannot_poison_flight() {
  hr::gameplay::Game game;game.start();
  const hr::gameplay::FlightBounds bad{std::numeric_limits<float>::quiet_NaN(),1000,2,-2};
  for(unsigned tick=0;tick<120;++tick) game.advance(1.F/60,{{1,0},false,{},bad});
  require(game.snapshot().player.x<=7.5F && !game.save_state().empty(),"invalid area preserves canonical finite gameplay bounds");
}
}
int main() {
  every_display_exposes_all_four_edges();
  actual_flight_reaches_each_edge_and_survives_cold_resize();
  malformed_display_area_cannot_poison_flight();
}
