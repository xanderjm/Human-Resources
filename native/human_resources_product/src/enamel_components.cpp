#include "enamel_components.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace human_resources::product::enamel {
namespace {
void quad(Canvas& c, ui::Rect box, ui::Rect source, Color color) {
  const auto r = c.rect(box.x, box.y, box.width, box.height);
  c.output.quads.push_back({{(r.x-c.vx)/c.w,(r.y-c.vy)/c.h,r.width/c.w,r.height/c.h},
      {source.x/atlas_width,source.y/atlas_height,source.width/atlas_width,source.height/atlas_height},
      color,{1,0,0,0}});
}
void register_button(Canvas& c, ui::Rect box, Command command, std::string label, bool enabled) {
  auto r = c.rect(box.x,box.y,box.width,box.height);
  if (r.width < 44) { r.x -= (44-r.width)*.5F; r.width = 44; }
  if (r.height < 44) { r.y -= (44-r.height)*.5F; r.height = 44; }
  c.output.buttons.push_back({r,command,std::move(label),enabled});
}
float font_size(const Canvas& c, float size) {
  return std::max(size,14.F/c.scale)*c.type_scale;
}
unsigned glyph_index(unsigned char ch) {
  return ch >= 32 && ch < 127 ? ch-32U : '?' - 32U;
}
std::optional<Sprite> action_icon(Command command) {
  switch(command) {
    case Command::hangar:case Command::current_build:return Sprite::build;
    case Command::settings:return Sprite::settings;
    case Command::privacy:return Sprite::folder;
    case Command::blueprints:return Sprite::blueprint;
    case Command::recipes:case Command::recipe0:return Sprite::beam_splitter;
    case Command::back_up_reset:case Command::back_up_bank:return Sprite::backup;
    case Command::finish:case Command::buy0:case Command::buy1:case Command::buy2:case Command::buy3:return Sprite::credit;
    case Command::home:case Command::page_previous:case Command::recipe_previous:return Sprite::arrow_left;
    case Command::page_next:case Command::recipe_next:return Sprite::arrow_right;
    case Command::reroll:return Sprite::reroll;
    default:return std::nullopt;
  }
}
void glyph_line(Canvas& c, ui::Rect box, std::string_view text, const Label& style, Color color, float dx, float dy) {
  const unsigned bank = style.font == Font::display ? 1 : 0;
  const float size = font_size(c,style.size);
  constexpr float width_scale = 1.F;
  const float width = measure(c,text,style.size,style.font);
  const float start = box.x+(style.centered ? (box.width-width)*.5F : 0)+dx;
  float x = start;
  for (const char raw : text) {
    const unsigned index = glyph_index(static_cast<unsigned char>(raw));
    const float y = box.y-font_cap_top[static_cast<unsigned>(style.font)]*size+dy+style.rise*(x-start-width*.5F);
    const ui::Rect geometry{x-4*size/48.F*width_scale,y-8*size/48.F,64*size/48.F*width_scale,80*size/48.F};
    quad(c,geometry,{static_cast<float>(index%16*64),static_cast<float>(font_bank_y[bank]+index/16*80),64,80},color);
    if (style.font == Font::compact || style.font == Font::prose) {
      auto& q = c.output.quads.back();
      q.uv = {static_cast<float>(index%16*64)/1024.F,static_cast<float>((style.font==Font::compact?512:0)+index/16*80)/2048.F,64.F/1024.F,80.F/2048.F};
      q.presentation[0] = 0;
    }
    c.output.quads.back().presentation[2] = style.rise*geometry.width*c.scale/c.h;
    const auto advance = style.font == Font::compact || style.font == Font::prose ? product::glyph_advances[(style.font==Font::compact?95:0)+index] : glyph_advances[bank*95+index];
    x += advance*size/48.F*width_scale;
  }
}
} // namespace

