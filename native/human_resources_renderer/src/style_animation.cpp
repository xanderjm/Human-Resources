#include "style_animation.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>
namespace human_resources::renderer {
namespace {
std::vector<std::byte> read(std::string_view path, std::size_t limit) {
  auto* file = SDL_IOFromFile(std::string(path).c_str(), "rb");
  if (!file)
    return {};
  const auto size = SDL_GetIOSize(file);
  if (size <= 0 || static_cast<Uint64>(size) > limit) {
    SDL_CloseIO(file);
    SDL_SetError("Animation input has invalid or excessive byte count: %s", std::string(path).c_str());
    return {};
  }
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  const bool ok = SDL_ReadIO(file, bytes.data(), bytes.size()) == bytes.size();
  SDL_CloseIO(file);
  if (!ok) {
    bytes.clear();
    SDL_SetError("Animation input was truncated or unreadable: %s", std::string(path).c_str());
  }
  return bytes;
}
void release_stream(SDL_GPUDevice* device, SDL_GPUBuffer* buffer, SDL_GPUTransferBuffer* transfer) {
  if (buffer)
    SDL_ReleaseGPUBuffer(device, buffer);
  if (transfer)
    SDL_ReleaseGPUTransferBuffer(device, transfer);
}
} // namespace
bool StyleAnimationStream::preload(std::span<const std::string_view> ids, std::string_view root) {
  if (ids.size() > AnimationInstances::asset_capacity) {
    SDL_SetError("Required animation preload exceeds %zu-asset limit", AnimationInstances::asset_capacity);
    return false;
  }
  for (const auto id : ids) {
    if (id.empty() || id.size() > 64 || !std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'; })) {
      SDL_SetError("Required animation preload has an invalid asset ID");
      return false;
    }
    if (!load(id, root))
      return false;
  }
  return true;
}
bool StyleAnimationStream::contains(std::string_view id) const {
  return instances_.contains(id);
}
bool StyleAnimationStream::load(std::string_view id, std::string_view root) {
  if (instances_.contains(id))
    return true;
  const auto base = std::filesystem::path(root) / std::string(id);
  const auto hra = read(base.string() + ".hra", animation::max_asset_bytes);
  if (hra.empty())
    return false;
  const auto hrm = read(base.string() + ".hrm", 80U * 1024U * 1024U);
  if (hrm.empty())
    return false;
  assets::PreviewMesh companion;
  if (!assets::decode_preview_mesh(hrm, companion) || !instances_.load(id, hra, companion.vertices)) {
    SDL_SetError("Invalid or mismatched animated HRA1/HRM1 asset: %s", std::string(id).c_str());
    return false;
  }
  return true;
}
bool StyleAnimationStream::stage(SDL_GPUDevice* device, SDL_GPUCommandBuffer* command, const presentation::StyleFrame& frame) {
  instances_.begin_frame();
  for (auto& launch: launches_) launch.seen=false;
  for (auto& [id, stream] : streams_) {
    (void)id;
    stream.seen = false;
  }
  for (const auto& instance : frame.instances) {
    if (!instance.animation.enabled)
      continue;
    const auto* actor = instances_.update(instance.stable_id, instance.mesh, instance.animation, frame.active_delta_seconds);
    if (!actor) {
      SDL_SetError("Invalid animated actor or capacity exceeded: %llu", static_cast<unsigned long long>(instance.stable_id));
      return false;
    }
    auto& stream = streams_[instance.stable_id];
    stream.seen = true;
    stream.actor = actor;
    stream.head = head_attachment_matrix(*actor);
    stream.weapon = weapon_attachment_matrix(*actor);
    const auto bytes = static_cast<Uint32>(actor->vertices.size() * sizeof(assets::PreviewVertex));
    if (stream.asset != instance.mesh) {
      stream.asset = instance.mesh;
      stream.revision = stream.pending_revision = 0;
    }
    if (stream.buffer && stream.byte_capacity != bytes) {
      release_stream(device, stream.buffer, stream.transfer);
      stream.buffer = nullptr;
      stream.transfer = nullptr;
    }
    if (!stream.buffer) {
      SDL_GPUBufferCreateInfo info{};
      info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
      info.size = bytes;
      stream.buffer = SDL_CreateGPUBuffer(device, &info);
      SDL_GPUTransferBufferCreateInfo upload{};
      upload.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
      upload.size = bytes;
      stream.transfer = SDL_CreateGPUTransferBuffer(device, &upload);
      if (!stream.buffer || !stream.transfer) {
        release_stream(device, stream.buffer, stream.transfer);
        stream.buffer = nullptr;
        stream.transfer = nullptr;
        return false;
      }
      stream.byte_capacity = bytes;
    }
    if (stream.revision == actor->revision)
      continue;
    auto* mapped = SDL_MapGPUTransferBuffer(device, stream.transfer, true);
    if (!mapped)
      return false;
    std::memcpy(mapped, actor->vertices.data(), bytes);
    SDL_UnmapGPUTransferBuffer(device, stream.transfer);
    auto* copy = SDL_BeginGPUCopyPass(command);
    if (!copy)
      return false;
    const SDL_GPUTransferBufferLocation from{stream.transfer, 0};
    const SDL_GPUBufferRegion to{stream.buffer, 0, bytes};
    SDL_UploadToGPUBuffer(copy, &from, &to, true);
    SDL_EndGPUCopyPass(copy);
    stream.pending_revision = actor->revision;
  }
  for (auto i = streams_.begin(); i != streams_.end();)
    if (!i->second.seen) {
      release_stream(device, i->second.buffer, i->second.transfer);
      i = streams_.erase(i);
    } else
      ++i;
  instances_.end_frame();
  for (const auto& part:frame.instances)
    if(part.launch_id!=0)
      for(auto& launch:launches_)
        if(launch.id==part.launch_id) launch.seen=true;
  for(auto& launch:launches_) if(!launch.seen) launch={};
  for (const auto& part:frame.instances) {
    if (part.launch_id==0) continue;
    auto found=std::find_if(launches_.begin(),launches_.end(),[&](const auto& launch){return launch.id==part.launch_id;});
    if (found==launches_.end()) {
      found=std::find_if(launches_.begin(),launches_.end(),[](const auto& launch){return launch.id==0;});
      if (found==launches_.end()) { SDL_SetError("Projectile launch cache capacity exceeded"); return false; }
      found->id=part.launch_id;
      found->position=part.launch_origin;
      const auto source=streams_.find(part.launch_parent_id);
      const auto parent=std::find_if(frame.instances.begin(),frame.instances.end(),[&](const auto& actor){return actor.stable_id==part.launch_parent_id && actor.animation.enabled;});
      if (source!=streams_.end() && parent!=frame.instances.end() && source->second.actor->has_weapon && source->second.actor->launch_sequence>0) {
        AnimatedActor launch_actor;
        launch_actor.weapon_delta=source->second.actor->launch_weapon_delta;
        const auto world=presentation::multiply(presentation::model_matrix(*parent),weapon_attachment_matrix(launch_actor));
        const auto& p=part.launch_local;
        found->position={world[0]*p.x+world[1]*p.y+world[2]*p.z+world[3],
            world[4]*p.x+world[5]*p.y+world[6]*p.z+world[7],world[8]*p.x+world[9]*p.y+world[10]*p.z+world[11]};
      }
    }
    found->seen=true;
  }
  for(auto& launch:launches_) if(!launch.seen) launch={};
  return true;
}
const StyleMesh* StyleAnimationStream::mesh(std::uint64_t id, const StyleMesh& base) {
  const auto found = streams_.find(id);
  if (found == streams_.end() || !found->second.seen)
    return nullptr;
  auto& stream = found->second;
  stream.mesh = base;
  stream.mesh.vertices = stream.buffer;
  const auto& a = *stream.actor;
  stream.mesh.minimum = {a.minimum[0], a.minimum[1], a.minimum[2]};
  stream.mesh.maximum = {a.maximum[0], a.maximum[1], a.maximum[2]};
  return &stream.mesh;
}
const presentation::Matrix* StyleAnimationStream::head_delta(std::uint64_t id) const {
  const auto found = streams_.find(id);
  if (found == streams_.end() || !found->second.seen || !found->second.actor->has_head)
    return nullptr;
  return &found->second.head;
}
const presentation::Matrix* StyleAnimationStream::weapon_delta(std::uint64_t id) const {
  const auto found = streams_.find(id);
  if (found == streams_.end() || !found->second.seen || !found->second.actor->has_weapon)
    return nullptr;
  return &found->second.weapon;
}
const presentation::Vec3* StyleAnimationStream::projectile_launch(std::uint64_t id) const {
  const auto found=std::find_if(launches_.begin(),launches_.end(),[&](const auto& launch){return launch.id==id && launch.seen;});
  return found==launches_.end() ? nullptr : &found->position;
}
void StyleAnimationStream::release(SDL_GPUDevice* device) {
  for (auto& [id, s] : streams_) {
    (void)id;
    release_stream(device, s.buffer, s.transfer);
  }
  streams_.clear();
  launches_={};
  instances_.clear_actors();
}
void StyleAnimationStream::commit() {
  for (auto& [id, s] : streams_) {
    (void)id;
    if (s.seen)
      s.revision = s.pending_revision;
  }
}
} // namespace human_resources::renderer
