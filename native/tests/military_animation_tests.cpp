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
  // Isolated three-pair parity directories intentionally omit the five other
  // product pairs. The shipping runtime exercises the complete transport set.
  if(std::filesystem::exists(std::filesystem::path(argv[1])/"cow.hra")) product_capture_transport(argv[1]);
  std::cout << "Military animation socket contracts passed.\n";
}