void picture(Canvas& c, Sprite sprite, ui::Rect box, Color tint) {
  const auto& r = regions[static_cast<unsigned>(sprite)];
  quad(c,box,{static_cast<float>(r.x),static_cast<float>(r.y),static_cast<float>(r.width),static_cast<float>(r.height)},tint);
}
void panel(Canvas& c, Sprite sprite, ui::Rect box, float corner, Color tint) {
  const auto& r = regions[static_cast<unsigned>(sprite)];
  const float left = static_cast<float>(r.insets[0]), top = static_cast<float>(r.insets[1]);
  const float right = static_cast<float>(r.insets[2]), bottom = static_cast<float>(r.insets[3]);
  if (left+right+top+bottom == 0) { picture(c,sprite,box,tint); return; }
  // One scale for all fixed corner pixels preserves screw/rivet proportions.
  // A zero vertical inset is a horizontal three-slice capsule, not a sampled
  // one-pixel top/bottom strip stretched over a fabricated corner.
  const float fixed_scale=std::min({top+bottom==0?box.height/static_cast<float>(r.height):corner/static_cast<float>(r.corner_radius),
      left+right>0?(box.width-std::min(1.F,box.width*.1F))/(left+right):1.F,
      top+bottom>0?(box.height-std::min(1.F,box.height*.1F))/(top+bottom):1.F});
  const std::array<float,4> x{box.x,box.x+left*fixed_scale,box.x+box.width-right*fixed_scale,box.x+box.width};
  const std::array<float,4> y{box.y,box.y+top*fixed_scale,box.y+box.height-bottom*fixed_scale,box.y+box.height};
  const std::array<float,4> sx{static_cast<float>(r.x),r.x+left,r.x+r.width-right,static_cast<float>(r.x+r.width)};
  const std::array<float,4> sy{static_cast<float>(r.y),r.y+top,r.y+r.height-bottom,static_cast<float>(r.y+r.height)};
  for (unsigned row=0; row<3; ++row) for (unsigned column=0; column<3; ++column)
    if (x[column+1]>x[column] && y[row+1]>y[row])
      quad(c,{x[column],y[row],x[column+1]-x[column],y[row+1]-y[row]},
          {sx[column],sy[row],sx[column+1]-sx[column],sy[row+1]-sy[row]},tint);
}
float measure(const Canvas& c, std::string_view value, float size, Font font) {
  const unsigned bank = font == Font::display ? 95 : 0;
  float width = 0;
  for (const char raw : value) {
    const auto index = glyph_index(static_cast<unsigned char>(raw));
    width += font == Font::compact || font == Font::prose ? product::glyph_advances[(font==Font::compact?95:0)+index] : glyph_advances[bank+index];
  }
  return width*font_size(c,size)/48.F;
}
std::string short_number(std::uint64_t value) {
  if(value<10000)return detail::number(value);
  constexpr std::array<std::string_view,6> suffix{"K","M","B","T","P","E"};
  unsigned unit=0;value/=1000;
  while(value>=1000 && unit+1<suffix.size()){value/=1000;++unit;}
  return detail::number(value)+std::string(suffix[unit]);
}
void label(Canvas& c, ui::Rect box, std::string_view text, Label style) {
  auto visible=text;
  while(!visible.empty() && visible.back()==' ')visible.remove_suffix(1);
  const float minimum = 14.F/c.scale;
  while (style.shrink_to_fit && c.type_scale == 1 && style.size>minimum &&
         (measure(c,visible,style.size,style.font)>box.width || font_size(c,style.size)>box.height))
    style.size -= .5F;
  const float width = measure(c,visible,style.size,style.font);
  const float size = font_size(c,style.size);
  if (width>box.width+.01F || size>box.height+.01F) c.output.text_overflow = true;
  if (style.shadow) {
    glyph_line(c,box,text,style,ink,2,3);
    glyph_line(c,box,text,style,accent,.8F,1.4F);
  }
  glyph_line(c,box,text,style,style.color,0,0);
}
std::vector<std::string> lines(const Canvas& c, std::string_view value, float width, float size, Font font) {
  std::vector<std::string> result;
  std::string line,word;
  const auto draw=[&]() {
    result.push_back(line);
    line.clear();
  };
  const auto flush=[&]() {
    if(word.empty())return;
    if(!line.empty() && measure(c,line+word,size,font)>width)draw();
    for(const auto ch:word) {
      if(!line.empty() && measure(c,line+ch,size,font)>width)draw();
      line+=ch;
    }
    word.clear();
  };
  for(const auto ch:value) {
    if(ch==' ' || ch=='\n') {
      flush();
      if(ch=='\n')draw();
      else if(!line.empty())line+=' ';
    } else word+=ch;
  }
  flush();
  if(!line.empty())draw();
  return result;
}
void tactical_label(Canvas& c, ui::Rect box, std::string_view text, float size, Color color, bool centered) {
  // The nominal em can exceed a fixed tactical strip. Centre the source's
  // uppercase/digit/suffix ink envelope without reducing requested type.
  const float actual_size=font_size(c,size);
  const float y=box.y+(box.height-font_tactical_height[0]*actual_size)*.5F-font_tactical_top[0]*actual_size;
  label(c,{box.x,y,box.width,actual_size},text,{size,color,Font::body,centered,false,0,false});
}
float paragraph_height(const Canvas& c, std::string_view value, float width, float size, Font font) {
  const auto count=lines(c,value,width,size,font).size();
  return count?static_cast<float>(count-1)*font_size(c,size)*1.2F+font_size(c,size):0;
}
void paragraph(Canvas& c, ui::Rect box, std::string_view value, float size, Color color, Font font) {
  const float leading=font_size(c,size)*1.2F;
  float y=box.y;
  for(const auto& line:lines(c,value,box.width,size,font)) {
    if(y+font_size(c,size)>box.bottom()+.01F)c.output.text_overflow=true;
    label(c,{box.x,y,box.width,leading},line,{size,color,font});
    y+=leading;
  }
}
void heading(Canvas& c, ui::Rect box, std::string_view title, bool compact) {
  if(compact) {
    panel(c,Sprite::navy_panel,{box.x,box.y+6,box.width,box.height-12},14);
    std::string text(title);
    std::replace(text.begin(),text.end(),'\n',' ');
    icon(c,{box.right()-54,box.y+(box.height-42)*.5F,44,42},Sprite::beam);
    label(c,{box.x+14,box.y+(box.height-30)*.5F,box.width-76,36},text,{42,cream,Font::compact,true,true,-.05F});
    return;
  }
  picture(c,Sprite::title_frame,box);
  // The tilt belongs to the same live glyph geometry as the blank sign, not
  // baked words. The body, actions and semantics retain ordinary coordinates.
  auto split=title.find('\n');
  if(split==std::string_view::npos && title.size()>13) {
    split=title.find(' ',title.size()/2);
    if(split==std::string_view::npos)split=title.rfind(' ');
  }
  if(split==std::string_view::npos) {
    const ui::Rect text{box.x+box.width*.10F,box.y+box.height*.37F,box.width*.68F,box.height*.44F};
    label(c,text,title,{60,cream,Font::compact,true,true,-.17F});
  } else {
    label(c,{box.x+box.width*.10F,box.y+box.height*.21F,box.width*.72F,box.height*.34F},title.substr(0,split),{52,cream,Font::compact,true,true,-.17F});
    label(c,{box.x+box.width*.10F,box.y+box.height*.50F,box.width*.72F,box.height*.34F},title.substr(split+1),{52,accent,Font::compact,true,true,-.17F});
  }
}
void button(Canvas& c, ui::Rect box, std::string text, Command command, ButtonStyle style, bool enabled) {
  const auto tint = c.button_color(enabled ? transparent_white : Color{.48F,.54F,.57F,.9F},command,enabled);
  const auto first_surface = c.output.quads.size();
  if (style == ButtonStyle::primary) {
    picture(c,Sprite::primary,box,tint);
    c.output.quads.back().presentation[3] = 1;
    label(c,{box.x+box.width*.13F,box.y+box.height*.32F,box.width*.74F,box.height*.42F},text,
          {54,enabled?cream:detail::muted,Font::compact,true,true});
  } else {
    panel(c,style==ButtonStyle::navy?Sprite::navy_panel:Sprite::secondary,box,style==ButtonStyle::navy?12.F:box.height*.65F,tint);
    for (auto i=first_surface; i<c.output.quads.size(); ++i) c.output.quads[i].presentation[3] = 1;
    float left=box.x+14,width=box.width-28;
    const float symbol_size=std::min(32.F,box.height-18);
    // Retain the entire live label at its physical floor on narrow controls.
    // Decorative icons never turn a reachable action into another text page.
    if(const auto symbol=text=="BACK"?std::optional<Sprite>(Sprite::arrow_left):action_icon(command);symbol && !text.empty() &&
        measure(c,text,14.F/c.scale)<=width-symbol_size-8) {
      const bool trailing=command==Command::page_next || command==Command::recipe_next;
      icon(c,{trailing?box.right()-14-symbol_size:left,box.y+(box.height-symbol_size)*.5F,symbol_size,symbol_size},*symbol);
      if(!trailing)left+=symbol_size+8;
      width-=symbol_size+8;
    }
    label(c,{left,box.y+(box.height-23)*.5F-2,width,28},text,
          {20,enabled?(style==ButtonStyle::navy?cream:ink):detail::muted,Font::body,true});
  }
  register_button(c,box,command,std::move(text),enabled);
}
void tabs(Canvas& c,ui::Rect box,std::span<const Tab> items,unsigned selected) {
  if(items.empty())return;
  panel(c,Sprite::secondary,box,box.height*.65F);
  const float width=box.width/static_cast<float>(items.size());
  for(unsigned i=0;i<items.size();++i) {
    const ui::Rect cell{box.x+static_cast<float>(i)*width,box.y,width,box.height};
    const auto first_surface=c.output.quads.size();
    if(i==selected) {
      c.fill(cell.x,cell.y,cell.width,cell.height,active);
      panel(c,Sprite::navy_panel,{cell.x+2,cell.y+2,cell.width-4,cell.height-4},10,c.button_color(transparent_white,items[i].command));
    } else {
      c.fill(cell.x+2,cell.y+3,cell.width-4,cell.height-6,c.button_color(cream,items[i].command));
      if(i)c.fill(cell.x,cell.y+8,1,cell.height-16,ink);
    }
    for(auto q=first_surface;q<c.output.quads.size();++q)c.output.quads[q].presentation[3]=1;
    const float icon_size=std::min(30.F,box.height*.43F);
    icon(c,{cell.x+(width-icon_size)*.5F,cell.y+6,icon_size,icon_size},items[i].icon);
    label(c,{cell.x+5,cell.bottom()-23,cell.width-10,21},items[i].label,{14,i==selected?cream:ink,Font::body,true});
    register_button(c,cell,items[i].command,std::string(items[i].label),true);
  }
}
void action_panel(Canvas& c, ui::Rect box, std::string text, Command command, Sprite sprite, bool enabled) {
  const auto first=c.output.quads.size();
  panel(c,sprite,box,sprite==Sprite::secondary?box.height*.65F:14.F,c.button_color(enabled?transparent_white:Color{.48F,.54F,.57F,.9F},command,enabled));
  for(auto i=first;i<c.output.quads.size();++i)c.output.quads[i].presentation[3]=1;
  register_button(c,box,command,std::move(text),enabled);
}
void icon(Canvas& c, ui::Rect box, Sprite sprite) {
  const auto& source=regions[static_cast<unsigned>(sprite)];
  const float fit=std::min(box.width/static_cast<float>(source.width),box.height/static_cast<float>(source.height));
  const float width=static_cast<float>(source.width)*fit,height=static_cast<float>(source.height)*fit;
  picture(c,sprite,{box.x+(box.width-width)*.5F,box.y+(box.height-height)*.5F,width,height});
}
void progress(Canvas& c, ui::Rect box, float fraction, Color color) {
  c.fill(box.x,box.y,box.width,box.height,cream);
  c.fill(box.x+1,box.y+1,box.width-2,box.height-2,ink);
  const float fill = (box.width-4)*std::clamp(fraction,0.F,1.F);
  if (fill>0) c.fill(box.x+2,box.y+2,fill,std::max(1.F,box.height-4),color);
}
void pips(Canvas& c, ui::Rect box, unsigned owned, unsigned maximum) {
  if (!maximum) return;
  const bool vertical=box.height>box.width;
  const float diameter = std::min(vertical?box.width:box.height,((vertical?box.height:box.width)-static_cast<float>(maximum-1)*4)/static_cast<float>(maximum));
  if(diameter<=0)return;
  for (unsigned i=0; i<maximum; ++i) {
    const float x = box.x+(vertical?0:static_cast<float>(i)*(diameter+4));
    const float y = box.y+(vertical?static_cast<float>(i)*(diameter+4):0);
    c.fill(x,y,diameter,diameter,ink);
    c.fill(x+1,y+1,diameter-2,diameter-2,cream);
    c.fill(x+2,y+2,diameter-4,diameter-4,i<owned?active:ink);
  }
}
void toggle(Canvas& c, ui::Rect box, std::string text, Sprite symbol, bool on, Command command, bool available) {
  const auto first_surface = c.output.quads.size();
  panel(c,Sprite::cream_panel,box,14,c.button_color(transparent_white,command,available));
  for (auto i=first_surface; i<c.output.quads.size(); ++i) c.output.quads[i].presentation[3] = 1;
  icon(c,{box.x+10,box.y+10,box.height-20,box.height-20},symbol);
  label(c,{box.x+box.height,box.y+12,box.width-box.height-66,26},text,{22,ink,Font::body});
  label(c,{box.x+box.height,box.y+36,box.width-box.height-66,24},available?(on?"ON":"OFF"):"UNAVAILABLE",{14,on?ink:detail::panel});
  c.fill(box.x+box.width-62,box.y+(box.height-26)*.5F,48,26,ink);
  if (on && available) icon(c,{box.x+box.width-40,box.y+(box.height-26)*.5F,26,26},Sprite::check);
  else c.fill(box.x+box.width-60,box.y+(box.height-22)*.5F,22,22,available?cream:detail::muted);
  register_button(c,box,command,std::move(text),available);
}
void wallet(Canvas& c, ui::Rect box, std::uint64_t credits) {
  panel(c,Sprite::navy_panel,box,12);
  icon(c,{box.x+8,box.y+8,32,32},Sprite::credit);
  const auto amount=detail::number(credits);
  label(c,{box.x+43,box.y+6,box.width-51,29},measure(c,amount,14)>box.width-51?short_number(credits):amount,{28,cream,Font::body,true});
  const auto caption=measure(c,"RESEARCH CREDITS",14)>box.width-10?"RESEARCH CR":"RESEARCH CREDITS";
  label(c,{box.x+5,box.y+36,box.width-10,22},caption,{14,active,Font::body,true});
}
void blueprint_progress(Canvas& c, ui::Rect box, const progression::AccountStore& store) {
  const auto blueprints=store.blueprints();
  const progression::BlueprintView* goal=nullptr;
  for(const auto& item:blueprints)
    if(!item.unlocked && (!goal || static_cast<double>(item.progress)/static_cast<double>(item.threshold)>
        static_cast<double>(goal->progress)/static_cast<double>(goal->threshold)))goal=&item;
  panel(c,Sprite::cream_panel,box,10);
  const float width=box.width-20;
  const auto caption=goal?(measure(c,"NEXT BLUEPRINT",14)<=width?"NEXT BLUEPRINT":"BLUEPRINT"):"BLUEPRINTS";
  label(c,{box.x+10,box.y+8,width,22},caption,{14,ink});
  label(c,{box.x+10,box.y+28,width,22},goal?detail::number(goal->progress)+" / "+detail::number(goal->threshold):"COMPLETE",{14,ink});
  progress(c,{box.x+10,box.y+49,width,6},goal?static_cast<float>(goal->progress)/static_cast<float>(goal->threshold):1);
}
Sprite hardware_icon(gameplay::Module module) {
  constexpr std::array<Sprite,16> icons{Sprite::laser,Sprite::rail_lens,Sprite::arc_coil,Sprite::orbit_drones,
      Sprite::emp_ring,Sprite::beam_splitter,Sprite::tractor_lens,Sprite::heavy_winch,Sprite::overtime_reactor,
      Sprite::reserve_battery,Sprite::shield,Sprite::repair_swarm,Sprite::mirror_plating,Sprite::vector_thrusters,
      Sprite::dash,Sprite::bounty_scanner};
  const auto i = static_cast<unsigned>(module);
  return i<icons.size()?icons[i]:Sprite::blueprint;
}
Sprite reward_icon(const gameplay::UpgradeChoice& pick) {
  if(pick.hardware)return hardware_icon(pick.module);
  switch(pick.research) {
    case gameplay::Research::hull_reinforcement:return Sprite::build;
    case gameplay::Research::shield_capacity:return Sprite::shield;
    case gameplay::Research::specimen_value:return Sprite::credit;
    case gameplay::Research::capture_speed:return Sprite::beam;
    case gameplay::Research::emergency_repairs:return Sprite::repair_swarm;
    default:return Sprite::laser;
  }
}
} // namespace human_resources::product::enamel
