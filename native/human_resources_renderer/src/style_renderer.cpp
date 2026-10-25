#include <human_resources/renderer/preview_viewport.h>
#include <human_resources/renderer/style_renderer.h>

#include "style_animation.h"
#include "style_resources.h"
#include "style_surfaces.h"
#include <SDL3/SDL_gpu.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <human_resources/presentation/scenery.h>
#include <map>
#include <string>
#include <vector>

namespace human_resources::renderer {
namespace {
constexpr Uint32 shadow_size = 2048;
constexpr std::size_t batch_capacity = 64;
struct alignas(16) InstanceUniforms final {
  presentation::Matrix model;
  std::array<float, 4> tint, params, texture_style;
};
struct alignas(16) SceneUniforms final {
  presentation::Matrix camera, light;
  std::array<float, 4> animation;
};
static_assert(sizeof(InstanceUniforms) == 112);
static_assert(sizeof(SceneUniforms) == 144);
static_assert(sizeof(InstanceUniforms) * batch_capacity < 16384);
SDL_GPUTexture* texture(SDL_GPUDevice* device, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage,
    Uint32 w, Uint32 h, SDL_GPUSampleCount samples = SDL_GPU_SAMPLECOUNT_1) {
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = format;
  info.usage = usage;
  info.width = w;
  info.height = h;
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  info.sample_count = samples;
  return SDL_CreateGPUTexture(device, &info);
}
SDL_GPUGraphicsPipeline* pipeline(SDL_GPUDevice* device, bool shadow, bool effect, bool overlay, bool product_ui = false,
    SDL_GPUTextureFormat color_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM) {
  auto* vs = style_shader(device, overlay, true, false, product_ui);
  auto* fs = style_shader(device, overlay, false, shadow, product_ui);
  if (!vs || !fs) {
    if (vs)
      SDL_ReleaseGPUShader(device, vs);
    if (fs)
      SDL_ReleaseGPUShader(device, fs);
    return nullptr;
  }
  const SDL_GPUVertexBufferDescription buffer{0, 56, SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
  const std::array<SDL_GPUVertexAttribute, 5> attributes{{{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0}, {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 12},
      {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 24}, {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 32},
      {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 48}}};
  SDL_GPUGraphicsPipelineCreateInfo info{};
  info.vertex_shader = vs;
  info.fragment_shader = fs;
  info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  if (!overlay) {
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes.data();
    info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
  }
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  info.rasterizer_state.enable_depth_clip = true;
  SDL_GPUColorTargetDescription color{};
  color.format = color_format;
  if (effect || overlay) {
    auto& b = color.blend_state;
    b.enable_blend = true;
    b.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    b.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    b.color_blend_op = SDL_GPU_BLENDOP_ADD;
    b.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    b.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    b.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  }
  if (!shadow) {
    info.target_info.color_target_descriptions = &color;
    info.target_info.num_color_targets = 1;
    info.multisample_state.sample_count = product_ui ? SDL_GPU_SAMPLECOUNT_1 : SDL_GPU_SAMPLECOUNT_4;
  }
  if (!overlay) {
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = !effect;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
  }
  auto* result = SDL_CreateGPUGraphicsPipeline(device, &info);
  SDL_ReleaseGPUShader(device, vs);
  if (fs)
    SDL_ReleaseGPUShader(device, fs);
  return result;
}
} // namespace
struct StyleRenderer::State final {
  SDL_GPUDevice* device{};
  SDL_Window* window{};
  bool claimed{};
  SDL_GPUGraphicsPipeline* world{};
  SDL_GPUGraphicsPipeline* shadow_pipeline{};
  SDL_GPUGraphicsPipeline* effects{};
  SDL_GPUGraphicsPipeline* overlay{};
  SDL_GPUGraphicsPipeline* product_ui{};
  SDL_GPUGraphicsPipeline* capture_ui{};
  SDL_GPUTexture *color{}, *multisample{}, *depth{}, *shadow{};
  SDL_GPUTexture* capture_color{};
  SDL_GPUSampler *sampler{}, *shadow_sampler{};
  SDL_GPUTransferBuffer* readback{};
  std::map<std::string, StyleMesh, std::less<>> meshes;
  StyleSurfaceStream surface_stream;
  StyleAnimationStream animation_stream;
  std::string root;
  Uint32 width{}, height{};
  Uint32 present_width{}, present_height{};
  CaptureExtent capture_extent;
  std::array<SDL_GPUFence*, 2> frame_fences{};
  unsigned fence_index{};
  static void wait(SDL_GPUDevice* device, SDL_GPUFence* fence) {
    while (fence && !SDL_QueryGPUFence(device, fence))
      SDL_DelayNS(250000);
  }
  ~State() {
    if (!device)
      return;
    for (auto* fence : frame_fences)
      wait(device, fence);
    SDL_WaitForGPUIdle(device);
    for (auto* fence : frame_fences)
      if (fence)
        SDL_ReleaseGPUFence(device, fence);
    surface_stream.release(device);
    animation_stream.release(device);
    for (auto& [name, mesh] : meshes) {
      (void)name;
      release_style_mesh(device, mesh);
    }
    for (auto* p : {world, shadow_pipeline, effects, overlay, product_ui, capture_ui})
      if (p)
        SDL_ReleaseGPUGraphicsPipeline(device, p);
    for (auto* t : {color, multisample, depth, shadow, capture_color})
      if (t)
        SDL_ReleaseGPUTexture(device, t);
    if (sampler)
      SDL_ReleaseGPUSampler(device, sampler);
    if (shadow_sampler)
      SDL_ReleaseGPUSampler(device, shadow_sampler);
    if (readback)
      SDL_ReleaseGPUTransferBuffer(device, readback);
    if (claimed)
      SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
  }
  bool ensure_mesh(std::string_view name) {
    if (meshes.contains(name))
      return true;
    auto& mesh = meshes[std::string(name)];
    return upload_style_mesh(device, (std::filesystem::path(root) / (std::string(name) + ".hrm")).string(), mesh);
  }
  bool ensure_capture(CaptureExtent extent) {
    if (capture_color && extent.width == capture_extent.width && extent.height == capture_extent.height)
      return true;
    if (!capture_ui)
      capture_ui = pipeline(device, false, false, true, true);
    if (!capture_ui)
      return false;
    auto* target = texture(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET, extent.width, extent.height);
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = extent.width * extent.height * 4;
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &info);
    if (!target || !transfer) {
      if (target)
        SDL_ReleaseGPUTexture(device, target);
      if (transfer)
        SDL_ReleaseGPUTransferBuffer(device, transfer);
      return false;
    }
    if (capture_color)
      SDL_ReleaseGPUTexture(device, capture_color);
    if (readback)
      SDL_ReleaseGPUTransferBuffer(device, readback);
    capture_color = target;
    readback = transfer;
    capture_extent = extent;
    return true;
  }
  void blit_world(SDL_GPUCommandBuffer* command, SDL_GPUTexture* target, unsigned target_width, unsigned target_height) {
    SDL_GPUBlitInfo blit{};
    blit.source.texture = color;
    blit.source.w = width;
    blit.source.h = height;
    blit.destination.texture = target;
    const auto fitted = fit_preview_extent(width, height, target_width, target_height, true);
    blit.destination.w = fitted.width;
    blit.destination.h = fitted.height;
    blit.destination.x = (target_width - fitted.width) / 2;
    blit.destination.y = (target_height - fitted.height) / 2;
    blit.load_op = SDL_GPU_LOADOP_CLEAR;
    blit.clear_color = {0, 0, 0, 1};
    blit.filter = SDL_GPU_FILTER_LINEAR;
    SDL_BlitGPUTexture(command, &blit);
  }
  bool draw_screen_quads(SDL_GPUCommandBuffer* command, SDL_GPUTexture* target, SDL_GPUGraphicsPipeline* ui_pipeline, std::span<const presentation::ScreenQuad> quads) {
    if (quads.empty())
      return true;
    SDL_GPUColorTargetInfo ui_target{};
    ui_target.texture = target;
    ui_target.load_op = SDL_GPU_LOADOP_LOAD;
    ui_target.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command, &ui_target, 1, nullptr);
    if (!pass)
      return false;
    // Full target viewport: normalized layout and input keep the same physical
    // screen bounds, independent of the world's small rounding/letterbox bars.
    SDL_BindGPUGraphicsPipeline(pass, ui_pipeline);
    const SDL_GPUTextureSamplerBinding binding{meshes.find("ui-atlas")->second.texture, sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    std::array<presentation::ScreenQuad, 128> batch{};
    for (std::size_t offset = 0; offset < quads.size(); offset += batch.size()) {
      const auto count = std::min(batch.size(), quads.size() - offset);
      std::copy_n(quads.begin() + static_cast<std::ptrdiff_t>(offset), count, batch.begin());
      SDL_PushGPUVertexUniformData(command, 0, batch.data(), sizeof(batch));
      SDL_DrawGPUPrimitives(pass, 6, static_cast<Uint32>(count), 0, 0);
    }
    SDL_EndGPURenderPass(pass);
    return true;
  }
  struct Prepared final {
    const presentation::StyleInstance* instance;
    const StyleMesh* mesh;
    InstanceUniforms uniforms;
    bool visible, caster;
  };
  std::vector<Prepared> prepared;
  std::vector<const Prepared*> selection;
  StyleRenderStats stats;
  bool prepare(const presentation::StyleFrame& frame) {
    prepared.clear();
    stats = {};
    for (const auto& i : frame.instances) {
      const auto* mesh = &meshes.find(i.mesh)->second;
      if (i.animation.enabled) {
        mesh = animation_stream.mesh(i.stable_id, *mesh);
        if (!mesh)
          return false;
      }
      auto model = presentation::model_matrix(i);
      if (i.attachment_parent_id != 0) {
        const auto parent = std::find_if(frame.instances.begin(), frame.instances.end(), [&](const auto& p) { return p.stable_id == i.attachment_parent_id && p.animation.enabled; });
        const auto* socket = i.attachment_socket == presentation::AttachmentSocket::weapon ? animation_stream.weapon_delta(i.attachment_parent_id) : animation_stream.head_delta(i.attachment_parent_id);
        if (parent == frame.instances.end() || !socket) {
          SDL_SetError("Animated attachment parent/socket is absent");
          return false;
        }
        model = presentation::multiply(presentation::model_matrix(*parent), presentation::multiply(*socket, i.attachment_local));
      }
      if (i.launch_id != 0) {
        if(const auto* launch=animation_stream.projectile_launch(i.launch_id)) {
          const float weight=std::clamp(i.launch_bridge,0.F,1.F);
          model[3]+=(launch->x-i.launch_origin.x)*weight;
          model[7]+=(launch->y-i.launch_origin.y)*std::clamp(i.launch_elevation,0.F,1.F);
          model[11]+=(launch->z-i.launch_origin.z)*weight;
        }
      }
      const auto clip = presentation::multiply(frame.camera, model);
      const bool visible = presentation::bounds_visible(clip, mesh->minimum, mesh->maximum);
      // Retain offscreen occluders near the view: their shadows can enter it.
      const bool caster = i.casts_shadow && !i.effect && presentation::bounds_visible(clip, mesh->minimum, mesh->maximum, 1.6F);
      stats.visible_instances += visible ? 1U : 0U;
      stats.culled_instances += visible ? 0U : 1U;
      stats.shadow_instances += caster ? 1U : 0U;
      prepared.push_back({&i, mesh, {model, i.tint, {0, i.shield_rim ? 2.F : i.effect ? 1.F
                                                                                      : 0.F,
                                                        i.texture_repeat, i.texture_repeat * i.scale.z / i.scale.x},
                                        {mesh->average[0], mesh->average[1], mesh->average[2], i.texture_detail}},
          visible, caster});
    }
    return true;
  }
  void draw(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass, const presentation::StyleFrame& frame, bool shadow_pass, bool effects_pass) {
    selection.clear();
    for (const auto& p : prepared)
      if (shadow_pass ? p.caster : p.visible && (p.instance->effect || p.instance->tint[3] < 1) == effects_pass)
        selection.push_back(&p);
    // Opaque depth-tested geometry may be reordered; alpha effects keep order.
    if (!effects_pass)
      std::stable_sort(selection.begin(), selection.end(), [](auto* a, auto* b) { return a->instance->mesh < b->instance->mesh; });
    const SceneUniforms scene{shadow_pass ? frame.light : frame.camera, frame.light, {frame.ring_angle, 0, 0, 0}};
    SDL_PushGPUVertexUniformData(command, 0, &scene, sizeof(scene));
    std::array<InstanceUniforms, batch_capacity> batch{};
    for (std::size_t first = 0; first < selection.size();) {
      const auto* mesh = selection[first]->mesh;
      std::size_t end = first;
      while (end < selection.size() && end - first < batch_capacity && selection[end]->mesh == mesh) {
        batch[end - first] = selection[end]->uniforms;
        ++end;
      }
      SDL_PushGPUVertexUniformData(command, 1, batch.data(), sizeof(batch));
      const SDL_GPUBufferBinding vb{mesh->vertices, 0}, ib{mesh->indices, 0};
      SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
      SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
      if (!shadow_pass) {
        const std::array<SDL_GPUTextureSamplerBinding, 2> bindings{{{mesh->texture, sampler}, {shadow, shadow_sampler}}};
        SDL_BindGPUFragmentSamplers(pass, 0, bindings.data(), 2);
      }
      const auto count = static_cast<Uint32>(end - first);
      SDL_DrawGPUIndexedPrimitives(pass, mesh->count, count, 0, 0, 0);
      if (shadow_pass)
        ++stats.shadow_draws;
      else
        ++stats.world_draws;
      stats.triangles += static_cast<std::uint64_t>(mesh->count / 3) * count;
      first = end;
    }
  }
};
StyleRenderer::StyleRenderer() : state_(std::make_unique<State>()) {}
StyleRenderer::~StyleRenderer() = default;
const char* StyleRenderer::backend() const {
  return state_->device ? SDL_GetGPUDeviceDriver(state_->device) : "unavailable";
}
bool StyleRenderer::initialize(SDL_Window* window, std::string_view root, unsigned width, unsigned height, std::span<const std::string_view> preload, std::span<const std::string_view> animated_preload) {
  auto& s = *state_;
  if (s.device || !window || width == 0 || height == 0 || width > 2560 || height > 2560) {
    SDL_SetError("Style renderer needs one valid window and aligned dimensions up to 2560");
    return false;
  }
  s.window = window;
  s.root = root;
#ifdef NDEBUG
  constexpr bool validation = false;
#else
  constexpr bool validation = true;
#endif
  s.device = SDL_CreateGPUDevice(style_shader_format(), validation, nullptr);
  if (!s.device)
    return false;
  if (!SDL_ClaimWindowForGPUDevice(s.device, window))
    return false;
  s.claimed = true;
  // The application owns the preview's frame deadline. Avoid a second display
  // throttle where immediate presentation is supported; retain vsync elsewhere.
  const auto present_mode = SDL_WindowSupportsGPUPresentMode(s.device, window, SDL_GPU_PRESENTMODE_IMMEDIATE) ? SDL_GPU_PRESENTMODE_IMMEDIATE : SDL_GPU_PRESENTMODE_VSYNC;
  if (!SDL_SetGPUSwapchainParameters(s.device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present_mode))
    return false;
  if (!SDL_SetGPUAllowedFramesInFlight(s.device, 2))
    return false;
  s.prepared.reserve(1024);
  s.selection.reserve(1024);
  s.world = pipeline(s.device, false, false, false);
  s.shadow_pipeline = pipeline(s.device, true, false, false);
  s.effects = pipeline(s.device, false, true, false);
  s.overlay = pipeline(s.device, false, false, true);
  s.product_ui = pipeline(s.device, false, false, true, true, SDL_GetGPUSwapchainTextureFormat(s.device, window));
  if (!s.world || !s.shadow_pipeline || !s.effects || !s.overlay || !s.product_ui)
    return false;
  if (!resize(width, height))
    return false;
  s.shadow = texture(s.device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, shadow_size, shadow_size);
  SDL_GPUSamplerCreateInfo sampler_info{};
  sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
  sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
  sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  s.sampler = SDL_CreateGPUSampler(s.device, &sampler_info);
  sampler_info.enable_compare = true;
  sampler_info.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
  sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  s.shadow_sampler = SDL_CreateGPUSampler(s.device, &sampler_info);
  // The caller supplies the consumed theme, with no asset I/O during playback.
  if (!preload.empty()) {
    for (const auto name : preload)
      if (!s.ensure_mesh(name))
        return false;
  } else {
    for (float t : {0.F, 10.F, 20.F})
      for (const auto& i : presentation::style_frame(t, static_cast<float>(width) / static_cast<float>(height)).instances)
        if (!s.ensure_mesh(i.mesh))
          return false;
    for (const char* name : {"hud-0", "hud-1", "hud-2", "hud-land-0", "hud-land-1", "hud-land-2"})
      if (!s.ensure_mesh(name))
        return false;
  }
  for (const auto name : animated_preload)
    if (!s.meshes.contains(name)) {
      SDL_SetError("Required animation has no prepared scene model: %s", std::string(name).c_str());
      return false;
    }
  const auto animation_start = SDL_GetTicksNS();
  if (!s.animation_stream.preload(animated_preload, s.root))
    return false;
  if (!animated_preload.empty())
    SDL_Log("style animations prepared=%zu preload_ms=%.3f", animated_preload.size(), static_cast<double>(SDL_GetTicksNS() - animation_start) / 1.e6);
  if (!s.surface_stream.initialize(s.device))
    return false;
  SDL_Log("style assets loaded=%zu", s.meshes.size());
  return s.color && s.multisample && s.depth && s.shadow && s.sampler && s.shadow_sampler;
}
StyleRenderStats StyleRenderer::stats() const {
  return state_->stats;
}
void StyleRenderer::reset_actor_animations() {
  auto& s = *state_;
  if (!s.device)
    return;
  for (auto* fence : s.frame_fences)
    State::wait(s.device, fence);
  s.prepared.clear();
  s.selection.clear();
  s.animation_stream.release(s.device);
}
bool StyleRenderer::resize(unsigned width, unsigned height) {
  auto& s = *state_;
  if (!s.device || !width || !height || width > 2560 || height > 2560)
    return SDL_SetError("Invalid style render extent");
  if (width == s.width && height == s.height)
    return true;
  auto* color = texture(s.device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, width, height);
  auto* multisample = texture(s.device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET, width, height, SDL_GPU_SAMPLECOUNT_4);
  auto* depth = texture(s.device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET, width, height, SDL_GPU_SAMPLECOUNT_4);
  if (!color || !multisample || !depth) {
    for (auto* t : {color, multisample, depth})
      if (t)
        SDL_ReleaseGPUTexture(s.device, t);
    return false;
  }
  for (auto* t : {s.color, s.multisample, s.depth})
    if (t)
      SDL_ReleaseGPUTexture(s.device, t);
  s.color = color;
  s.multisample = multisample;
  s.depth = depth;
  s.width = width;
  s.height = height;
  return true;
}
bool StyleRenderer::wait_idle() {
  for (auto* fence : state_->frame_fences)
    State::wait(state_->device, fence);
  return SDL_WaitForGPUIdle(state_->device);
}
bool StyleRenderer::render(const presentation::StyleFrame& frame, SDL_IOStream* capture, bool present, CaptureExtent capture_pixels) {
  auto& s = *state_;
  if (!capture_pixels.empty() && (!capture || !capture_pixels.valid()))
    return SDL_SetError("Explicit capture requires a stream and at most 4096 pixels per side / 8388608 pixels total");
  if (!capture_pixels.empty()) {
    int window_width{}, window_height{};
    if (!SDL_GetWindowSize(s.window, &window_width, &window_height) || window_width <= 0 || window_height <= 0 ||
        !capture_pixels.matches_aspect(static_cast<unsigned>(window_width), static_cast<unsigned>(window_height)))
      return SDL_SetError("Explicit capture aspect must match the logical window; select matching --width/--height");
  }
  if (!frame.screen_quads.empty() && (!s.meshes.contains("ui-atlas") || frame.screen_quads.size() > 8192))
    return SDL_SetError("Missing UI atlas or UI quad budget exceeded");
  for (const auto& instance : frame.instances)
    if (!s.meshes.contains(instance.mesh)) {
      SDL_SetError("Unprepared scene model: %s", std::string(instance.mesh).c_str());
      return false;
    }
  for (const auto& instance : frame.instances)
    if (instance.animation.enabled && !s.animation_stream.contains(instance.mesh)) {
      SDL_SetError("Unprepared scene animation: %s", std::string(instance.mesh).c_str());
      return false;
    }
  for (const auto& surface : frame.surfaces)
    if (!s.meshes.contains(surface.material)) {
      SDL_SetError("Unprepared scene material: %s", std::string(surface.material).c_str());
      return false;
    }
  // Same bounded completion-fence lifetime as Midnight Menu. Keep the old
  // fence alive through the next submit's Metal command-buffer cleanup.
  State::wait(s.device, s.frame_fences[s.fence_index]);
  if (capture && !s.ensure_capture(capture_pixels.empty() ? CaptureExtent{s.width, s.height} : capture_pixels))
    return false;
  auto* command = SDL_AcquireGPUCommandBuffer(s.device);
  if (!command)
    return false;
  if (!s.surface_stream.stage(s.device, command, frame.surfaces)) {
    SDL_CancelGPUCommandBuffer(command);
    return false;
  }
  if (!s.animation_stream.stage(s.device, command, frame) || !s.prepare(frame)) {
    SDL_CancelGPUCommandBuffer(command);
    return false;
  }
  SDL_GPUDepthStencilTargetInfo shadow{};
  shadow.texture = s.shadow;
  shadow.clear_depth = 1;
  shadow.load_op = SDL_GPU_LOADOP_CLEAR;
  shadow.store_op = SDL_GPU_STOREOP_STORE;
  auto* pass = SDL_BeginGPURenderPass(command, nullptr, 0, &shadow);
  if (!pass) {
    SDL_CancelGPUCommandBuffer(command);
    return false;
  }
  SDL_BindGPUGraphicsPipeline(pass, s.shadow_pipeline);
  s.draw(command, pass, frame, true, false);
  SDL_EndGPURenderPass(pass);
  SDL_GPUColorTargetInfo color{};
  color.texture = s.multisample;
  color.resolve_texture = s.color;
  color.clear_color = {.5F, .55F, .4F, 1};
  color.load_op = SDL_GPU_LOADOP_CLEAR;
  color.store_op = SDL_GPU_STOREOP_RESOLVE;
  SDL_GPUDepthStencilTargetInfo depth{};
  depth.texture = s.depth;
  depth.clear_depth = 1;
  depth.load_op = SDL_GPU_LOADOP_CLEAR;
  depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
  pass = SDL_BeginGPURenderPass(command, &color, 1, &depth);
  if (!pass) {
    SDL_CancelGPUCommandBuffer(command);
    return false;
  }
  SDL_BindGPUGraphicsPipeline(pass, s.world);
  const SceneUniforms scene{frame.camera, frame.light, {frame.ring_angle, 0, 0, 0}};
  SDL_PushGPUVertexUniformData(command, 0, &scene, sizeof(scene));
  std::array<InstanceUniforms, batch_capacity> surface_batch{};
  for (const auto& range : s.surface_stream.ranges) {
    const auto& mesh = s.meshes.find(range.material)->second;
    surface_batch[0] = {presentation::model_matrix({}), range.tint, {0, 0, 1, 1}, {mesh.average[0], mesh.average[1], mesh.average[2], range.detail}};
    SDL_PushGPUVertexUniformData(command, 1, surface_batch.data(), sizeof(surface_batch));
    const SDL_GPUBufferBinding binding{s.surface_stream.buffer, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
    const std::array<SDL_GPUTextureSamplerBinding, 2> samplers{{{mesh.texture, s.sampler}, {s.shadow, s.shadow_sampler}}};
    SDL_BindGPUFragmentSamplers(pass, 0, samplers.data(), 2);
    SDL_DrawGPUPrimitives(pass, range.count, 1, range.first, 0);
    ++s.stats.world_draws;
    s.stats.triangles += range.count / 3;
  }
  s.draw(command, pass, frame, false, false);
  SDL_BindGPUGraphicsPipeline(pass, s.effects);
  s.draw(command, pass, frame, false, true);
  if (frame.show_overlay) {
    SDL_BindGPUGraphicsPipeline(pass, s.overlay);
    const auto& hud = s.meshes.find(std::string(s.width > s.height ? "hud-land-" : "hud-") + std::to_string(frame.location))->second;
    const SDL_GPUTextureSamplerBinding hud_binding{hud.texture, s.sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &hud_binding, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
  }
  SDL_EndGPURenderPass(pass);
  if (capture) {
    s.blit_world(command, s.capture_color, s.capture_extent.width, s.capture_extent.height);
    if (!s.draw_screen_quads(command, s.capture_color, s.capture_ui, frame.screen_quads)) {
      SDL_CancelGPUCommandBuffer(command);
      return false;
    }
    auto* copy = SDL_BeginGPUCopyPass(command);
    if (!copy) {
      SDL_CancelGPUCommandBuffer(command);
      return false;
    }
    SDL_GPUTextureRegion source{};
    source.texture = s.capture_color;
    source.w = s.capture_extent.width;
    source.h = s.capture_extent.height;
    source.d = 1;
    const SDL_GPUTextureTransferInfo destination{s.readback, 0, s.capture_extent.width, s.capture_extent.height};
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);
  }
  // Acquire after all fallible staging/capture work. SDL forbids cancellation
  // once a real swapchain texture has been acquired, even on a later error.
  SDL_GPUTexture* swapchain = nullptr;
  Uint32 sw = 0, sh = 0;
  if (present && !SDL_WaitAndAcquireGPUSwapchainTexture(command, s.window, &swapchain, &sw, &sh)) {
    SDL_CancelGPUCommandBuffer(command);
    return false;
  }
  if (swapchain) {
    if (sw != s.present_width || sh != s.present_height) {
      SDL_Log("style composition world=%ux%u ui=%ux%u ui_samples=1 format=%u", s.width, s.height, sw, sh,
          static_cast<unsigned>(SDL_GetGPUSwapchainTextureFormat(s.device, s.window)));
      s.present_width = sw;
      s.present_height = sh;
    }
    s.blit_world(command, swapchain, sw, sh);
    if (!s.draw_screen_quads(command, swapchain, s.product_ui, frame.screen_quads)) {
      const std::string error = SDL_GetError();
      (void)SDL_SubmitGPUCommandBuffer(command);
      s.animation_stream.commit();
      return SDL_SetError("Native UI pass failed: %s", error.c_str());
    }
  }
  auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
  if (!fence)
    return false;
  s.animation_stream.commit();
  if (auto* previous = s.frame_fences[s.fence_index])
    SDL_ReleaseGPUFence(s.device, previous);
  s.frame_fences[s.fence_index] = fence;
  s.fence_index = (s.fence_index + 1) % 2;
  if (capture) {
    State::wait(s.device, fence);
    void* pixels = SDL_MapGPUTransferBuffer(s.device, s.readback, false);
    if (!pixels)
      return false;
    const std::size_t size = static_cast<std::size_t>(s.capture_extent.width) * s.capture_extent.height * 4;
    const bool wrote = SDL_WriteIO(capture, pixels, size) == size;
    SDL_UnmapGPUTransferBuffer(s.device, s.readback);
    return wrote;
  }
  return true;
}
} // namespace human_resources::renderer
