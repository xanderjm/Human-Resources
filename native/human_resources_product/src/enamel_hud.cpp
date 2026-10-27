#include "enamel_hud.h"
#include "enamel_components.h"
#include "thumbstick.h"
#include <human_resources/gameplay/tuning.h>

#include <algorithm>
#include <cmath>

namespace human_resources::product {
namespace {
namespace e=enamel;
using detail::number;
using e::short_number;
std::string count(float value) {return short_number(static_cast<std::uint64_t>(std::max(0.F,value)));}
void flight_status(e::Canvas& c,const View& v) {
  const auto& run=*v.run;
  const bool boost=run.harvest_overdrive_seconds>0,hit=v.damage_feedback.age>=0;
  const bool intro=run.stats.active_seconds<12 && !boost && !run.harvest_chain && !hit;
  if(intro) {
    e::panel(c,e::Sprite::navy_panel,{8,620,374,62},13);
    const bool keys=v.hint_device==input::DeviceKind::keyboard,pad=v.hint_device==input::DeviceKind::gamepad;
    const auto move=keys?"WASD / ARROWS TO FLY":pad?"LEFT STICK TO FLY":"DRAG TO FLY";
    const auto dodge=keys?"SPACE TO DODGE":pad?"RIGHT FACE BUTTON TO DODGE":"FLICK TO DASH";
    e::label(c,{16,632,358,23},run.stats.active_seconds<4?move:"KEEP MOVING THROUGH TARGETS",{17,e::cream,e::Font::body,true});
    e::label(c,{16,657,358,24},run.stats.active_seconds<4?dodge:"3 SWEEPS = OVERDRIVE",{14,e::cream,e::Font::prose,true});
    return;
  }
  e::panel(c,e::Sprite::navy_panel,{10,590,178,80},13);
  if(!boost)e::icon(c,{20,600,26,25},e::Sprite::beam);
  const auto text=boost?"OVERDRIVE "+number(static_cast<unsigned>(std::ceil(run.harvest_overdrive_seconds)))+"s":
      "SWEEP "+number(run.harvest_chain)+" / "+number(gameplay::tuning::harvest_chain_target);
  e::label(c,{boost?22.F:51.F,602,boost?154.F:125.F,23},text,{18,boost?e::active:e::cream,e::Font::body});
  if(hit) {
    const auto cause=v.damage_feedback.cause==gameplay::DamageCause::contact?"BUMP":v.damage_feedback.cause==gameplay::DamageCause::projectile?"SHOT":"HIT";
    e::label(c,{22,633,154,24},std::string(v.damage_feedback.hull?"HP ":"SHIELD ")+cause,{14,v.damage_feedback.hull?e::accent:e::cream,e::Font::prose});
  } else e::pips(c,{51,633,104,12},boost?gameplay::tuning::harvest_chain_target:run.harvest_chain,gameplay::tuning::harvest_chain_target);
  const float fraction=boost?run.harvest_overdrive_seconds/gameplay::tuning::harvest_overdrive_seconds:
      run.harvest_chain_seconds/gameplay::tuning::harvest_chain_window_seconds;
  e::progress(c,{22,658,154,6},fraction);
}
} // namespace
std::optional<Interface> enamel_hud(const View& v,const GameplayFraming& framing) {
  if(v.screen!=Screen::run || v.save_error || v.text_scale>1 || framing.compact ||
      v.viewport.width>=v.viewport.height || framing.safe.width<300)return std::nullopt;
  e::Canvas c(v);
  // Top pods use physical points, so their minimum type and controls do not
  // inherit a tall phone's world/composition scale. The bottom action keeps
  // the same portrait component grid as the other screens.
  const auto original_scale=c.scale,original_x=c.ox,original_y=c.oy;
  c.scale=c.type_scale=1;c.ox=c.oy=0;
  const auto& run=*v.run;
  const auto safe=c.safe;
  const bool short_phone=safe.height<650;
  const float left=safe.x+4,top=safe.y+(short_phone?2.F:8.F),pause_x=safe.right()-52;
  const float usable=pause_x-8-left,gap=7,defence=usable*.44F,progress=usable*.29F;
  const float progress_x=left+defence+gap,score_x=progress_x+progress+gap,score_width=pause_x-8-score_x;
  const float height=short_phone?50.F:78.F;
  e::panel(c,e::Sprite::navy_panel,{left,top,defence,height},12);
  e::panel(c,e::Sprite::navy_panel,{progress_x,top,progress,height},12);
  e::panel(c,e::Sprite::navy_panel,{score_x,top,score_width,height},12);
  const auto defence_row=[&](float y,std::string_view label,float value,float maximum,e::Color color) {
    e::label(c,{left+8,y,56,21},label,{14,e::cream,e::Font::body});
    const float value_x=label=="HP"?left+34:left+68;
    const float value_width=left+defence-8-value_x;
    auto text=count(value)+"/"+count(maximum);
    if(e::measure(c,text,14,e::Font::body)>value_width)text=count(value);
    e::label(c,{value_x,y,value_width,23},text,{short_phone?18.F:21.F,color,e::Font::body,true});
    e::progress(c,{left+8,y+(short_phone?16.F:23.F),defence-16,short_phone?4.F:5.F},value/std::max(1.F,maximum),color);
  };
  defence_row(top+(short_phone?5.F:9.F),"HP",run.hull,run.maximum_hull,v.damage_feedback.age>=0&&v.damage_feedback.hull?e::accent:e::active);
  defence_row(top+(short_phone?27.F:43.F),"SHIELD",run.shield,run.maximum_shield,{.57F,.83F,1,1});
  e::label(c,{progress_x+8,top+(short_phone?5.F:10.F),23,22},"LV",{14,e::cream,e::Font::body});
  e::label(c,{progress_x+36,top+(short_phone?3.F:8.F),progress-44,27},short_number(run.stats.level),{short_phone?21.F:25.F,e::active,e::Font::body,true});
  e::progress(c,{progress_x+8,top+(short_phone?22.F:37.F),progress-16,short_phone?4.F:7.F},run.xp/std::max(1.F,run.next_level_xp),detail::gold);
  auto xp=count(run.xp)+" / "+count(run.next_level_xp)+" XP";
  if(e::measure(c,xp,14,e::Font::body)>progress-16)xp="XP";
  e::label(c,{progress_x+8,top+(short_phone?31.F:53.F),progress-16,19},xp,{14,e::cream,e::Font::body,true});
  e::label(c,{score_x+6,top+(short_phone?5.F:12.F),score_width-12,20},"SCORE",{14,e::cream,e::Font::body,true});
  e::label(c,{score_x+6,top+(short_phone?25.F:39.F),score_width-12,29},short_number(run.stats.score),{short_phone?20.F:25.F,e::cream,e::Font::body,true});
  e::action_panel(c,{pause_x,top,44,44},"PAUSE",Command::pause,e::Sprite::secondary);
  e::icon(c,{pause_x+10,top+9,24,25},e::Sprite::pause);
  c.output.hud_bottom=top+height;
  c.scale=original_scale;c.ox=original_x;c.oy=original_y;
  flight_status(c,v);
  const bool ready=run.dash_cooldown_seconds<=0;
  const float dash_y=short_phone?744.F:700.F,dash_height=short_phone?76.F:104.F,badge_y=short_phone?812.F:793.F;
  e::button(c,{214,dash_y,170,dash_height},"DASH",Command::dash,e::ButtonStyle::primary,ready);
  const auto status=ready?"READY":number(static_cast<unsigned>(std::ceil(run.dash_cooldown_seconds)))+"s";
  c.output.buttons.back().label=ready?"DASH":"DASH "+status;
  e::panel(c,e::Sprite::navy_panel,{245,badge_y,108,28},9);
  e::label(c,{251,badge_y+6,96,24},status,{16,ready?e::active:e::cream,e::Font::body,true});
  detail::append_thumbstick(c.output,v);
  if(c.output.text_overflow)return std::nullopt;
  return std::move(c.output);
}
} // namespace human_resources::product
