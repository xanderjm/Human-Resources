#include "test_support.h"
#include <human_resources/animation/animation.h>
#include <human_resources/renderer/animation_instances.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using test_support::require;
namespace {
using namespace human_resources::animation;
using Bytes = std::vector<std::byte>;
Bytes read(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  require(stream.good(), "military animation evidence opens");
  const auto size = stream.tellg();
  require(size > 0 && size <= static_cast<std::streamoff>(max_asset_bytes), "military evidence byte bound");
  Bytes result(static_cast<std::size_t>(size));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(size));
  require(stream.good(), "military evidence read complete");
  return result;
}
class SocketReference final {
public:
  explicit SocketReference(Bytes bytes) : bytes_(std::move(bytes)) {}
  std::uint32_t integer() {
    require(at_ + 4 <= bytes_.size(), "socket reference bounded integer");
    std::uint32_t value{};
    for (unsigned k = 0; k < 4; ++k)
      value |= std::to_integer<std::uint32_t>(bytes_[at_++]) << (8 * k);
    return value;
  }
  float number() {
    const float value = std::bit_cast<float>(integer());
    require(std::isfinite(value), "socket reference finite matrix");
    return value;
  }
  std::string name() {
    std::string value;
    bool ended = false;
    for (unsigned k = 0; k < 32; ++k) {
      require(at_ < bytes_.size(), "socket reference bounded name");
      const char c = static_cast<char>(bytes_[at_++]);
      if (c == 0)
        ended = true;
      else {
        require(!ended, "socket reference padded name");
        value += c;
      }
    }
    require(!value.empty(), "socket reference named node");
    return value;
  }
  bool ended() const { return at_ == bytes_.size(); }
private:
  Bytes bytes_;
  std::size_t at_{};
};
void verify_sockets(const std::filesystem::path& assets, const std::filesystem::path& references) {
  for (const auto* id : {"infantry-rifleman-body", "infantry-shotgunner-body", "infantry-rocketeer-body"}) {
    Asset asset;
    require(decode(read(assets / (std::string(id) + ".hra")), asset).valid(), "infantry HRA decodes");
    require(asset.clips().size() == 14, "all authored infantry clips retained");
    SocketReference source(read(references / (std::string(id) + ".hws")));
    require(source.integer() == 0x31535748U, "test-only HWS1 evidence header");
    const auto samples = source.integer(), count = source.integer();
    require(samples > 0 && samples < 10000 && count >= 9 && count <= 16, "socket reference sample bounds");
    std::vector<std::string> names;
    for (std::uint32_t n = 0; n < count; ++n) {
      auto name = source.name();
      require(asset.find_node(name) < asset.nodes().size(), "every source socket exists in cooked asset");
      require(std::find(names.begin(), names.end(), name) == names.end(), "socket evidence has distinct nodes");
      names.push_back(std::move(name));
    }
    Player player;
    require(player.bind(asset), "infantry player binds");
    float maximum_error{};
    std::string maximum_node;
    std::uint32_t maximum_clip{};
    float maximum_time{};
    for (std::uint32_t n = 0; n < samples; ++n) {
      const auto clip = source.integer();
      const float seconds = source.number();
      require(clip < asset.clips().size(), "socket reference clip exists");
      require(player.play(asset.clips()[clip].name, 0) && player.seek(seconds), "native source subframe sampled");
      for (const auto& name : names) {
        const auto* pose = player.node_transform(name);
        require(pose != nullptr, "native named socket frame exists");
        for (const float value : *pose) {
          const float error = std::abs(value - source.number());
          if (error > maximum_error) {
            maximum_error = error;
            maximum_node = name;
            maximum_clip = clip;
            maximum_time = seconds;
          }
        }
      }
    }
    require(source.ended(), "socket reference consumed exactly");
    std::cout << "SOCKET_PARITY " << id << " samples=" << samples << " nodes=" << count
              << " maximum_matrix_error=" << maximum_error << " node=" << maximum_node
              << " clip=" << maximum_clip << " time=" << maximum_time << std::endl;
    require(maximum_error < .0001F, "head/chest/palms/weapon/muzzles match source keys and half-keys");
  }
}
void planted_aim_and_events(const std::filesystem::path& assets) {
  namespace renderer=human_resources::renderer;
  namespace mesh=human_resources::assets;
  for(const auto* id:{"infantry-rifleman-body","infantry-shotgunner-body","infantry-rocketeer-body"}) {
    const auto bytes=read(assets/(std::string(id)+".hra"));
    Asset asset; require(decode(bytes,asset).valid(),"final infantry source decodes");
    Player reference; require(reference.bind(asset,"aim"),"authored planted aim binds");
    const auto weapon=asset.find_node("socket_weapon");
    auto muzzle=asset.find_node("socket_muzzle");
    if(muzzle>=asset.nodes().size()) muzzle=asset.find_node("socket_muzzle_left");
    if(muzzle>=asset.nodes().size()) muzzle=asset.find_node("socket_nose");
    require(muzzle<asset.nodes().size(),"actual source muzzle found");
    const auto& origin=reference.pose().world[muzzle];
    const auto& axis=reference.pose().world[weapon];
    Vec3 emitter{origin[12],origin[13],origin[14]};
    const auto second_muzzle=asset.find_node("socket_muzzle_right");
    if(second_muzzle<asset.nodes().size()) for(std::size_t k=0;k<3;++k) emitter[k]=(emitter[k]+reference.pose().world[second_muzzle][12+k])*.5F;
    for(float distance:{3.F,6.F,12.F}) for(float height:{2.2F,2.5F}) {
      const Vec3 target{emitter[0]+axis[8]*distance,height,emitter[2]+axis[10]*distance};
      Pose aimed;
      require(aim_infantry_pose(asset,reference.pose(),reference.pose(),target,aimed),"bounded chest elevation composes");
      Vec3 aimed_muzzle{};
      for(std::size_t k=0;k<3;++k) {
        aimed_muzzle[k]=aimed.world[muzzle][12+k];
        if(second_muzzle<asset.nodes().size()) aimed_muzzle[k]=(aimed_muzzle[k]+aimed.world[second_muzzle][12+k])*.5F;
      }
      float dot{},ray_length{},forward_length{};
      for(std::size_t k=0;k<3;++k) {
        const float ray=target[k]-aimed_muzzle[k],forward=aimed.world[weapon][8+k];
        dot+=ray*forward;ray_length+=ray*ray;forward_length+=forward*forward;
      }
      require(dot/std::sqrt(ray_length*forward_length)>.999999F,"actual elevated muzzle axis intersects the reviewed target plane");
      for(const char* name:{"foot.L","foot.R","thigh.L","thigh.R"}) {
        const auto node=asset.find_node(name);
        require(node<asset.nodes().size() && aimed.world[node]==reference.pose().world[node],"elevation preserves lower-body source transforms exactly");
      }
      for(const auto& pair:{std::pair{"socket_palm.L","socket_support"},std::pair{"socket_palm.R","socket_primary"}}) {
        const auto a=asset.find_node(pair.first),b=asset.find_node(pair.second);
        float gap{}; for(std::size_t k=0;k<3;++k) gap+=std::pow(aimed.world[a][12+k]-aimed.world[b][12+k],2.F);
        require(std::sqrt(gap)<.002F,"composed elevation preserves both real grip contacts");
      }
      std::vector<DeformedVertex> skin(asset.vertices().size());
      require(skin_vertices(asset,aimed,skin),"elevated skin evaluates");
      require(std::all_of(skin.begin(),skin.end(),[](const auto& v){return v.position[1]>=-.0001F;}),"elevated body remains grounded");
    }
    mesh::PreviewMesh companion;
    require(mesh::decode_preview_mesh(read(assets/(std::string(id)+".hrm")),companion),"matching final companion decodes");
    renderer::AnimationInstances cache;
    require(cache.load(id,bytes,companion.vertices),"equipped pair validates aim/fire and named sockets at startup");
    renderer::AnimationIntent intent{.warning=true,.enabled=true};
    intent.aim_target={origin[12]+axis[8]*6,2.5F,origin[14]+axis[10]*6};
    cache.begin_frame(); auto* actor=cache.update(1,id,intent,0);
    require(actor && actor->clip=="aim_enter" && actor->has_head && actor->has_weapon,"planted warning begins authored aiming and both sockets"); cache.end_frame();
    for(unsigned k=0;k<3;++k) {cache.begin_frame();actor=cache.update(1,id,intent,.25F);require(actor!=nullptr,"authored aim advances");cache.end_frame();}
    intent.warning=false; intent.fire_sequence=42; intent.recovering=true;
    cache.begin_frame();actor=cache.update(1,id,intent,.016F);
    require(actor && actor->clip=="fire" && actor->launch_sequence==42,"typed enemy-shot outcome starts authored fire once");
    const auto launch=actor->launch_weapon_delta,gun=actor->weapon_delta;
    const float clock=actor->clip_time;cache.end_frame();
    cache.begin_frame();actor=cache.update(1,id,intent,0);
    require(actor && actor->clip_time==clock && actor->weapon_delta==gun && actor->launch_weapon_delta==launch,"pause freezes pose and firing-time launch matrix");cache.end_frame();
    intent.fire_age_seconds=.1F;
    cache.begin_frame();actor=cache.update(1,id,intent,.1F);
    require(actor && actor->clip_time>clock && actor->launch_weapon_delta==launch && actor->weapon_delta!=gun,"post-shot recoil moves the gun without steering emitted projectiles");cache.end_frame();
    cache.clear_actors(); intent.fire_sequence=0; intent.fire_age_seconds=.4F;
    cache.begin_frame();actor=cache.update(1,id,intent,0);
    require(actor && actor->clip=="fire" && std::abs(actor->clip_time-.4F)<1e-6F && actor->launch_sequence==0,"cold restored recovery shows aged source pose without inventing a shot");cache.end_frame();
    intent.capturing=true;
    cache.begin_frame();actor=cache.update(1,id,intent,.016F);
    require(actor && actor->clip=="beam_lift","capture preempts authored fire recovery");cache.end_frame();
  }
}
void live_and_cold_preparation(const std::filesystem::path& assets) {
  namespace renderer=human_resources::renderer;
  namespace mesh=human_resources::assets;
  for(const auto* id:{"infantry-rifleman-body","infantry-shotgunner-body","infantry-rocketeer-body"}) {
    const auto bytes=read(assets/(std::string(id)+".hra"));
    Asset asset; require(decode(bytes,asset).valid(),"preparation source decodes");
    Player reference; require(reference.bind(asset,"aim"),"preparation target uses actual source frame");
    const auto weapon=asset.find_node("socket_weapon");
    const auto& axis=reference.pose().world[weapon];
    mesh::PreviewMesh companion;
    require(mesh::decode_preview_mesh(read(assets/(std::string(id)+".hrm")),companion),"preparation companion decodes");
    renderer::AnimationInstances live,cold;
    require(live.load(id,bytes,companion.vertices) && cold.load(id,bytes,companion.vertices),"live and cold equipped sources load");
    renderer::AnimationIntent intent{.speed=3.2F,.enabled=true};
    intent.aim_target={axis[12]+axis[8]*6,2.5F,axis[14]+axis[10]*6};
    live.begin_frame();auto* actor=live.update(901,id,intent,.1F);
    require(actor && actor->clip=="walk","live preparation begins after a real moving gait");live.end_frame();
    intent.speed=0;intent.preparing=true;
    for(unsigned tick=0;tick<=72;++tick) {
      const float age=static_cast<float>(tick)/60;
      intent.aim_pose_seconds=age;
      intent.preparing=age<.5F;
      intent.warning=!intent.preparing;
      live.begin_frame();actor=live.update(901,id,intent,1.F/60);
      require(actor && actor->clip==(age<.5F ? "aim_enter" : "aim"),"actual raise progresses into held aim without restarting on warning");
      if(tick==0 || tick==1 || tick==15 || tick==29 || tick==30 || tick==31 || tick==54 || tick==72) {
        cold.clear_actors();cold.begin_frame();const auto* restored=cold.update(901,id,intent,0);
        require(restored && restored->clip==actor->clip && restored->clip_time==actor->clip_time,
            "fresh cold phase samples the actual live preparation clock");
        require(restored->head_delta==actor->head_delta && restored->weapon_delta==actor->weapon_delta,
            "cold preparation reproduces live head and weapon sockets, including mid-enter");
        require(restored->launch_sequence==0 && actor->launch_sequence==0,"preparation invents no firing event");
        require(restored->vertices.size()==actor->vertices.size(),"cold and live skin topology agree");
        for(std::size_t v=0;v<actor->vertices.size();++v) {
          require(restored->vertices[v].position==actor->vertices[v].position && restored->vertices[v].normal==actor->vertices[v].normal,
              "cold preparation reproduces every live skinned position and normal");
        }
        cold.end_frame();
      }
      live.end_frame();
    }
    const auto gun=actor->weapon_delta;
    const float clock=actor->clip_time;
    intent.aim_pose_seconds=2.F;
    live.begin_frame();actor=live.update(901,id,intent,0);
    require(actor && actor->weapon_delta==gun && actor->clip_time==clock,"zero active time freezes an existing authoritative aim pose");live.end_frame();
    intent.preparing=false;intent.warning=false;intent.aim_pose_seconds.reset();
    live.begin_frame();actor=live.update(901,id,intent,1.F/60);
    require(actor && actor->clip=="aim_exit","cancelled preparation lowers the weapon using its existing authored exit");live.end_frame();
    intent.preparing=true;intent.aim_pose_seconds=.2F;intent.capturing=true;
    live.begin_frame();actor=live.update(901,id,intent,1.F/60);
    require(actor && actor->clip=="beam_lift","capture still preempts an authoritative preparation pose");live.end_frame();
    cold.clear_actors();intent.capturing=false;intent.preparing=false;intent.warning=true;intent.recovering=true;intent.fire_age_seconds=.4F;
    cold.begin_frame();actor=cold.update(901,id,intent,0);
    require(actor && actor->clip=="fire" && std::abs(actor->clip_time-.4F)<1e-6F,"real fire recovery takes priority over an aged aim intent");cold.end_frame();
    cold.clear_actors();intent.recovering=false;intent.preparing=true;intent.aim_pose_seconds.reset();
    cold.begin_frame();require(!cold.update(901,id,intent,0),"preparation without an actual phase age is rejected");cold.end_frame();
  }
}
void physical_turn_feet(const std::filesystem::path& assets) {
  namespace renderer=human_resources::renderer;
  namespace mesh=human_resources::assets;
  for(const char* id:{"infantry-rifleman-body","infantry-shotgunner-body","infantry-rocketeer-body"}) {
    const auto bytes=read(assets/(std::string(id)+".hra"));Asset asset;
    require(decode(bytes,asset).valid(),"turn source decodes");
    Player source;require(source.bind(asset,"aim"),"leg-only solve reference binds");
    std::array<Matrix,2> targets{source.pose().world[asset.find_node("foot.L")],source.pose().world[asset.find_node("foot.R")]};
    targets[0][13]+=.08F;targets[0][14]+=.025F;
    std::array<Vec3,2> knees;for(std::size_t side=0;side<2;++side) {const auto& k=source.pose().world[asset.find_node(side==0 ? "shin.L":"shin.R")];knees[side]={k[12],k[13],k[14]};}
    Pose solved;require(place_infantry_feet(asset,source.pose(),targets,knees,solved),"reachable ankle solve succeeds");
    for(std::size_t n=0;n<asset.nodes().size();++n) {
      const auto& name=asset.nodes()[n].name;
      const bool leg=name=="thigh.L" || name=="shin.L" || name=="foot.L" || name=="thigh.R" || name=="shin.R" || name=="foot.R";
      if(!leg) {
        require(solved.local[n].translation==source.pose().local[n].translation && solved.local[n].rotation==source.pose().local[n].rotation &&
            solved.local[n].scale==source.pose().local[n].scale && solved.world[n]==source.pose().world[n],"all nonleg locals and world frames remain bit exact");
      }
    }
    for(std::size_t side=0;side<2;++side)for(std::size_t k=0;k<3;++k)
      require(std::abs(solved.world[asset.find_node(side==0 ? "foot.L":"foot.R")][12+k]-targets[side][12+k])<.0001F,"ankle reaches finite authored target");
    auto unreachable=targets;unreachable[0][13]=100;
    require(!place_infantry_feet(asset,source.pose(),unreachable,knees,solved),"unreachable solve rejected without root displacement");
    mesh::PreviewMesh companion;require(mesh::decode_preview_mesh(read(assets/(std::string(id)+".hrm")),companion),"turn companion decodes");
    renderer::AnimationInstances turning,canonical,cold;
    require(turning.load(id,bytes,companion.vertices) && canonical.load(id,bytes,companion.vertices) && cold.load(id,bytes,companion.vertices),"turn caches load");
    renderer::AnimationIntent intent{.speed=1.5F,.enabled=true,.body_heading=0.F};
    std::vector<mesh::PreviewVertex> previous,previous_canonical;
    float maximum_step{},maximum_leg_step{},maximum_contact_step{},maximum_foot_lift{};
    bool previous_grounded=false;unsigned changed_foot_frames{},maximum_frame{},maximum_leg_frame{};std::size_t maximum_vertex{};
    for(unsigned frame=0;frame<300;++frame) {
      if(frame==20) {intent.speed=0;intent.preparing=true;intent.aim_pose_seconds=0.F;}
      if(frame>=20 && frame<140) {
        intent.aim_pose_seconds=static_cast<float>(frame-20)/60;
        intent.body_heading=-static_cast<float>(frame-20)*.14F;
      }
      if(frame==140) {intent.preparing=false;intent.aim_pose_seconds.reset();intent.speed=2;}
      if(frame==170)intent.speed=0;
      // Nonattack stationary pivot covers the ordinary A142 failure class.
      if(frame>=180 && frame<190)intent.body_heading=*intent.body_heading+.16F;
      if(frame==190) {intent.capturing=true;intent.product_capture_transport=true;intent.capture_progress=.03F;}
      if(frame>190 && frame<210)intent.capture_progress+=.01F;
      if(frame==210)intent.capturing=false;
      auto plain=intent;plain.body_heading.reset();
      turning.begin_frame();canonical.begin_frame();
      const auto* actor=turning.update(78,id,intent,1.F/60);const auto* ordinary=canonical.update(78,id,plain,1.F/60);
      if(!actor || !ordinary)std::cout<<"TURN_EVALUATION_FAILURE "<<id<<" frame="<<frame<<" foot="<<(actor!=nullptr)<<" canonical="<<(ordinary!=nullptr)<<std::endl;
      require(actor && ordinary,"walk/preparation/pivot/walk/capture/release remain evaluable");
      require(actor->weapon_delta==ordinary->weapon_delta && actor->head_delta==ordinary->head_delta && actor->launch_weapon_delta==ordinary->launch_weapon_delta,
          "transient feet never change live upper/head/weapon or actual launch frame");
      const bool grounded=!intent.capturing && !actor->clip.starts_with("beam_");
      if(intent.capturing)require(actor->clip=="beam_lift","capture preempts transient turn articulation immediately");
      if(frame>=193 && frame<210)for(std::size_t v=0;v<actor->vertices.size();++v)
        require(actor->vertices[v].position==ordinary->vertices[v].position && actor->vertices[v].normal==ordinary->vertices[v].normal,
            "after existing .06 capture fade every vertex matches canonical airborne pose exactly");
      float foot_difference{},capture_step{},canonical_capture_step{};
      for(std::size_t v=0;v<actor->vertices.size();++v) {
        if(!previous.empty()) {
          float distance{};for(std::size_t k=0;k<3;++k)distance+=std::pow(actor->vertices[v].position[k]-previous[v].position[k],2.F);
          if(std::sqrt(distance)>maximum_step){maximum_step=std::sqrt(distance);maximum_frame=frame;maximum_vertex=v;}
          const auto& joint=asset.nodes()[asset.joints()[asset.vertices()[v].joints[0]].node].name;
          if(joint=="thigh.L" || joint=="shin.L" || joint=="foot.L" || joint=="thigh.R" || joint=="shin.R" || joint=="foot.R") {
            if(std::sqrt(distance)>maximum_leg_step){maximum_leg_step=std::sqrt(distance);maximum_leg_frame=frame;}
            if(grounded && previous_grounded)maximum_contact_step=std::max(maximum_contact_step,std::sqrt(distance));
          }
          if(joint=="foot.R" && frame>=190 && frame<=193) {
            capture_step=std::max(capture_step,std::sqrt(distance));float canonical_distance{};
            for(std::size_t k=0;k<3;++k)canonical_distance+=std::pow(ordinary->vertices[v].position[k]-previous_canonical[v].position[k],2.F);
            canonical_capture_step=std::max(canonical_capture_step,std::sqrt(canonical_distance));
          }
        }
        for(std::size_t slot=0;slot<4;++slot)if(asset.vertices()[v].weights[slot]>.99F) {
          const auto& name=asset.nodes()[asset.joints()[asset.vertices()[v].joints[slot]].node].name;
          if(name=="foot.L" || name=="foot.R") {
            foot_difference=std::max(foot_difference,std::abs(actor->vertices[v].position[1]-ordinary->vertices[v].position[1]));
            if(frame>=21 && frame<140)maximum_foot_lift=std::max(maximum_foot_lift,foot_difference);
            if(frame>=21 && frame<140)require(actor->vertices[v].position[1]>=-.001F,"turning rigid boots remain above flat authored ground");
          }
        }
      }
      if(foot_difference>.01F)++changed_foot_frames;
      if(frame==80) {
        cold.begin_frame();const auto* restored=cold.update(78,id,intent,0);
        require(restored && restored->weapon_delta==actor->weapon_delta && restored->head_delta==actor->head_delta,"cold neutral feet retain exact live upper aim frame");cold.end_frame();
        const auto revision=actor->revision;const auto frozen=std::vector<mesh::PreviewVertex>(actor->vertices.begin(),actor->vertices.end());
        turning.end_frame();turning.begin_frame();actor=turning.update(78,id,intent,0);
        require(actor && actor->revision==revision,"paused turn does not advance transient history");
        for(std::size_t v=0;v<frozen.size();++v)require(actor->vertices[v].position==frozen[v].position,"paused visible turn feet freeze exactly");
      }
      if(frame>=190 && frame<=193)std::cout<<"CAPTURE_PAIRED "<<id<<" frame="<<frame<<" turned_foot_R="<<capture_step<<" canonical_foot_R="<<canonical_capture_step<<std::endl;
      previous_grounded=grounded;previous.assign(actor->vertices.begin(),actor->vertices.end());previous_canonical.assign(ordinary->vertices.begin(),ordinary->vertices.end());
      turning.end_frame();canonical.end_frame();
    }
    std::cout<<"TURN_FEET "<<id<<" max_vertex_step="<<maximum_step<<" max_frame="<<maximum_frame<<" vertex="<<maximum_vertex<<" joint="<<asset.nodes()[asset.joints()[asset.vertices()[maximum_vertex].joints[0]].node].name<<" max_leg_step="<<maximum_leg_step<<" max_leg_frame="<<maximum_leg_frame<<" grounded_contact_step="<<maximum_contact_step<<" foot_lift="<<maximum_foot_lift<<" changed_frames="<<changed_foot_frames<<'\n';
    require(maximum_foot_lift>.025F && changed_foot_frames>20,"actual stationary rotation releases boots using existing gait lift");
    require(maximum_contact_step<.25F,"adjacent grounded60Hz entry/movement/turn frames have bounded lower-body contact steps");
    turning.clear_actors();canonical.clear_actors();previous.clear();
    intent={.enabled=true,.body_heading=0.F};float settle_step{};
    for(unsigned frame=0;frame<90;++frame) {
      if(frame>0 && frame<=12)intent.body_heading=*intent.body_heading+.14F;
      auto plain=intent;plain.body_heading.reset();turning.begin_frame();canonical.begin_frame();
      const auto* actor=turning.update(91,id,intent,1.F/60);const auto* ordinary=canonical.update(91,id,plain,1.F/60);
      require(actor && ordinary,"nonattack pivot and stationary settle remain evaluable");
      require(actor->weapon_delta==ordinary->weapon_delta && actor->head_delta==ordinary->head_delta,"stationary settle preserves upper frame");
      if(!previous.empty())for(std::size_t v=0;v<actor->vertices.size();++v) {
        float step{};for(std::size_t k=0;k<3;++k)step+=std::pow(actor->vertices[v].position[k]-previous[v].position[k],2.F);
        settle_step=std::max(settle_step,std::sqrt(step));
      }
      if(frame==89)for(std::size_t v=0;v<actor->vertices.size();++v)require(actor->vertices[v].position==ordinary->vertices[v].position,"settled stationary feet fully return to canonical pose");
      previous.assign(actor->vertices.begin(),actor->vertices.end());turning.end_frame();canonical.end_frame();
    }
    std::cout<<"TURN_SETTLE "<<id<<" maximum_step="<<settle_step<<std::endl;
    require(settle_step<.25F,"nonattack pivot retirement has no contact pop");
  }
}

