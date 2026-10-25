// Read-only public Game runs. Geometry is measured from consumed body HRMs.
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/presentation/style_scene.h>
#include "footprints.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <tuple>

namespace g=human_resources::gameplay;
namespace p=human_resources::presentation;
struct Box {g::Vec2 center,u,v;float x,z;};
float dot(g::Vec2 a,g::Vec2 b){return a.x*b.x+a.z*b.z;}
Box box(const g::EntityView&e){
  const auto&f=footprints[static_cast<unsigned>(e.kind)];
  const float yaw=std::hypot(e.velocity.x,e.velocity.z)>.08F?std::atan2(e.velocity.x,-e.velocity.z):static_cast<float>(e.id)*1.7F;
  const g::Vec2 u{std::cos(yaw),std::sin(yaw)},v{std::sin(yaw),-std::cos(yaw)};
  return {{e.position.x+u.x*f.cx+v.x*f.cz,e.position.z+u.z*f.cx+v.z*f.cz},u,v,f.x,f.z};
}
bool intersects(const Box&a,const Box&b){
  const g::Vec2 delta{b.center.x-a.center.x,b.center.z-a.center.z};
  for(auto axis:{a.u,a.v,b.u,b.v}){
    const float extent=a.x*std::abs(dot(axis,a.u))+a.z*std::abs(dot(axis,a.v))+b.x*std::abs(dot(axis,b.u))+b.z*std::abs(dot(axis,b.v));
    if(std::abs(dot(delta,axis))>=extent)return false;
  }return true;
}
bool visible(const g::EntityView&e,const g::Snapshot&s,const p::Matrix&m){
  const float x=e.position.x,y=.5F,z=static_cast<float>(s.runner_distance)-e.position.z+2.3F;
  if(std::abs(z)>32)return false;
  const float cx=m[0]*x+m[1]*y+m[2]*z+m[3],cy=m[4]*x+m[5]*y+m[6]*z+m[7];
  const float cz=m[8]*x+m[9]*y+m[10]*z+m[11],w=m[12]*x+m[13]*y+m[14]*z+m[15];
  return std::abs(cx)<=w && std::abs(cy)<=w && cz>=0 && cz<=w;
}
g::Control controls(const g::Snapshot&s,unsigned policy){
  if(!policy)return {};
  g::Vec2 goal{0,static_cast<float>(s.runner_distance)+1};float best=1e9F;
  for(const auto&e:s.entities)if(e.kind<=g::EntityKind::pig){
    const float dx=e.position.x-s.player.x,dz=e.position.z-s.player.z,d=dx*dx+dz*dz;
    if(d<best){best=d;goal=e.position;}
  }
  return {{(goal.x-s.player.x)*1.3F,(goal.z-s.player.z)*1.3F},false};
}
std::size_t draft(const g::Snapshot&s){
  float best=-1;std::size_t index=0;
  for(std::size_t i=0;i<3;++i){const auto&c=s.choices[i];float value=c.hardware?10.F:1.F;
    if(c.hardware&&(c.module==g::Module::beam_splitter||c.module==g::Module::tractor_lens))value=30;
    if(!c.evolution_preview.empty())value+=50;
    if(value>best){best=value;index=i;}
  }return index;
}
struct Episode {unsigned samples{},a_kind{},b_kind{};double start{};float minimum{1000},penetration{};};
int main(int argc,char**argv){
  if(argc!=2)return 2;
  std::ofstream trace(argv[1]);trace<<"seed,policy,seconds,a_id,a_kind,b_id,b_kind,distance,core_sum,box_overlap,a_x,a_z,b_x,b_z\n";
  p::StyleFrame frame;p::set_overhead_camera(frame,588.F/1280.F,20.5F,{},27.F);
  for(unsigned policy=0;policy<2;++policy)for(unsigned seed:{1U,21U,42U,514U}){
    g::Game game;game.start({.seed=seed});
    unsigned samples=0,box_frames=0,core_frames=0,box_pair_samples=0,core_pair_samples=0,peak_core=0,long_episodes=0,episodes=0;
    double maximum_duration=0;float maximum_penetration=0;
    using Pair=std::pair<unsigned,unsigned>;std::map<Pair,Episode>active;
    const auto finish=[&](const Pair&pair,const Episode&e){
      ++episodes;const double duration=e.samples*.1;maximum_duration=std::max(maximum_duration,duration);
      if(duration>=1){++long_episodes;std::cout<<"EPISODE seed="<<seed<<" policy="<<policy<<" ids="<<pair.first<<','<<pair.second<<" kinds="<<e.a_kind<<','<<e.b_kind<<" start="<<e.start<<" duration="<<duration<<" minimum_distance="<<e.minimum<<" core_penetration_fraction="<<e.penetration<<'\n';}
    };
    for(unsigned tick=0;tick<60*120&&game.snapshot().phase!=g::Phase::dead;++tick){
      while(game.snapshot().phase==g::Phase::choosing_upgrade)(void)game.choose_upgrade(draft(game.snapshot()));
      game.advance(1.F/60,controls(game.snapshot(),policy));if(tick%6!=5)continue;
      const auto&s=game.snapshot();++samples;
      std::vector<const g::EntityView*>bodies;
      for(const auto&e:s.entities)if(e.kind<=g::EntityKind::soldier&&e.capture_progress==0&&visible(e,s,frame.camera))bodies.push_back(&e);
      unsigned boxes=0,cores=0;std::set<Pair>now;
      for(std::size_t i=0;i<bodies.size();++i)for(std::size_t j=i+1;j<bodies.size();++j){
        const auto&a=*bodies[i],&b=*bodies[j];const float d=std::hypot(a.position.x-b.position.x,a.position.z-b.position.z);
        const float radii=footprints[static_cast<unsigned>(a.kind)].core+footprints[static_cast<unsigned>(b.kind)].core;
        const bool overlap=intersects(box(a),box(b));boxes+=overlap;
        if(overlap||d<radii)trace<<seed<<','<<policy<<','<<std::setprecision(9)<<s.stats.active_seconds<<','<<a.id<<','<<static_cast<unsigned>(a.kind)<<','<<b.id<<','<<static_cast<unsigned>(b.kind)<<','<<d<<','<<radii<<','<<overlap<<','<<a.position.x<<','<<a.position.z<<','<<b.position.x<<','<<b.position.z<<'\n';
        if(d>=radii)continue;
        ++cores;Pair pair{std::min(a.id,b.id),std::max(a.id,b.id)};now.insert(pair);
        auto&ep=active[pair];if(!ep.samples){ep.start=s.stats.active_seconds;ep.a_kind=static_cast<unsigned>(a.kind);ep.b_kind=static_cast<unsigned>(b.kind);}
        ++ep.samples;ep.minimum=std::min(ep.minimum,d);ep.penetration=std::max(ep.penetration,(radii-d)/radii);
        maximum_penetration=std::max(maximum_penetration,ep.penetration);
      }
      for(auto it=active.begin();it!=active.end();){if(!now.contains(it->first)){finish(it->first,it->second);it=active.erase(it);}else ++it;}
      box_frames+=boxes>0;core_frames+=cores>0;box_pair_samples+=boxes;core_pair_samples+=cores;peak_core=std::max(peak_core,cores);
    }
    for(const auto&[pair,ep]:active)finish(pair,ep);
    std::cout<<"RUN seed="<<seed<<" policy="<<policy<<" active_seconds="<<game.snapshot().stats.active_seconds<<" samples="<<samples<<" box_frames="<<box_frames<<" core_frames="<<core_frames<<" box_pair_samples="<<box_pair_samples<<" core_pair_samples="<<core_pair_samples<<" core_episodes="<<episodes<<" episodes_ge1s="<<long_episodes<<" longest_seconds="<<maximum_duration<<" maximum_penetration="<<maximum_penetration<<" peak_core_pairs="<<peak_core<<'\n';
  }
}
