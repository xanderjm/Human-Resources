#include "enamel_screens.h"
#include "enamel_components.h"
#include "flight_report.h"
#include "enamel_menus.h"
#include "enamel_flight_screens.h"
#include "enamel_layout.h"

#include <algorithm>
#include <cmath>

namespace human_resources::product {
namespace {
namespace e = enamel;
using detail::Canvas;
using detail::number;
using detail::time_text;

void home(Canvas& c, const View& v) {
  const auto& account = v.store->account();
  e::panel(c,e::Sprite::navy_panel,{22,14,158,65},10);
  e::icon(c,{30,23,30,30},e::Sprite::credit);
  const auto credits=number(account.credits);
  e::label(c,{66,22,103,29},e::measure(c,credits,14)>103?e::short_number(account.credits):credits,{25,e::cream});
  e::label(c,{30,54,142,21},e::measure(c,"RESEARCH CREDITS",14)>142?"RESEARCH CR":"RESEARCH CREDITS",{14,e::active});
  e::blueprint_progress(c,{222,19,146,64},*v.store);
  if(e::compact(c))e::icon(c,{25,86,340,166},e::Sprite::logo);
  else e::picture(c,e::Sprite::logo,{25,86,340,192});
  const float selector=e::compact(c)?e::height(c)-294:550;
  e::panel(c,e::Sprite::secondary,{46,selector,298,64},25);
  e::icon(c,{89,selector+11,42,42},e::Sprite::farmland);
  std::string location(location_name(v.theme));
  std::string uppercase = location;
  for (auto& ch : uppercase) if (ch>='a' && ch<='z') ch=static_cast<char>(ch-'a'+'A');
  e::label(c,{135,selector+18,154,30},uppercase,{25,e::ink,e::Font::display,true});
  e::button(c,{18,selector+66,354,128},"START RUN",Command::start,e::ButtonStyle::primary);
  e::button(c,{48,selector+8,44,48},"",Command::location_previous,e::ButtonStyle::navy);
  c.output.buttons.back().label="Previous location: "+location;
  e::icon(c,{58,selector+20,24,24},e::Sprite::arrow_left);
  e::button(c,{298,selector+8,44,48},"",Command::location_next,e::ButtonStyle::navy);
  c.output.buttons.back().label="LOCATION: "+location+" >";
  e::icon(c,{308,selector+20,24,24},e::Sprite::arrow_right);
  e::button(c,{28,selector+204,150,58},"HANGAR",Command::hangar,e::ButtonStyle::navy);
  e::button(c,{212,selector+204,150,58},"SETTINGS",Command::settings,e::ButtonStyle::navy);
}

void pause(Canvas& c, const View& v) {
  const auto& run = *v.run;
  const auto bank = pause_bank_preview(v);
  c.fill(-1000,-1000,3000,3000,{.015F,.045F,.09F,.48F});
  if(e::compact(c)) {
    const float header=std::clamp(e::height(c)-552,60.F,116.F);
    const float location=header+22,resume=location+84;
    const float resume_height=e::height(c)<650?80.F:100.F;
    const float build=resume+resume_height+10,settings=build+64,bank_y=settings+66;
    e::heading(c,{24,12,342,header},v.restored_run?"RUN SAVED":"ON HOLD",true);
    e::panel(c,e::Sprite::cream_panel,{24,location,342,72},14);
    e::icon(c,{35,location+12,44,44},e::Sprite::farmland);
    e::label(c,{94,location+10,258,27},std::string(location_name(v.theme))+" / LV "+number(run.stats.level),{20,e::ink});
    e::label(c,{94,location+39,258,23},time_text(run.stats.active_seconds)+" / SCORE "+number(run.stats.score),{16,e::ink});
    e::button(c,{30,resume,330,resume_height},v.restored_run?"RESUME FLIGHT":"RESUME",Command::resume,e::ButtonStyle::primary);
    e::button(c,{52,build,286,54},"CURRENT BUILD",Command::current_build);
    e::button(c,{52,settings,286,54},"SETTINGS",Command::settings);
    e::panel(c,e::Sprite::navy_panel,{34,bank_y,322,90},14);
    e::label(c,{48,bank_y+9,294,27},bank.heading,{21,e::active});
    e::paragraph(c,{48,bank_y+41,294,41},bank.detail,14,e::cream);
    e::button(c,{52,bank_y+100,286,54},"END RUN & BANK",Command::finish);
    if(v.restored_run && run.phase==gameplay::Phase::choosing_upgrade) {
      e::panel(c,e::Sprite::cream_panel,{34,bank_y+164,322,40},12);
      e::label(c,{48,bank_y+174,294,25},"YOUR EARNED UPGRADE IS WAITING",{14,e::ink,e::Font::body,true});
    }
    return;
  }
  e::heading(c,{24,30,342,158},v.restored_run?"RUN SAVED":"ON HOLD");
  e::panel(c,e::Sprite::cream_panel,{24,184,342,84},14);
  e::icon(c,{35,200,48,48},e::Sprite::farmland);
  e::label(c,{94,197,258,27},std::string(location_name(v.theme))+" / LV "+number(run.stats.level),{20,e::ink});
  e::label(c,{94,225,258,23},time_text(run.stats.active_seconds)+" / SCORE "+number(run.stats.score),{16,e::ink});
  e::button(c,{30,286,330,134},v.restored_run?"RESUME FLIGHT":"RESUME",Command::resume,e::ButtonStyle::primary);
  e::button(c,{52,426,286,54},"CURRENT BUILD",Command::current_build);
  e::button(c,{52,496,286,54},"SETTINGS",Command::settings);
  e::panel(c,e::Sprite::navy_panel,{34,565,322,94},14);
  e::label(c,{48,578,294,30},bank.heading,{23,e::active});
  c.wrap(48,612,294,bank.detail,14,e::cream,3,1.1F);
  e::button(c,{52,684,286,56},"END RUN & BANK",Command::finish);
  if (v.restored_run && run.phase==gameplay::Phase::choosing_upgrade) {
    e::panel(c,e::Sprite::cream_panel,{34,758,322,52},12);
    e::label(c,{48,770,294,30},"YOUR EARNED UPGRADE IS WAITING",{14,e::ink,e::Font::body,true});
  }
}
bool reachable(const Canvas& c) {
  for (std::size_t i=0; i<c.output.buttons.size(); ++i) {
    const auto& box=c.output.buttons[i].bounds;
    if (box.x<c.safe.x || box.y<c.safe.y || box.right()>c.safe.right()+.01F || box.bottom()>c.safe.bottom()+.01F) return false;
    for (std::size_t j=i+1; j<c.output.buttons.size(); ++j) {
      const auto overlap=ui::intersect(box,c.output.buttons[j].bounds);
      if (overlap.width>.01F && overlap.height>.01F) return false;
    }
  }
  return true;
}
} // namespace
std::optional<Interface> enamel_interface(const View& v) {
  if(v.viewport.width>=v.viewport.height)return std::nullopt;
  if(v.text_scale>1)return std::nullopt;
  Canvas c(v);
  e::configure_portrait(c);
  if(enamel_menu(c,v)) {}
  else if(enamel_flight_screen(c,v)) {}
  else if (v.screen==Screen::title) home(c,v);
  else if(v.screen==Screen::pause) pause(c,v);
  else return std::nullopt;
  if (c.output.text_overflow || !reachable(c)) return std::nullopt;
  return std::move(c.output);
}
} // namespace human_resources::product