void product_capture_transport(const std::filesystem::path& assets) {
  namespace renderer=human_resources::renderer;
  namespace mesh=human_resources::assets;
  for(const char* id:{"cow","pig","sheep","farmer-body","resident-body",
      "infantry-rifleman-body","infantry-shotgunner-body","infantry-rocketeer-body"}) {
    const auto bytes=read(assets/(std::string(id)+".hra"));
    Asset asset; require(decode(bytes,asset).valid(),"capture source decodes");
    const auto root=asset.find_node("root");
    require(root<asset.nodes().size(),"product capture source has explicit root bone");
    require(asset.clips()[asset.find_clip("beam_release")].seconds==1.4F,"product return duration matches every authored release");
    Player sample;require(sample.bind(asset,"beam_lift") && sample.seek(.25F),"independent early source pose samples");
    Pose normalized;require(normalize_capture_root(asset,sample.pose(),normalized),"capture root normalization composes");
    for(std::size_t n=0;n<asset.nodes().size();++n) {
      require(normalized.local[n].rotation==sample.pose().local[n].rotation && normalized.local[n].scale==sample.pose().local[n].scale,
          "capture normalization preserves every source rotation and scale");
      require(normalized.local[n].translation==(n==root ? asset.nodes()[n].rest.translation : sample.pose().local[n].translation),
          "only the named root translation delta is removed");
    }
    const Vec3 delta{sample.pose().world[root][12]-normalized.world[root][12],
        sample.pose().world[root][13]-normalized.world[root][13],sample.pose().world[root][14]-normalized.world[root][14]};
    mesh::PreviewMesh companion;
    require(mesh::decode_preview_mesh(read(assets/(std::string(id)+".hrm")),companion),"capture companion decodes");
    renderer::AnimationInstances raw,transport;
    require(raw.load(id,bytes,companion.vertices) && transport.load(id,bytes,companion.vertices),"same exact source loads in both presentation modes");
    const float lift_seconds = asset.clips()[asset.find_clip("beam_lift")].seconds;
    const float struggle_seconds = asset.clips()[asset.find_clip("beam_struggle")].seconds;
    const auto progress_at = [=](float seconds) {
      return seconds < lift_seconds ? .65F * seconds / lift_seconds : .65F + .35F * std::min(.99F, (seconds - lift_seconds) / struggle_seconds);
    };
    renderer::AnimationIntent intent{.capturing = true, .enabled = true, .product_capture_transport = true, .capture_progress = progress_at(.25F)};
    raw.begin_frame();transport.begin_frame();
    auto raw_intent=intent;raw_intent.product_capture_transport=false;
    const auto* raw_actor=raw.update(1,id,raw_intent,.25F);
    const auto* actor=transport.update(1,id,intent,.25F);
    require(raw_actor && actor && std::abs(raw_actor->clip_time - .25F) < 1e-6F && std::abs(actor->clip_time - .25F) < 1e-6F,
        "source and progress-driven capture sample the identical authored quarter-second pose");
    for(std::size_t v=0;v<actor->vertices.size();++v) for(std::size_t k=0;k<3;++k)
      require(std::abs(raw_actor->vertices[v].position[k]-delta[k]-actor->vertices[v].position[k])<.00001F,
          "all body vertices share exactly the root translation correction");
    for(std::size_t k=0;k<16;++k) {
      const float shift=k>=12 && k<15 ? delta[k-12] : 0;
      if(actor->has_head) require(std::abs(raw_actor->head_delta[k]-shift-actor->head_delta[k])<.00001F,
          "head attachment consumes exactly the skin root correction");
      if(actor->has_weapon) require(std::abs(raw_actor->weapon_delta[k]-shift-actor->weapon_delta[k])<.00001F,
          "weapon attachment consumes exactly the skin root correction");
    }
    raw.end_frame();transport.end_frame();
    // Early and mature cancellation use real clip transitions at normal60Hz.
    for(unsigned held_frames:{3U,54U,119U}) {
      transport.clear_actors();intent.capturing=true;
      for(unsigned frame=0;frame<=held_frames;++frame) {
        intent.capture_progress = progress_at(static_cast<float>(frame + 1) / 60);
        transport.begin_frame();actor=transport.update(1,id,intent,1.F/60);require(actor,"capture progresses");transport.end_frame();
      }
      const float held_seconds = static_cast<float>(held_frames + 1) / 60;
      require(actor->clip == (held_seconds < lift_seconds ? "beam_lift" : "beam_struggle") &&
                  std::abs(actor->clip_time - (held_seconds < lift_seconds ? held_seconds : held_seconds - lift_seconds)) < .00001F,
          "early and mature cancellation exercise their intended authored lift or struggle phase");
      const std::vector<mesh::PreviewVertex> before(actor->vertices.begin(),actor->vertices.end());
      intent.capturing=false;
      transport.begin_frame();actor=transport.update(1,id,intent,1.F/60);
      require(actor && actor->clip=="beam_release","actual capture cancellation starts source release");
      float maximum_step{};
      for(std::size_t v=0;v<before.size();++v) {
        float distance{};for(std::size_t k=0;k<3;++k) distance+=std::pow(before[v].position[k]-actor->vertices[v].position[k],2.F);
        maximum_step=std::max(maximum_step,std::sqrt(distance));
      }
      require(maximum_step<.12F,"first60Hz early/mature cancellation blends relative joints without a suspended-root teleport");
      std::cout<<"CAPTURE_CANCEL "<<id<<" held_frames="<<held_frames<<" first60Hz_max_local_vertex_step_m="<<maximum_step<<'\n';
      transport.end_frame();
      for(unsigned frame=0;frame<90;++frame) {
        transport.begin_frame();actor=transport.update(1,id,intent,1.F/60);require(actor,"release progresses");transport.end_frame();
      }
      require(actor->minimum[1]>=-.04F && actor->minimum[1]<.1F,"release lands without source suspended height surviving");
    }
  }
}
} // namespace
int main(int argc, char** argv) {
  require(argc == 2 || argc == 3, "supply cooked infantry and optional independent source socket reference directory");
  if(argc==3) verify_sockets(argv[1], argv[2]);
  planted_aim_and_events(argv[1]);
  live_and_cold_preparation(argv[1]);
  physical_turn_feet(argv[1]);
  // Isolated three-pair parity directories intentionally omit the five other
  // product pairs. The shipping runtime exercises the complete transport set.
  if(std::filesystem::exists(std::filesystem::path(argv[1])/"cow.hra")) product_capture_transport(argv[1]);
  std::cout << "Military animation socket contracts passed.\n";
}
