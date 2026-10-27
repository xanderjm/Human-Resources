// Explicit host review composition root. Fixtures and storage are memory-only;
// this executable never discovers or reads a player's save.
#include "progression_test_backend.h"
#include <SDL3/SDL.h>
#include <human_resources/product/presentation.h>
#include <human_resources/renderer/style_renderer.h>
#include <array>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace hr = human_resources;
namespace {
struct Case { std::string_view name; hr::product::Screen screen; bool restored{}, build{}, recipes{}, privacy{}, protection{}; unsigned page{},recipe_page{}; };
constexpr std::array<Case,37> cases{{
    {"home",hr::product::Screen::title}, {"flight",hr::product::Screen::run},
    {"pause",hr::product::Screen::pause}, {"saved-run",hr::product::Screen::pause,true},
    {"current-build",hr::product::Screen::pause,false,true}, {"promotion",hr::product::Screen::upgrade},
    {"results",hr::product::Screen::results}, {"hangar",hr::product::Screen::hangar},
    {"signal-lost",hr::product::Screen::death},
    {"blueprints",hr::product::Screen::blueprints}, {"evolutions",hr::product::Screen::blueprints,false,false,true},
    {"settings",hr::product::Screen::settings}, {"privacy",hr::product::Screen::settings,false,false,false,true},
    {"protected-save",hr::product::Screen::pause,false,false,false,false,true},
    {"current-build-2",hr::product::Screen::pause,false,true,false,false,false,1},
    {"current-build-3",hr::product::Screen::pause,false,true,false,false,false,2},
    {"current-build-full",hr::product::Screen::pause,false,true},
    {"current-build-research-full",hr::product::Screen::pause,false,true,false,false,false,1},
    {"evolutions-2",hr::product::Screen::blueprints,false,false,true,false,false,0,1},
    {"evolutions-3",hr::product::Screen::blueprints,false,false,true,false,false,0,2},
    {"evolutions-4",hr::product::Screen::blueprints,false,false,true,false,false,0,3},
    {"evolutions-5",hr::product::Screen::blueprints,false,false,true,false,false,0,4},
    {"privacy-2",hr::product::Screen::settings,false,false,false,true,false,1},
    {"privacy-3",hr::product::Screen::settings,false,false,false,true,false,2},
    {"privacy-4",hr::product::Screen::settings,false,false,false,true,false,3},
    {"privacy-5",hr::product::Screen::settings,false,false,false,true,false,4},
    {"privacy-6",hr::product::Screen::settings,false,false,false,true,false,5},
    {"saved-upgrade",hr::product::Screen::pause,true},
    {"protected-bank",hr::product::Screen::pause,false,false,false,false,true},
    {"protected-retry",hr::product::Screen::pause,false,false,false,false,true},
    {"guest-results",hr::product::Screen::results},
    {"pending-results",hr::product::Screen::results},
    {"flight-overdrive",hr::product::Screen::run},
    {"flight-damage",hr::product::Screen::run},
    {"flight-installed",hr::product::Screen::run},
    {"flight-controller",hr::product::Screen::run},
    {"flight-coaching",hr::product::Screen::run}}};
void check(bool ok, std::string_view reason) {
  if (!ok) throw std::runtime_error(std::string(reason)+": "+SDL_GetError());
}
unsigned dimension(const char* value) {
  unsigned result{};
  const std::string_view text(value);
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
  if (parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || result<240 || result>2048)
    throw std::runtime_error("Review dimensions must be integers from 240 to 2048");
  return result;
}
void capture(const std::filesystem::path& output,const std::string& content,unsigned width,unsigned height,std::string_view selection,float text_scale,hr::ui::Insets safe_area) {
  std::filesystem::create_directories(output);
  std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(
      SDL_CreateWindow("Human Resources isolated UI review",static_cast<int>(width),static_cast<int>(height),SDL_WINDOW_HIDDEN),SDL_DestroyWindow);
  check(window!=nullptr,"review window");
  hr::renderer::StyleRenderer renderer;
  const auto ids=hr::product::content_ids();
  constexpr std::array<std::string_view,8> animated{"cow","pig","sheep","farmer-body","resident-body","infantry-rifleman-body","infantry-shotgunner-body","infantry-rocketeer-body"};
  const auto initial=hr::product::gameplay_framing({0,0,static_cast<float>(width),static_cast<float>(height)},safe_area,1,{width,height});
  check(renderer.initialize(window.get(),content,initial.render_extent.width,initial.render_extent.height,ids,animated),"review renderer");
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore account(backend);
  check(account.load().playable(),"memory account");
  check(account.begin_run().succeeded(),"memory flight");
  const hr::progression::RunReceipt receipt{.run_id=account.account().checkpoint.run_id,.active_milliseconds=222000,.score=790,.captures=42};
  check(account.settle(receipt).succeeded(),"memory receipt");
  hr::gameplay::Game game; game.start({.seed=514});
  auto run=game.snapshot();
  run.stats.level=7; run.stats.score=790; run.stats.active_seconds=222; run.stats.captures=42;
  run.modules[static_cast<unsigned>(hr::gameplay::Module::laser_array)]=2;
  run.modules[static_cast<unsigned>(hr::gameplay::Module::beam_splitter)]=3;
  run.modules[static_cast<unsigned>(hr::gameplay::Module::tractor_lens)]=2;
  run.modules[static_cast<unsigned>(hr::gameplay::Module::shield_generator)]=1;
  // Promotion evidence must use actual eligible offers, not the empty choices
  // on a decorative attract snapshot. The public Game owns the seeded draw.
  hr::gameplay::Game promoted;promoted.start({.seed=514});
  for(unsigned step=0;step<20000 && promoted.snapshot().phase==hr::gameplay::Phase::active;++step)
    promoted.advance(1.F/60,{});
  check(promoted.snapshot().phase==hr::gameplay::Phase::choosing_upgrade,"natural promotion fixture");
  std::ofstream report(output/"capture.csv");
  report<<"screen,width,height,text_scale,quads,enamel_quads,controls,text_overflow,page,page_count,recipe_page_count,safe_left,safe_top,safe_right,safe_bottom\n";
  unsigned captured=0;
  for (const auto& item:cases) {
    if (selection!="all" && (","+std::string(selection)+",").find(","+std::string(item.name)+",")==std::string::npos) continue;
    auto presented=run;
    if(item.name=="current-build-full" || item.name=="current-build-research-full") {
      presented.modules.fill(0);
      for(unsigned i=0;i<6;++i)presented.modules[i]=3;
      presented.research.fill(15);
      presented.evolution_bits=3;
    }
    hr::product::View view{.screen=item.screen,.run=&presented,.store=&account,.viewport={0,0,static_cast<float>(width),static_cast<float>(height)}};
    view.safe_area=safe_area;
    if(item.screen==hr::product::Screen::upgrade || item.name=="saved-upgrade")view.run=&promoted.snapshot();
    view.restored_run=item.restored; view.show_build=item.build; view.show_recipes=item.recipes;
    view.show_privacy=item.privacy; view.save_error=item.protection; view.can_reset_save=item.protection;
    view.save_message=item.protection?"This save cannot be opened in this version. Your original progress is protected.":"";
    view.current_receipt=&receipt;
    view.ui_page=item.page;view.recipe_page=item.recipe_page;view.text_scale=text_scale;
    view.guest=item.name=="guest-results";
    if(item.name=="pending-results")view.pending_results=&receipt;
    if(item.name=="protected-bank") {view.can_reset_save=false;view.can_bank_checkpoint=true;}
    if(item.name=="protected-retry")view.can_reset_save=false;
    if(item.name=="flight-overdrive")presented.harvest_overdrive_seconds=4.2F;
    if(item.name=="flight-damage") {
      presented.harvest_chain=2;presented.harvest_chain_seconds=3.2F;
      view.damage_feedback={.1F,true,hr::gameplay::DamageCause::projectile};
    }
    if(item.name=="flight-installed") {
      view.promotion.installed=&promoted.snapshot().choices[0];view.promotion.reduced_motion=true;
    }
    if(item.name=="flight-controller") {presented.stats.active_seconds=0;view.hint_device=hr::input::DeviceKind::gamepad;}
    if(item.name=="flight-coaching")presented.stats.active_seconds=4;
    const auto context=item.screen==hr::product::Screen::title || item.screen==hr::product::Screen::hangar ?
        hr::product::FramingContext::attract:hr::product::FramingContext::flight;
    const auto framing=hr::product::gameplay_framing(view.viewport,view.safe_area,text_scale,{width,height},context);
    view.framing=&framing;
    auto ui=hr::product::interface(view);
    auto scene=hr::product::scene(*view.run,framing,true,nullptr,view.theme);
    scene.active_delta_seconds=0; // Static UI evidence; no motion qualification.
    scene.screen_quads=ui.quads;
    const auto file=output/(std::string(item.name)+".rgba");
    SDL_IOStream* pixels=SDL_IOFromFile(file.string().c_str(),"wb");
    check(pixels!=nullptr,"review capture output");
    const bool rendered=renderer.render(scene,pixels,false,{width,height});
    SDL_CloseIO(pixels); check(rendered,"review frame");
    const auto enamel=std::count_if(ui.quads.begin(),ui.quads.end(),[](const auto& q){return q.presentation[0]==1;});
    report<<item.name<<','<<width<<','<<height<<','<<text_scale<<','<<ui.quads.size()<<','<<enamel<<','<<ui.buttons.size()<<','<<ui.text_overflow<<','<<ui.page<<','<<ui.page_count<<','<<ui.recipe_page_count<<','<<safe_area.left<<','<<safe_area.top<<','<<safe_area.right<<','<<safe_area.bottom<<'\n';
    ++captured;
  }
  check(renderer.wait_idle(),"review idle");
  check(captured>0,"no matching review screens");
  std::cout<<"Captured "<<captured<<" static UI fixtures using "<<renderer.backend()<<"; memory-only storage.\n";
}
} // namespace
int main(int argc,char** argv) {
  try {
    if (argc!=3 && argc!=5 && argc!=6 && argc!=7 && argc!=8) throw std::runtime_error("Usage: ui_review OUTPUT_DIRECTORY COOKED_GAME_DIRECTORY [WIDTH HEIGHT [SCREEN,SCREEN [TEXT_SCALE [none|phone|phone-classic|narrow]]]]");
    float text_scale=1;
    if(argc>=7) {
      const std::string_view value(argv[6]);
      const auto parsed=std::from_chars(value.data(),value.data()+value.size(),text_scale);
      if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !std::isfinite(text_scale) || text_scale<1 || text_scale>3.4F)
        throw std::runtime_error("Text scale must be finite and from 1 to 3.4");
    }
    hr::ui::Insets safe_area{};
    if(argc==8) {
      const std::string_view profile(argv[7]);
      if(profile=="phone")safe_area={0,59,0,34};
      else if(profile=="phone-classic")safe_area={0,47,0,34};
      else if(profile=="narrow")safe_area={26,20,28,16};
      else if(profile!="none")throw std::runtime_error("Unknown safe-area fixture profile");
    }
    check(SDL_Init(SDL_INIT_VIDEO),"SDL video");
    capture(argv[1],argv[2],argc>=5?dimension(argv[3]):390,argc>=5?dimension(argv[4]):844,argc>=6?argv[5]:"all",text_scale,safe_area);
    SDL_Quit();return 0;
  } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';SDL_Quit();return 1; }
}
