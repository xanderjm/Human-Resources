#include "enamel_flight_screens.h"
#include "enamel_components.h"
#include "enamel_layout.h"
#include "flight_report.h"
#include "upgrade_presentation.h"

#include <algorithm>

namespace human_resources::product {
namespace {
namespace e=enamel;
using detail::Canvas;
using detail::number;
using detail::time_text;

void build(Canvas& c,const View& v) {
  const auto& run=*v.run;
  const bool compact=e::compact(c);
  const float h=e::height(c),header=compact?std::clamp(h-570.F,52.F,112.F):132.F;
  const float tabs_y=compact?header+8:211.F,tabs_height=compact?54.F:62.F;
  const float summary_y=tabs_y+tabs_height+8,rows_y=compact?summary_y+30:306.F;
  c.fill(-1000,-1000,3000,3000,{.015F,.045F,.09F,.42F});
  c.output.page_count=3;c.output.page=std::min(v.ui_page,2U);
  e::heading(c,{compact?30.F:58.F,compact?0.F:78.F,compact?330.F:274.F,header},"CURRENT\nBUILD",compact);
  constexpr std::array tabs{e::Tab{"HARDWARE",e::Sprite::beam,Command::build_hardware},
      e::Tab{"RESEARCH",e::Sprite::motion,Command::build_research},e::Tab{"HANGAR",e::Sprite::build,Command::build_hangar}};
  e::tabs(c,{22,tabs_y,346,tabs_height},tabs,c.output.page);
  constexpr std::array titles{"FLIGHT HARDWARE & EVOLUTIONS","TEMPORARY FLIGHT RESEARCH","PERMANENT HANGAR"};
  c.output.page_text=std::string(titles[c.output.page])+"\n";
  const auto line=[&](ui::Rect box,std::string text,float size=19,e::Color color=e::ink) {
    e::label(c,box,text,{size,color,e::Font::body});c.output.page_text+=text+"\n";
  };
  const float footer=compact?h-60:790.F;
  if(c.output.page==0) {
    e::panel(c,e::Sprite::secondary,{80,summary_y,230,26},12);
    line({90,summary_y+5,210,22},number(hardware_count(run))+" / 6 hardware slots",14);
    unsigned slot=0;
    const float step=compact?44.F:56.F,row_height=compact?42.F:52.F;
    for(unsigned i=0;i<run.modules.size();++i) {
      if(!run.modules[i])continue;
      const float y=rows_y+static_cast<float>(slot++)*step;
      e::panel(c,e::Sprite::cream_panel,{24,y,342,row_height},11);
      e::panel(c,e::Sprite::navy_panel,{28,y+3,92,row_height-6},9);
      e::icon(c,{43,y+6,62,row_height-12},e::hardware_icon(static_cast<gameplay::Module>(i)));
      line({130,y+6,186,23},std::string(gameplay::tuning::module_names[i]),compact?18.F:20.F);
      line({130,y+(compact?24.F:30.F),180,19},"RANK "+number(run.modules[i])+" OF 3",14,e::active);
      e::pips(c,{332,y+7,compact?8.F:10.F,row_height-14},run.modules[i],3);
    }
    for(;slot<6;++slot) {
      const float y=rows_y+static_cast<float>(slot)*step;
      e::panel(c,e::Sprite::navy_panel,{24,y,342,row_height},11,{.65F,.7F,.75F,.65F});
      e::panel(c,e::Sprite::navy_panel,{28,y+3,92,row_height-6},9,{.65F,.7F,.75F,.6F});
      c.fill(70,y+row_height*.5F-11,6,22,e::cream);c.fill(62,y+row_height*.5F-3,22,6,e::cream);
      line({130,y+(row_height-20)*.5F,218,23},"Empty hardware slot",compact?18.F:20.F,e::cream);
    }
    const float notice_y=compact?footer-56:738.F;
    const float evo_y=compact?rows_y+6*step+6:675.F;
    const float evo_height=compact?std::min(98.F,notice_y-evo_y-8):56.F;
    if(evo_height<76 && compact)c.output.text_overflow=true;
    if(!compact)e::label(c,{26,650,338,23},"ACTIVE EVOLUTIONS",{18,e::cream});
    e::panel(c,e::Sprite::navy_panel,{24,evo_y,342,evo_height},12);
    e::icon(c,{38,evo_y+10,46,evo_height-20},e::Sprite::beam_splitter);
    std::string names;
    for(const auto i:gameplay::tuning::active_evolutions)
      if(run.evolution_bits&(1U<<i))names+=(names.empty()?"":" / ")+std::string(gameplay::tuning::evolution_names[i]);
    if(compact) {
      e::label(c,{98,evo_y+8,252,23},"ACTIVE EVOLUTIONS",{16,e::active});
      if(names.empty())names="None yet. Both rank III. First two evolve automatically.";
      e::paragraph(c,{98,evo_y+32,252,evo_height-38},names,14,e::cream);
    } else if(names.empty()) {
      e::label(c,{98,evo_y+7,252,23},"NONE YET",{20,e::cream});
      e::paragraph(c,{98,evo_y+30,252,21},"Both ingredients need rank III.",14,e::cream);
      names="None yet. Both ingredients need rank III. First two recipes evolve automatically.";
    } else e::paragraph(c,{98,evo_y+9,252,evo_height-16},names,14,e::cream);
    c.output.page_text+=names+"\n";
    e::panel(c,e::Sprite::cream_panel,{24,notice_y,342,compact?48.F:40.F},11);
    e::icon(c,{34,notice_y+6,24,24},e::Sprite::info);
    const auto scope="Hardware and research last for this flight.";
    e::paragraph(c,{65,notice_y+7,287,compact?40.F:32.F},scope,14);c.output.page_text+=scope;
  } else if(c.output.page==1) {
    const auto speed="Level abduction: +"+metric_number(gameplay::level_capture_speed_bonus(run.stats.level)*100)+"% (+"+
        metric_number(gameplay::level_capture_speed_bonus(2)*100)+"%/level).";
    e::paragraph(c,{30,summary_y,330,compact?32.F:24.F},speed,14,e::cream);c.output.page_text+=speed+"\n";
    unsigned row=0;
    for(unsigned i=0;i<run.research.size();++i) {
      if(run.research[i]<=0)continue;
      const float y=rows_y+static_cast<float>(row++)*(compact?48.F:58.F);
      e::panel(c,e::Sprite::cream_panel,{24,y,342,compact?46.F:53.F},11);
      line({38,y+6,314,23},std::string(gameplay::tuning::research_names[i])+":",compact?18.F:20.F);
      const bool points=i==static_cast<unsigned>(gameplay::Research::hull_reinforcement)||i==static_cast<unsigned>(gameplay::Research::shield_capacity);
      const auto value="+"+metric_number(run.research[i])+(points?" points":i==static_cast<unsigned>(gameplay::Research::emergency_repairs)?"% immediate repair earned":"% of base");
      e::paragraph(c,{38,y+29,314,20},value,14);c.output.page_text+=value+"\n";
    }
    if(!row) {
      constexpr std::string_view empty="No research earned this flight. Choose research at a promotion.";
      e::paragraph(c,{38,rows_y+10,314,70},empty,18,e::cream);c.output.page_text+=empty;
    }
  } else {
    e::paragraph(c,{30,summary_y,330,compact?32.F:24.F},"Permanent bonuses apply to future flights.",14,e::cream);
    unsigned row=0;
    for(const auto& t:v.store->hangar()) {
      const float y=rows_y+static_cast<float>(row++)*(compact?84.F:104.F);
      e::panel(c,e::Sprite::cream_panel,{24,y,342,compact?80.F:98.F},12);
      line({38,y+8,314,28},std::string(t.title),compact?20.F:24.F);
      line({38,y+36,314,24},"RANK "+number(t.rank)+" OF "+number(t.maximum_rank),14);
      e::paragraph(c,{38,y+(compact?58.F:68.F),314,24},t.benefit,14);c.output.page_text+=t.benefit+"\n";
    }
  }
  e::button(c,{24,footer,136,compact?54.F:44.F},"BACK",Command::current_build);
  if(c.output.page<2)e::button(c,{230,footer,136,compact?54.F:44.F},"NEXT",Command::page_next);
  else e::button(c,{230,footer,136,compact?54.F:44.F},"PREVIOUS",Command::page_previous);
  e::label(c,{164,footer+14,64,24},number(c.output.page+1)+" / 3",{14,e::cream,e::Font::prose,true});
}
void compact_report(Canvas& c,const View& v) {
  const auto r=flight_report(v);
  c.fill(-1000,-1000,3000,3000,{.015F,.045F,.09F,.28F});
  e::heading(c,{24,0,342,100},r.heading,true);
  e::panel(c,e::Sprite::cream_panel,{56,104,278,44},20);
  e::icon(c,{68,109,36,34},e::Sprite::farmland);
  e::label(c,{116,115,206,26},std::string(location_name(v.theme))+" / LV "+number(v.run->stats.level),{20,e::ink});
  const float status_height=e::paragraph_height(c,r.status,322,14);
  e::paragraph(c,{34,160,322,status_height},r.status,14,e::cream);
  const float reward=170+status_height,stats=reward+100,goals=stats+92;
  e::panel(c,e::Sprite::navy_panel,{24,reward,342,88},14);
  e::icon(c,{38,reward+16,54,48},e::Sprite::credit);
  e::label(c,{106,reward+7,242,22},r.reward_label,{17,e::active});
  e::label(c,{106,reward+31,242,35},"+ "+number(r.credits),{34,e::cream});
  if(!r.guest&&!r.pending&&!r.preview)e::label(c,{40,reward+65,308,18},"BANK "+number(r.bank)+" CR",{14,e::cream,e::Font::prose,true});
  e::panel(c,e::Sprite::cream_panel,{24,stats,342,80},14);
  const std::array values{number(r.score),number(r.best_score),number(r.captures),time_text(r.seconds)};
  constexpr std::array titles{"SCORE","BEST","CAPTURES","SURVIVED"};
  for(unsigned i=0;i<4;++i) {
    const float x=34+static_cast<float>(i%2)*164,y=stats+8+static_cast<float>(i/2)*34;
    e::label(c,{x,y+4,92,20},titles[i],{14,e::ink});
    e::label(c,{x+96,y,58,27},values[i],{22,e::ink,e::Font::body,true});
  }
  const float footer=e::height(c)-76;
  if(!r.preview&&!r.guest&&!r.pending) {
    e::panel(c,e::Sprite::cream_panel,{24,goals,342,76},13);
    e::icon(c,{34,goals+16,49,49},e::Sprite::blueprint);
    e::label(c,{96,goals+6,250,22},r.blueprint_heading,{17,e::ink});
    e::label(c,{96,goals+29,250,24},r.blueprint_name,{20,e::ink});
    e::paragraph(c,{96,goals+52,250,20},r.blueprint_detail,14);
    e::progress(c,{96,goals+70,250,5},r.blueprint_fraction);
    e::panel(c,e::Sprite::cream_panel,{24,goals+86,342,76},13);
    e::icon(c,{34,goals+101,49,49},e::Sprite::settings);
    e::label(c,{96,goals+96,250,24},r.hangar_name,{20,e::ink});
    e::paragraph(c,{96,goals+124,250,20},r.hangar_detail,14);
    e::progress(c,{96,goals+154,250,5},r.hangar_fraction);
    if(goals+162>footer-8)c.output.text_overflow=true;
  } else {
    const auto text=r.preview?"Hardware and research last for this flight. Bank the earned credits once before your next flight.":r.guest?"Saving is unavailable. Permanent progress has not changed.":"Your reward is not banked yet. Retry saving before continuing.";
    e::panel(c,e::Sprite::navy_panel,{24,goals,342,footer-goals-12},14);
    e::paragraph(c,{42,goals+16,306,footer-goals-40},text,16,e::cream);
  }
  e::button(c,{24,footer,r.preview?342.F:204.F,64},r.preview?"BANK & RESULTS":"FLY AGAIN",r.preview?Command::finish:Command::retry,e::ButtonStyle::primary);
  if(!r.preview) {
    e::button(c,{244,footer,122,64},"HANGAR",Command::hangar);
    c.output.buttons.back().label=r.hangar_action;
  }
}
void report(Canvas& c,const View& v) {
  if(e::compact(c)){compact_report(c,v);return;}
  const auto r=flight_report(v);
  c.fill(-1000,-1000,3000,3000,{.015F,.045F,.09F,.28F});
  e::heading(c,{24,25,342,165},r.heading);
  e::panel(c,e::Sprite::cream_panel,{56,186,278,51},20);
  e::icon(c,{68,192,40,37},e::Sprite::farmland);
  e::label(c,{120,198,202,26},std::string(location_name(v.theme))+" / LV "+number(v.run->stats.level),{21,e::ink,e::Font::body});
  e::paragraph(c,{34,248,322,56},r.status,15,e::cream);
  e::panel(c,e::Sprite::navy_panel,{24,304,342,103},14);
  e::icon(c,{38,319,60,56},e::Sprite::credit);
  e::label(c,{110,317,238,24},r.reward_label,{19,e::active,e::Font::body});
  e::label(c,{110,346,238,40},"+ "+number(r.credits),{42,e::cream,e::Font::body});
  if(!r.guest&&!r.pending&&!r.preview)e::label(c,{40,387,308,17},"BANK "+number(r.bank)+" CR",{14,e::cream,e::Font::prose,true});
  e::panel(c,e::Sprite::cream_panel,{24,421,342,73},14);
  const std::array values{number(r.score),number(r.best_score),number(r.captures),time_text(r.seconds)};
  constexpr std::array titles{"SCORE","BEST","CAPTURES","SURVIVED"};
  for(unsigned i=0;i<4;++i) {
    const float x=34+static_cast<float>(i)*81;
    e::label(c,{x,433,79,17},titles[i],{14,e::ink,e::Font::body,true});
    e::label(c,{x,456,77,29},values[i],{27,e::ink,e::Font::body,true});
  }
  if(!r.preview&&!r.guest&&!r.pending) {
    e::panel(c,e::Sprite::cream_panel,{24,510,342,85},13);
    e::icon(c,{34,525,49,49},e::Sprite::blueprint);
    e::label(c,{96,520,250,21},r.blueprint_heading,{17,e::ink,e::Font::body});
    e::label(c,{96,543,250,25},r.blueprint_name,{22,e::ink,e::Font::body});
    e::paragraph(c,{96,571,250,19},r.blueprint_detail,14);
    e::progress(c,{96,589,250,5},r.blueprint_fraction);
    e::panel(c,e::Sprite::cream_panel,{24,608,342,85},13);
    e::icon(c,{34,624,49,49},e::Sprite::settings);
    e::label(c,{96,619,250,24},r.hangar_name,{22,e::ink,e::Font::body});
    e::paragraph(c,{96,650,250,20},r.hangar_detail,14);
    e::progress(c,{96,680,250,5},r.hangar_fraction);
  } else {
    e::panel(c,e::Sprite::navy_panel,{24,512,342,170},14);
    const auto text=r.preview?"Hardware and research last for this flight. Bank the earned credits once before your next flight.":r.guest?"Saving is unavailable. Permanent progress has not changed.":"Your reward is not banked yet. Retry saving before continuing.";
    e::paragraph(c,{42,535,306,122},text,18,e::cream);
  }
  const float secondary_height=std::max(44.F,44.F/c.scale);
  const float secondary_y=std::min(798.F,e::height(c)-secondary_height-2);
  const float primary_y=r.preview?696.F:std::min(696.F,secondary_y-102);
  e::button(c,{24,primary_y,342,94},r.preview?"BANK & RESULTS":"FLY AGAIN",r.preview?Command::finish:Command::retry,e::ButtonStyle::primary);
  if(!r.preview)e::button(c,{48,secondary_y,294,secondary_height},r.hangar_action,Command::hangar);
}
void compact_promotion(Canvas& c,const View& v) {
  const auto& run=*v.run;
  std::array<std::string,3> benefits,details;
  std::array<float,3> heights{},condition_y{};
  float total=0;
  for(unsigned i=0;i<3;++i) {
    const auto& pick=run.choices[i];
    // Unstructured historical fixtures already carry their complete benefit
    // in description. Do not invent a second empty before/after metric.
    if(!pick.effect_label.empty())benefits[i]=std::string(choice_effect_label(pick))+" "+choice_change(pick);
    details[i]=std::string(choice_conditions(pick));
    condition_y[i]=55+(benefits[i].empty()?0:e::paragraph_height(c,benefits[i],314,14,e::Font::body)+5);
    heights[i]=std::max(106.F,condition_y[i]+e::paragraph_height(c,details[i],314,14)+10);
    total+=heights[i]+6;
  }
  const float footer=e::height(c)-60;
  const float first=footer-4-total;
  const float capacity=first-40;
  const auto caption="LEVEL "+number(run.stats.level)+". "+level_capture_reward()+". Choose one. Flight paused.";
  const float caption_height=e::paragraph_height(c,caption,318,14);
  const float header=std::min(e::compact(c)?94.F:171.F,capacity-caption_height-20);
  if(header<54) {c.output.text_overflow=true;return;}
  c.fill(-1000,-1000,3000,3000,{.015F,.045F,.09F,.34F});
  e::heading(c,{24,0,342,header},"PROMOTION",header<130);
  e::paragraph(c,{36,header+6,318,caption_height},caption,14,e::cream);
  e::panel(c,e::Sprite::cream_panel,{24,capacity,342,36},12);
  e::label(c,{38,capacity+9,190,23},number(hardware_count(run))+" / 6 HARDWARE",{18,e::ink});
  e::pips(c,{232,capacity+13,116,10},hardware_count(run),6);
  float y=first;
  for(unsigned i=0;i<3;++i) {
    const auto& pick=run.choices[i];
    e::action_panel(c,{24,y,342,heights[i]},"CHOOSE "+number(i+1),static_cast<Command>(static_cast<unsigned>(Command::pick0)+i));
    e::panel(c,e::Sprite::navy_panel,{32,y+7,54,44},10);
    e::icon(c,{38,y+10,42,32},e::reward_icon(pick));
    if(pick.hardware)e::pips(c,{40,y+43,38,6},pick.next_rank,3);
    const bool keyboard=v.hint_device==input::DeviceKind::keyboard;
    e::label(c,{98,y+7,keyboard?192.F:252.F,25},pick.title,{20,e::ink});
    if(keyboard)e::label(c,{300,y+7,50,22},"KEY "+number(i+1),{14,e::ink,e::Font::body,true});
    const auto rarity=pick.rarity==gameplay::Rarity::rare?"RARE":pick.rarity==gameplay::Rarity::uncommon?"UNCOMMON":"COMMON";
    const auto status=pick.evolution_preview.empty()?std::string(rarity)+" / "+choice_status(pick):"EVOLVES: "+pick.evolution_preview;
    e::label(c,{94,y+32,266,22},status,{14,e::ink});
    if(!benefits[i].empty())e::paragraph(c,{38,y+55,314,condition_y[i]-60},benefits[i],14,e::ink,e::Font::body);
    e::paragraph(c,{38,y+condition_y[i],314,heights[i]-condition_y[i]-10},details[i],14);
    if(v.hint_device==input::DeviceKind::keyboard)
      c.output.buttons.back().label+=" / KEY "+number(i+1);
    y+=heights[i]+6;
  }
  if(run.rerolls_remaining)e::button(c,{24,footer,164,54},"REROLL / "+number(run.rerolls_remaining),Command::reroll);
  e::button(c,{202,footer,164,54},"EVOLUTIONS",Command::recipe0);
}
void promotion(Canvas& c,const View& v) {
  if(e::compact(c)){compact_promotion(c,v);return;}
  const auto& run=*v.run;
  std::array<float,3> heights{};
  std::array<std::string,3> details{};
  float total=0;
  for(unsigned i=0;i<3;++i) {
    const auto& choice=run.choices[i];
    details[i]=std::string(choice_conditions(choice));
    if(!choice.evolution_preview.empty())details[i]+="\nEvolves: "+choice.evolution_preview;
    const auto count=e::lines(c,details[i],314,14).size();
    const float type=std::max(14.F,14.F/c.scale);
    heights[i]=std::max(132.F,96+(count?static_cast<float>(count-1)*type*1.2F+type:0));
    total+=heights[i]+10;
  }
  const float first=std::min(392.F,788-total);
  if(first<359){compact_promotion(c,v);return;}
  c.fill(-1000,-1000,3000,3000,{.015F,.045F,.09F,.34F});
  e::heading(c,{24,20,342,171},"PROMOTION");
  e::panel(c,e::Sprite::secondary,{206,164,152,45},18);
  e::label(c,{218,176,128,25},"LEVEL "+number(run.stats.level),{23,e::ink,e::Font::body,true});
  e::paragraph(c,{36,215,318,36},level_capture_reward()+". Choose one. Flight paused.",14,e::cream);
  e::panel(c,e::Sprite::cream_panel,{24,first-54,342,44},13);
  e::label(c,{38,first-42,190,23},number(hardware_count(run))+" / 6 HARDWARE",{20,e::ink,e::Font::body});
  e::pips(c,{232,first-38,116,14},hardware_count(run),6);
  float y=first;
  for(unsigned i=0;i<3;++i) {
    const auto& pick=run.choices[i];
    e::action_panel(c,{24,y,342,heights[i]},"CHOOSE "+number(i+1),static_cast<Command>(static_cast<unsigned>(Command::pick0)+i));
    e::panel(c,e::Sprite::navy_panel,{32,y+8,64,52},10);
    e::icon(c,{40,y+12,48,44},e::reward_icon(pick));
    e::label(c,{108,y+10,182,27},pick.title,{24,e::ink,e::Font::body});
    const bool keyboard=v.hint_device==input::DeviceKind::keyboard;
    const auto rarity=pick.rarity==gameplay::Rarity::rare?"RARE":pick.rarity==gameplay::Rarity::uncommon?"UNCOMMON":"COMMON";
    e::label(c,{108,y+40,242,21},std::string(rarity)+" / "+choice_status(pick),{14,e::ink,e::Font::body});
    if(pick.hardware)e::pips(c,{300,y+28,48,8},pick.next_rank,3);
    if(keyboard)e::label(c,{300,y+10,50,21},"KEY "+number(i+1),{14,e::ink,e::Font::body,true});
    e::label(c,{38,y+63,314,20},std::string(choice_effect_label(pick))+" "+choice_change(pick),{16,e::ink,e::Font::body});
    e::paragraph(c,{38,y+88,314,heights[i]-96},details[i],14);
    y+=heights[i]+10;
  }
  if(run.rerolls_remaining)e::button(c,{24,783,164,49},"REROLL / "+number(run.rerolls_remaining),Command::reroll);
  e::button(c,{202,783,164,49},"EVOLUTIONS",Command::recipe0);
}
} // namespace
bool enamel_flight_screen(Canvas& c,const View& v) {
  if(v.screen==Screen::pause&&v.show_build)build(c,v);
  else if(v.screen==Screen::upgrade)promotion(c,v);
  else if(v.screen==Screen::results||v.screen==Screen::death)report(c,v);
  else return false;
  return true;
}
} // namespace human_resources::product
