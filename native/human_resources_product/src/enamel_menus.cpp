#include "enamel_menus.h"
#include "enamel_components.h"
#include "enamel_layout.h"
#include "privacy_policy.h"
#include "upgrade_presentation.h"
#include "save_protection.h"

#include <algorithm>

namespace human_resources::product {
namespace {
namespace e=enamel;
using detail::Canvas;
using detail::number;

void hangar(Canvas& c, const View& v) {
  const bool compact=e::compact(c),shortest=e::height(c)<640;
  if(compact) {
    e::heading(c,{10,shortest?-8.F:0.F,204,shortest?82.F:104.F},"HANGAR",true);
    e::wallet(c,{224,shortest?0.F:16.F,150,62},v.store->account().credits);
    e::paragraph(c,{22,shortest?78.F:110.F,346,22},"Permanent bonuses for your next flight.",14,e::cream);
  } else {
    e::heading(c,{10,39,276,150},"HANGAR");
    e::wallet(c,{217,151,157,62},v.store->account().credits);
    e::paragraph(c,{30,217,330,22},"Permanent bonuses for your next flight.",15,e::cream);
  }
  const auto tracks=v.store->hangar();
  constexpr std::array symbols{e::Sprite::laser,e::Sprite::motion,e::Sprite::shield,e::Sprite::beam};
  for(unsigned i=0;i<tracks.size();++i) {
    const auto& t=tracks[i];
    const float y=(compact?(shortest?104.F:134.F):244.F)+static_cast<float>(i)*(compact?112.F:116.F);
    if(compact) {
      e::panel(c,e::Sprite::cream_panel,{18,y,354,108},14);
      e::panel(c,e::Sprite::navy_panel,{24,y+6,60,96},12);
      e::icon(c,{31,y+16,46,44},symbols[i]);
      e::pips(c,{30,y+70,48,8},t.rank,t.maximum_rank);
      e::label(c,{90,y+8,266,24},t.title,{20,e::ink});
      e::label(c,{90,y+33,150,22},"RANK "+number(t.rank)+" / "+number(t.maximum_rank),{14,e::ink});
      e::label(c,{90,y+57,150,24},t.owned_bonus+(t.next_bonus.empty()?"":" -> "+t.next_bonus),{20,e::ink});
      e::button(c,{254,y+28,104,54},t.rank>=t.maximum_rank?"MAXED":number(t.cost)+" CR",static_cast<Command>(static_cast<unsigned>(Command::buy0)+i),e::ButtonStyle::navy,t.affordable);
      e::paragraph(c,{90,y+84,266,18},t.bonus_basis,14);
      continue;
    }
    e::panel(c,e::Sprite::cream_panel,{18,y,354,112},14);
    e::panel(c,e::Sprite::navy_panel,{24,y+6,76,100},12);
    e::icon(c,{31,y+25,62,62},symbols[i]);
    e::label(c,{110,y+7,246,26},t.title,{23,e::ink,e::Font::body});
    e::label(c,{110,y+34,105,20},"RANK "+number(t.rank)+" / "+number(t.maximum_rank),{15,e::ink,e::Font::body});
    e::pips(c,{226,y+30,124,9},t.rank,t.maximum_rank);
    e::label(c,{110,y+57,138,23},t.owned_bonus+(t.next_bonus.empty()?"":" -> "+t.next_bonus),{21,e::ink,e::Font::body});
    e::button(c,{254,y+43,104,44},t.rank>=t.maximum_rank?"MAXED":number(t.cost)+" CR",static_cast<Command>(static_cast<unsigned>(Command::buy0)+i),e::ButtonStyle::navy,t.affordable);
    e::paragraph(c,{110,y+90,246,18},t.bonus_basis,14);
  }
  if(compact) {
    const float y=e::height(c)-66;
    e::button(c,{26,y,54,54},"",Command::home);
    c.output.buttons.back().label="BACK";
    e::icon(c,{41,y+15,24,24},e::Sprite::arrow_left);
    e::button(c,{88,y,136,54},"BLUEPRINTS",Command::blueprints);
    e::button(c,{232,y,134,54},"FLIGHT",Command::start,e::ButtonStyle::primary);
    c.output.buttons.back().label="NEXT FLIGHT";
  } else {
    e::button(c,{26,714,148,44},"BACK",Command::home);
    e::button(c,{192,714,172,44},"BLUEPRINTS",Command::blueprints);
    e::button(c,{28,766,334,76},"NEXT FLIGHT",Command::start,e::ButtonStyle::primary);
  }
}
void blueprints(Canvas& c, const View& v) {
  const bool compact=e::compact(c);
  e::heading(c,{30,compact?0.F:23.F,330,compact?124.F:176.F},"BLUEPRINT\nLIBRARY",compact);
  e::paragraph(c,{30,compact?128.F:201.F,330,compact?42.F:40.F},"Free milestones. Unlocked hardware joins future draws.",14,e::cream);
  const auto items=v.store->blueprints();
  for(unsigned i=0;i<items.size();++i) {
    const auto& b=items[i];
    const float y=(compact?180.F:250.F)+static_cast<float>(i)*(compact?140.F:156.F);
    e::panel(c,e::Sprite::cream_panel,{22,y,346,compact?134.F:143.F},15);
    e::panel(c,e::Sprite::blueprint_tile,{29,y+8,82,compact?118.F:124.F},12);
    const auto module=std::find(gameplay::tuning::module_names.begin(),gameplay::tuning::module_names.end(),b.title);
    if(module!=gameplay::tuning::module_names.end())
      e::icon(c,{38,y+26,64,64},e::hardware_icon(static_cast<gameplay::Module>(module-gameplay::tuning::module_names.begin())));
    else e::icon(c,{38,y+26,64,64},e::Sprite::blueprint);
    e::label(c,{121,y+7,230,30},b.title,{compact?22.F:24.F,e::ink,e::Font::body});
    e::paragraph(c,{121,y+40,225,38},b.unlocked?"UNLOCKED":b.milestone,14);
    e::progress(c,{121,y+82,193,10},b.threshold?static_cast<float>(b.progress)/static_cast<float>(b.threshold):1);
    e::icon(c,{324,y+74,28,28},b.unlocked?e::Sprite::check:e::Sprite::lock);
    e::paragraph(c,{121,y+(compact?92.F:101.F),230,compact?39.F:38.F},"Run: "+b.requirement,14);
  }
  e::button(c,{24,compact?e::height(c)-60:778,150,compact?54.F:50.F},"BACK",Command::home);
  e::button(c,{192,compact?e::height(c)-60:778,174,compact?54.F:50.F},"EVOLUTIONS",Command::recipes,e::ButtonStyle::navy);
}
void recipes(Canvas& c,const View& v) {
  // A height-fitted portrait can also have a narrow physical text column.
  // Choose the spread model once for the viewport, never per recipe length.
  if(e::compact(c) || c.scale<.95F) {
    const auto count=static_cast<unsigned>(gameplay::tuning::active_evolutions.size());
    c.output.recipe_page_count=count;
    const unsigned page=v.recipe_page%count;
    const auto i=gameplay::tuning::active_evolutions[page];
    const auto pair=gameplay::tuning::evolution_ingredients[i];
    e::heading(c,{30,0,330,108},"EARNED\nEVOLUTIONS",true);
    e::paragraph(c,{30,113,330,61},"Both ingredients need rank III. Your first two completed recipes evolve automatically.",14,e::cream);
    const float y=184,bottom=e::height(c)-126;
    e::panel(c,e::Sprite::cream_panel,{22,y,346,bottom-y},15);
    e::panel(c,e::Sprite::navy_panel,{28,y+6,334,40},9);
    e::icon(c,{38,y+12,28,28},e::hardware_icon(static_cast<gameplay::Module>(pair[0])));
    e::label(c,{76,y+14,272,28},gameplay::tuning::evolution_names[i],{25,e::cream});
    for(unsigned ingredient=0;ingredient<2;++ingredient) {
      const auto module=pair[ingredient];
      const float x=37+static_cast<float>(ingredient)*164;
      e::panel(c,e::Sprite::navy_panel,{x,y+52,146,72},10);
      e::icon(c,{x+5,y+60,44,44},e::hardware_icon(static_cast<gameplay::Module>(module)));
      e::label(c,{x+55,y+64,82,25},number(v.run->modules[module])+" / III",{20,e::cream});
      e::pips(c,{x+55,y+98,75,10},v.run->modules[module],3);
    }
    const auto ingredients=std::string(gameplay::tuning::module_names[pair[0]])+" III + "+std::string(gameplay::tuning::module_names[pair[1]])+" III";
    const float ingredient_height=e::paragraph_height(c,ingredients,316,14,e::Font::body);
    e::paragraph(c,{37,y+136,316,ingredient_height},ingredients,14,e::ink,e::Font::body);
    const float effect_y=y+146+ingredient_height;
    e::paragraph(c,{37,effect_y,316,bottom-effect_y-12},recipe_effects[i],14);
    e::button(c,{26,e::height(c)-114,156,54},"PREVIOUS",Command::recipe_previous);
    e::button(c,{208,e::height(c)-114,156,54},"NEXT",Command::recipe_next);
    e::button(c,{26,e::height(c)-56,148,54},"BACK",Command::home);
    e::button(c,{192,e::height(c)-56,172,54},"BLUEPRINTS",Command::recipes,e::ButtonStyle::navy);
    return;
  }
  e::heading(c,{30,18,330,170},"EARNED\nEVOLUTIONS");
  e::paragraph(c,{30,189,330,45},"Both ingredients need rank III. Your first two completed recipes evolve automatically.",14,e::cream);
  const auto page_count=(static_cast<unsigned>(gameplay::tuning::active_evolutions.size())+1U)/2U;
  c.output.recipe_page_count=page_count;
  const unsigned page=v.recipe_page%page_count;
  for(unsigned slot=0;slot<2;++slot) {
    const unsigned position=page*2+slot;
    if(position>=gameplay::tuning::active_evolutions.size())break;
    const auto i=gameplay::tuning::active_evolutions[position];
    const auto pair=gameplay::tuning::evolution_ingredients[i];
    const float y=242+static_cast<float>(slot)*227;
    e::panel(c,e::Sprite::cream_panel,{22,y,346,216},15);
    e::panel(c,e::Sprite::navy_panel,{28,y+6,334,40},9);
    e::icon(c,{38,y+12,28,28},e::hardware_icon(static_cast<gameplay::Module>(pair[0])));
    e::label(c,{76,y+14,272,28},gameplay::tuning::evolution_names[i],{27,e::cream,e::Font::body});
    for(unsigned ingredient=0;ingredient<2;++ingredient) {
      const auto module=pair[ingredient];
      const float x=37+static_cast<float>(ingredient)*164;
      e::panel(c,e::Sprite::navy_panel,{x,y+47,146,72},10);
      e::label(c,{x+5,y+52,136,20},gameplay::tuning::module_names[module],{14,e::cream,e::Font::body});
      e::icon(c,{x+8,y+77,36,34},e::hardware_icon(static_cast<gameplay::Module>(module)));
      e::label(c,{x+53,y+78,84,25},number(v.run->modules[module])+" / III",{20,e::cream,e::Font::body});
      e::pips(c,{x+53,y+105,75,9},v.run->modules[module],3);
    }
    e::paragraph(c,{37,y+124,316,84},recipe_effects[i],14);
  }
  e::button(c,{26,706,156,48},"PREVIOUS",Command::recipe_previous);
  e::button(c,{208,706,156,48},"NEXT",Command::recipe_next);
  e::label(c,{120,758,150,20},number(page+1)+" / "+number(page_count),{14,e::cream,e::Font::prose,true});
  e::button(c,{26,784,148,48},"BACK",Command::home);
  e::button(c,{192,784,172,48},"BLUEPRINTS",Command::recipes,e::ButtonStyle::navy);
}
void settings(Canvas& c,const View& v) {
  const bool compact=e::compact(c);
  const auto& s=v.store->account().settings;
  e::heading(c,{30,compact?0.F:17.F,330,compact?142.F:180.F},"FLIGHT\nSETTINGS",compact);
  e::panel(c,e::Sprite::navy_panel,{24,compact?149.F:204.F,342,compact?201.F:241.F},16);
  e::toggle(c,{36,compact?160.F:216.F,318,compact?56.F:66.F},"SOUND EFFECTS",e::Sprite::sound,s.audio,Command::audio);
  e::toggle(c,{36,compact?224.F:291.F,318,compact?56.F:66.F},"HAPTICS",e::Sprite::haptics,s.haptics,Command::haptics,v.haptics_available);
  e::toggle(c,{36,compact?288.F:366.F,318,compact?56.F:66.F},"REDUCED MOTION",e::Sprite::motion,s.reduced_motion,Command::motion);
  e::panel(c,e::Sprite::cream_panel,{24,compact?365.F:459.F,342,compact?170.F:225.F},16);
  e::icon(c,{38,compact?377.F:472.F,compact?36.F:42.F,compact?36.F:42.F},e::Sprite::motion);
  e::label(c,{compact?86.F:93.F,compact?383.F:479.F,compact?260.F:250.F,30},"FLIGHT CONTROLS",{compact?23.F:25.F,e::ink,e::Font::body});
  constexpr std::array symbols{e::Sprite::motion,e::Sprite::dash,e::Sprite::laser,e::Sprite::beam,e::Sprite::beam_splitter};
  constexpr std::array copy{"Drag to fly / WASD / left stick.","Dash: flick / Space / button.","Capture and fire are automatic.","Keep moving through targets.","Three sweeps trigger Overdrive."};
  for(unsigned i=0;i<copy.size();++i) {
    const float y=(compact?417.F:522.F)+static_cast<float>(i)*(compact?22.F:30.F);
    e::icon(c,{41,y,compact?20.F:24.F,compact?20.F:24.F},symbols[i]);
    e::paragraph(c,{compact?70.F:79.F,y+2,compact?280.F:269.F,compact?24.F:28.F},copy[i],14);
  }
  e::button(c,{38,compact?550.F:708.F,314,compact?54.F:52.F},"PRIVACY POLICY",Command::privacy);
  e::button(c,{116,compact?e::height(c)-58:780,158,compact?54.F:48.F},"BACK",Command::home);
}
void privacy(Canvas& c,const View& v) {
  const bool compact=e::compact(c);
  const auto count=static_cast<unsigned>(privacy_policy_sections.size());
  c.output.page_count=count;
  c.output.page=std::min(v.ui_page,count-1);
  const auto text=privacy_policy_sections[c.output.page];
  const auto split=text.find('\n');
  e::heading(c,{30,compact?0.F:20.F,330,compact?100.F:180.F},"PRIVACY\nPOLICY",compact);
  e::panel(c,e::Sprite::cream_panel,{24,compact?112.F:216.F,342,compact?e::height(c)-234:449},16);
  e::icon(c,{40,compact?130.F:234.F,40,40},e::Sprite::folder);
  e::label(c,{93,compact?137.F:241.F,251,32},text.substr(0,split),{compact?22.F:26.F,e::ink,e::Font::body});
  e::paragraph(c,{42,compact?190.F:294.F,306,compact?e::height(c)-328:345},text.substr(split+1),18);
  c.output.page_text=std::string(text);
  if(!compact)e::label(c,{120,682,150,24},number(c.output.page+1)+" / "+number(count),{16,e::cream,e::Font::prose,true});
  e::button(c,{26,compact?e::height(c)-114:714,156,compact?54.F:48.F},"PREVIOUS",Command::page_previous,e::ButtonStyle::secondary,c.output.page>0);
  e::button(c,{208,compact?e::height(c)-114:714,156,compact?54.F:48.F},"NEXT",Command::page_next,e::ButtonStyle::secondary,c.output.page+1<count);
  e::button(c,{116,compact?e::height(c)-56:784,158,compact?54.F:48.F},"BACK",Command::privacy);
}
void protected_save(Canvas& c,const View& v) {
  const bool compact=e::compact(c);
  const auto message=v.save_message.empty()?std::string_view("Saving failed. Retry before continuing."):v.save_message;
  const auto notice=protection_notice(v);
  const float header=compact?86.F:120.F,body_size=compact?16.F:18.F;
  const float message_height=std::max(compact?64.F:76.F,e::paragraph_height(c,message,298,body_size));
  const float notice_height=std::max(34.F,e::paragraph_height(c,notice,266,14));
  const float action_height=compact?92.F:108.F;
  const bool backup=v.can_reset_save || v.can_bank_checkpoint;
  const float total=header+12+message_height+12+notice_height+6+action_height+(backup?64.F:0.F)+14;
  const float top=(e::height(c)-total)*.5F+(compact?0.F:32.F);
  c.fill(-1000,-1000,3000,3000,{.015F,.045F,.09F,.58F});
  e::panel(c,e::Sprite::cream_panel,{26,top,338,total},18);
  e::panel(c,e::Sprite::navy_panel,{30,top+4,330,header-4},15);
  e::panel(c,e::Sprite::navy_panel,{37,top+13,compact?66.F:84.F,header-26},11);
  e::icon(c,{43,top+20,compact?54.F:72.F,header-40},e::Sprite::shield);
  e::icon(c,{compact?59.F:62.F,top+header*.49F,compact?23.F:34.F,compact?20.F:28.F},e::Sprite::folder);
  const float text_x=compact?115.F:130.F,text_width=350-text_x;
  e::label(c,{text_x,top+(compact?15.F:21.F),text_width,header*.4F},"PROGRESS",{compact?31.F:42.F,e::cream,e::Font::compact,false,true});
  e::label(c,{text_x,top+header*.54F,text_width,header*.4F},"PROTECTED",{compact?31.F:42.F,e::cream,e::Font::compact,false,true});
  const float message_y=top+header+12,notice_y=message_y+message_height+12;
  e::paragraph(c,{46,message_y,298,message_height},message,body_size);
  c.fill(46,notice_y-6,298,1,e::ink);
  e::icon(c,{46,notice_y+3,26,26},e::Sprite::info);
  e::paragraph(c,{78,notice_y,266,notice_height},notice,14);
  c.output.page_text=std::string(message)+"\n"+std::string(notice);
  const float action_y=notice_y+notice_height+6;
  e::button(c,{38,action_y,314,action_height},"RETRY SAVE",Command::retry_save,e::ButtonStyle::primary);
  if(backup)e::button(c,{44,action_y+action_height+10,302,54},v.can_reset_save?"BACK UP & RESET":"BACK UP & BANK",v.can_reset_save?Command::back_up_reset:Command::back_up_bank);
}
} // namespace
bool enamel_menu(detail::Canvas& c,const View& v) {
  if(v.save_error)protected_save(c,v);
  else switch(v.screen) {
    case Screen::hangar:hangar(c,v);break;
    case Screen::settings:if(v.show_privacy)privacy(c,v);else settings(c,v);break;
    case Screen::blueprints:if(v.show_recipes)recipes(c,v);else blueprints(c,v);break;
    default:return false;
  }
  return true;
}
} // namespace human_resources::product
