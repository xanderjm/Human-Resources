#include "progression_test_backend.h"
#include "test_support.h"
#include "enamel_components.h"
#include <human_resources/gameplay/tuning.h>
#include <algorithm>
#include <set>
#include <limits>
#include <iostream>

namespace hr = human_resources;
namespace e = hr::product::enamel;
using test_support::require;
int main() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store(backend);
  require(store.load().playable(),"isolated component account");
  hr::gameplay::Game game;game.start();
  hr::product::View view{.run=&game.snapshot(),.store=&store,.viewport={0,0,390,844}};
  hr::product::detail::Canvas card(view);
  e::panel(card,e::Sprite::cream_panel,{20,100,350,180});
  require(card.output.quads.size()==9,"nine-slice keeps one shared frame without distorting its corners");
  for (const auto& q:card.output.quads) {
    require(q.presentation[0]==1,"components use the checked enamel atlas page");
    require(q.rect[0]*390>=20 && q.rect[1]*844>=100,"slice stays inside its requested component");
    require(q.rect[2]>=0 && q.rect[3]>=0,"slice has nonnegative geometry");
  }
  for(const auto sprite:{e::Sprite::cream_panel,e::Sprite::navy_panel,e::Sprite::blueprint_tile})
    for(const auto box:{hr::ui::Rect{20,40,350,54},hr::ui::Rect{20,40,160,600}}) {
      hr::product::detail::Canvas sliced(view);
      e::panel(sliced,sprite,box,16);
      require(sliced.output.quads.size()==9,"short and tall frames retain nine independent slices");
      const auto& corner=sliced.output.quads.front();
      const float sx=corner.rect[2]*390/(corner.uv[2]*e::atlas_width);
      const float sy=corner.rect[3]*844/(corner.uv[3]*e::atlas_height);
      require(std::abs(sx-sy)<.0001F,"fixed corner art keeps one pixel scale on both axes");
      for(unsigned i:{0U,2U,6U,8U}) {
        const auto& q=sliced.output.quads[i];
        require(std::abs(q.rect[2]*390/(q.uv[2]*e::atlas_width)-sx)<.0001F &&
            std::abs(q.rect[3]*844/(q.uv[3]*e::atlas_height)-sy)<.0001F,"all four riveted corners preserve the same scale");
      }
    }
  {
    hr::product::detail::Canvas capsule(view);
    e::panel(capsule,e::Sprite::secondary,{20,40,350,54});
    require(capsule.output.quads.size()==3,"capsule has three real slices with no fabricated zero-source edge strips");
    for(const auto& q:capsule.output.quads)require(q.uv[2]>0 && q.uv[3]>0,"every emitted slice samples a nonempty source region");
  }
  {
    hr::product::detail::Canvas pictogram(view);
    e::icon(pictogram,{20,40,40,40},e::Sprite::farmland);
    const auto& q=pictogram.output.quads.front();
    const auto& source=e::regions[static_cast<unsigned>(e::Sprite::farmland)];
    require(std::abs((q.rect[2]*390)/(q.rect[3]*844)-static_cast<float>(source.width)/static_cast<float>(source.height))<.0001F,"shared pictograms preserve their authored aspect instead of stretching to the tile");
  }
  auto draw=[&](hr::product::Command held) {
    view.held_command=held;
    hr::product::detail::Canvas c(view);
    e::button(c,{20,600,350,130},"START RUN",hr::product::Command::start,e::ButtonStyle::primary);
    return c.output;
  };
  const auto idle=draw(hr::product::Command::none),pressed=draw(hr::product::Command::start);
  require(idle.buttons.size()==1 && pressed.buttons.size()==1,"one durable semantic action per component");
  require(idle.quads.size()==pressed.quads.size(),"pressure adds no geometry");
  bool changed=false;
  for (std::size_t i=0;i<idle.quads.size();++i) {
    require(idle.quads[i].rect==pressed.quads[i].rect && idle.quads[i].uv==pressed.quads[i].uv,"pressure retains visual geometry and glyph placement");
    changed|=idle.quads[i].color!=pressed.quads[i].color;
  }
  require(changed,"a held action has a visible surface response");
  require(!idle.text_overflow,"ordinary saucer action fits its live label");
  require(e::measure(card,"1111",24)==e::measure(card,"8888",24),"upright label numbers retain tabular advances");
  require(e::short_number(std::numeric_limits<std::uint64_t>::max())=="18E","bounded counters cannot overflow their suffix bank");
  const auto require_shared=[&](const hr::product::View& sample) {
    const auto ui=hr::product::interface(sample);
    if(ui.text_overflow || std::none_of(ui.quads.begin(),ui.quads.end(),[](const auto& q){return q.presentation[0]==1;}))
      std::cerr<<"Shared variant screen="<<static_cast<unsigned>(sample.screen)<<" viewport="<<sample.viewport.width<<"x"<<sample.viewport.height<<" scale="<<sample.text_scale<<" page="<<sample.ui_page<<" privacy="<<sample.show_privacy<<" recipe="<<sample.show_recipes<<" overflow="<<ui.text_overflow<<"\n";
    require(!ui.text_overflow && std::any_of(ui.quads.begin(),ui.quads.end(),[](const auto& q){return q.presentation[0]==1;}),"all screen variants must consume the shared component atlas without overflow");
    const auto safe=hr::ui::inset(sample.viewport,sample.safe_area);
    std::set<hr::product::Command> commands;
    for(std::size_t i=0;i<ui.buttons.size();++i) {
      const auto& b=ui.buttons[i];
      require(commands.insert(b.command).second,"compact controls retain unique semantic identities");
      require(b.bounds.width>=44 && b.bounds.height>=44 && b.bounds.x>=safe.x && b.bounds.y>=safe.y && b.bounds.right()<=safe.right()+.01F && b.bounds.bottom()<=safe.bottom()+.01F,"compact actions retain complete physical touch targets in the safe viewport");
      for(std::size_t j=i+1;j<ui.buttons.size();++j) {
        const auto overlap=hr::ui::intersect(b.bounds,ui.buttons[j].bounds);
        require(overlap.width<.01F || overlap.height<.01F,"compact hit targets never overlap");
      }
    }
    return ui;
  };
  for(const auto screen:{hr::product::Screen::title,hr::product::Screen::pause,hr::product::Screen::hangar,
      hr::product::Screen::blueprints,hr::product::Screen::settings,hr::product::Screen::results}) {
    auto sample=view;sample.viewport={0,0,320,568};sample.safe_area={0,20,0,0};sample.screen=screen;
    require_shared(sample);
    if(screen==hr::product::Screen::pause) {
      sample.show_build=true;
      for(unsigned page=0;page<3;++page){sample.ui_page=page;require_shared(sample);}
    }
    if(screen==hr::product::Screen::blueprints) {
      sample.show_recipes=true;
      for(unsigned page=0;page<hr::gameplay::tuning::active_evolutions.size();++page) {
        sample.recipe_page=page;
        const auto ui=require_shared(sample);
        require(ui.recipe_page_count==hr::gameplay::tuning::active_evolutions.size(),"each compact spread has one complete recipe and all five remain navigable");
      }
    }
  }
  for (const auto screen:{hr::product::Screen::title,hr::product::Screen::pause}) {
    view.screen=screen;view.held_command=hr::product::Command::none;
    hr::progression::RunReceipt receipt{.run_id=1,.active_milliseconds=45000,.score=80};
    view.current_receipt=&receipt;
    const auto ui=hr::product::interface(view);
    require(std::any_of(ui.quads.begin(),ui.quads.end(),[](const auto& q){return q.presentation[0]==1;}),"Home and Pause use the live component owner");
    std::set<hr::product::Command> identities;
    for (const auto& b:ui.buttons) {
      require(identities.insert(b.command).second,"a screen exposes each action identity once");
      require(b.bounds.width>=44 && b.bounds.height>=44,"shared controls keep physical touch targets");
    }
  }
  for(const auto phone:{hr::ui::Rect{0,0,320,568},hr::ui::Rect{0,0,390,844}}) {
    auto run=game.snapshot();run.stats.active_seconds=30;
    run.hull=run.maximum_hull=run.shield=run.maximum_shield=9999;
    run.stats.level=std::numeric_limits<std::uint32_t>::max();run.stats.score=std::numeric_limits<std::uint64_t>::max();
    hr::product::View flight{hr::product::Screen::run,&run,&store,phone,{0,20,0,0}};
    for(unsigned state=0;state<4;++state) {
      run.harvest_chain=state==1?2:0;run.harvest_overdrive_seconds=state==2?4.2F:0;
      flight.damage_feedback=state==3?hr::product::DamageFeedback{.1F,true,hr::gameplay::DamageCause::projectile}:hr::product::DamageFeedback{};
      const auto ui=hr::product::interface(flight);
      if(ui.hud_bottom==0 || ui.text_overflow)std::cerr<<"HUD fixture width="<<phone.width<<" state="<<state<<"\n";
      require(ui.hud_bottom>0 && !ui.text_overflow,"ordinary small and tall phone states actually use the shared three-pod HUD at supported maxima");
      require(ui.buttons.size()==2,"shared HUD retains exactly Pause and Dash");
    }
  }
  hr::gameplay::Game earned;earned.start({.seed=514});
  for(unsigned step=0;step<20000 && earned.snapshot().phase==hr::gameplay::Phase::active;++step)earned.advance(1.F/60,{});
  require(earned.snapshot().phase==hr::gameplay::Phase::choosing_upgrade,"public Game reaches a natural promotion for component wiring");
  view={.screen=hr::product::Screen::upgrade,.run=&earned.snapshot(),.store=&store,.viewport={0,0,390,844},.safe_area={0,20,0,0}};
  const auto promotion=hr::product::interface(view);
  require(!promotion.text_overflow && std::any_of(promotion.quads.begin(),promotion.quads.end(),[](const auto& q){return q.presentation[0]==1 && q.presentation[3]==1;}),"a real natural draw uses shared choice frames rather than a legacy fit fallback");
  view.viewport={0,0,320,568};
  require_shared(view);
  for(const auto viewport:{hr::ui::Rect{0,0,320,568},hr::ui::Rect{0,0,390,844},hr::ui::Rect{0,0,430,932}})
    for(const float scale:{1.F,1.3F,2.F,3.4F})for(unsigned screen=0;screen<9;++screen) {
      hr::product::View sample{.screen=static_cast<hr::product::Screen>(screen),
          .run=screen==static_cast<unsigned>(hr::product::Screen::upgrade)?&earned.snapshot():&game.snapshot(),
          .store=&store,.viewport=viewport,.safe_area={0,20,0,0}};
      sample.text_scale=scale;
      const auto first=require_shared(sample);
      for(unsigned page=1;page<first.page_count;++page){sample.ui_page=page;require_shared(sample);}
      if(sample.screen==hr::product::Screen::settings) {
        sample.show_privacy=true;sample.ui_page=0;
        const auto policy=require_shared(sample);
        for(unsigned page=1;page<policy.page_count;++page){sample.ui_page=page;require_shared(sample);}
      }
      if(scale==1 && sample.screen==hr::product::Screen::hangar)
        require(first.page_count==1 && first.buttons.size()==7,"portrait Hangar keeps all four purchases, flight, library and Back directly available");
      if(scale==1 && sample.screen==hr::product::Screen::upgrade)
        require(first.page_count==1 && std::count_if(first.buttons.begin(),first.buttons.end(),[](const auto& b){return b.command>=hr::product::Command::pick0 && b.command<=hr::product::Command::pick2;})==3,"portrait earned promotion retains three immediate choices");
    }
  // Safe-area scaling must not change recipe navigation on a longer effect.
  for(const auto viewport:{hr::ui::Rect{0,0,320,694},hr::ui::Rect{0,0,390,844},hr::ui::Rect{0,0,430,932}}) {
    hr::product::View sample{.screen=hr::product::Screen::blueprints,.run=&game.snapshot(),.store=&store,
        .viewport=viewport,.safe_area={0,59,0,34}};
    sample.show_recipes=true;
    unsigned count=0,text_pages=0;
    bool spread_navigation=false;
    for(unsigned page=0;page<5;++page) {
      sample.recipe_page=page;
      const auto recipe=require_shared(sample);
      const bool spread=std::any_of(recipe.buttons.begin(),recipe.buttons.end(),[](const auto& b){return b.command==hr::product::Command::recipe_next;});
      if(!page){count=recipe.recipe_page_count;text_pages=recipe.page_count;spread_navigation=spread;}
      require(recipe.recipe_page_count==count && recipe.page_count==text_pages && spread==spread_navigation,"all ordinary recipe spreads retain one consistent navigation model at the same safe-area size");
    }
  }
  for(const auto insets:{hr::ui::Insets{0,47,0,34},hr::ui::Insets{0,59,0,34}})
    for(const auto screen:{hr::product::Screen::title,hr::product::Screen::blueprints,hr::product::Screen::results}) {
      hr::product::View sample{.screen=screen,.run=&game.snapshot(),.store=&store,.viewport={0,0,390,844},.safe_area=insets};
      const auto ui=require_shared(sample);
      const auto contains=[&](e::Sprite sprite) {
        const auto& source=e::regions[static_cast<unsigned>(sprite)];
        return std::any_of(ui.quads.begin(),ui.quads.end(),[&](const auto& q){
          return q.presentation[0]==1 && std::abs(q.uv[0]-static_cast<float>(source.x)/e::atlas_width)<.00001F &&
              std::abs(q.uv[1]-static_cast<float>(source.y)/e::atlas_height)<.00001F &&
              std::abs(q.uv[2]-static_cast<float>(source.width)/e::atlas_width)<.00001F &&
              std::abs(q.uv[3]-static_cast<float>(source.height)/e::atlas_height)<.00001F;
        });
      };
      require(ui.page_count==1 && (contains(e::Sprite::logo)||contains(e::Sprite::title_frame)||contains(e::Sprite::beam)),"ordinary phone safe areas retain their reference sign/logo composition rather than a reading-mode substitution");
      require(ui.buttons.size()==(screen==hr::product::Screen::title?5U:2U),"safe-area compositions retain all direct screen actions");
    }
  for (const auto theme:{hr::presentation::SceneryTheme::farmland,hr::presentation::SceneryTheme::suburbia,hr::presentation::SceneryTheme::desert})
    require(hr::product::next_location(hr::product::previous_location(theme))==theme,"left and right destination actions are inverse");
  return 0;
}
