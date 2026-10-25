#include <human_resources/renderer/gpu_renderer.h>

#include "asset_loader.h"
#include <shader_manifest.generated.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace human_resources::renderer {
namespace {

struct ShaderAsset final {
  std::string_view path;
  SDL_GPUShaderFormat format;
};

struct alignas(16) UiDrawUniforms final {
  float rect[4];
  float viewport[4];
  float color[4];
};
static_assert(sizeof(UiDrawUniforms) == 48U);

ShaderAsset shader_asset(const bool vertex) {
  if (generated::package_platform == "ios" || generated::package_platform == "macos") {
    const std::string_view platform = generated::package_platform;
    return {platform == "ios" ? (vertex ? "shaders/metal/ios/bootstrap.vertex.metallib"
                                        : "shaders/metal/ios/bootstrap.fragment.metallib")
                              : (vertex ? "shaders/metal/macos/bootstrap.vertex.metallib"
                                        : "shaders/metal/macos/bootstrap.fragment.metallib"),
        SDL_GPU_SHADERFORMAT_METALLIB};
  }
  if (generated::package_platform == "windows") {
    return {vertex ? "shaders/dxil/bootstrap.vertex.dxil" : "shaders/dxil/bootstrap.fragment.dxil",
        SDL_GPU_SHADERFORMAT_DXIL};
  }
  return {vertex ? "shaders/spirv/bootstrap.vertex.spv" : "shaders/spirv/bootstrap.fragment.spv",
      SDL_GPU_SHADERFORMAT_SPIRV};
}

void advertise_native_shader_format(const SDL_PropertiesID properties) {
  const auto asset = shader_asset(true);
  SDL_SetBooleanProperty(
      properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, asset.format == SDL_GPU_SHADERFORMAT_SPIRV);
  SDL_SetBooleanProperty(
      properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, asset.format == SDL_GPU_SHADERFORMAT_DXIL);
  SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_METALLIB_BOOLEAN,
      asset.format == SDL_GPU_SHADERFORMAT_METALLIB);
  if (generated::package_platform == "android") {
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN, false);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN, false);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN, false);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN, false);
  }
#ifndef NDEBUG
  SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, true);
#endif
}

UiDrawUniforms uniforms_for(const human_resources::ui::DrawCommand& command,
    const human_resources::ui::Size logical_size) noexcept {
  const human_resources::ui::Rect visible = human_resources::ui::intersect(command.rect, command.clip);
  return {
      .rect = {visible.x, visible.y, visible.width, visible.height},
      .viewport = {logical_size.width, logical_size.height, 0.0F, 0.0F},
      .color = {command.color.red, command.color.green, command.color.blue, command.color.alpha},
  };
}

} // namespace

GpuRenderer::GpuRenderer(human_resources::diagnostics::Logger& logger) noexcept : logger_(logger) {}

GpuRenderer::~GpuRenderer() {
  shutdown();
}

void GpuRenderer::log_error(const std::uint32_t code, const char* message) const noexcept {
  const std::array fields{
      human_resources::diagnostics::Field::text_value("sdl_error", SDL_GetError()),
  };
  (void)logger_.try_record({.severity = human_resources::diagnostics::Severity::error,
      .subsystem = human_resources::diagnostics::Subsystem::renderer,
      .code = code,
      .message = message,
      .fields = fields});
}

void GpuRenderer::log_shader_error(const std::uint32_t code, const char* message,
    const std::string_view path, const bool vertex) const noexcept {
  const std::array fields{
      human_resources::diagnostics::Field::text_value("shader", "bootstrap"),
      human_resources::diagnostics::Field::text_value("stage", vertex ? "vertex" : "fragment"),
      human_resources::diagnostics::Field::text_value("backend", backend_name()),
      human_resources::diagnostics::Field::text_value("path", path),
      human_resources::diagnostics::Field::text_value("sdl_error", SDL_GetError()),
  };
  (void)logger_.try_record({.severity = human_resources::diagnostics::Severity::error,
      .subsystem = human_resources::diagnostics::Subsystem::renderer,
      .code = code,
      .message = message,
      .fields = fields});
}

bool GpuRenderer::initialize(SDL_Window* window) {
  window_ = window;
  const SDL_PropertiesID properties = SDL_CreateProperties();
  advertise_native_shader_format(properties);
  device_ = SDL_CreateGPUDeviceWithProperties(properties);
  SDL_DestroyProperties(properties);
  if (device_ == nullptr) {
    log_error(2001U, "SDL_GPU device creation failed");
    return false;
  }
  if (!SDL_ClaimWindowForGPUDevice(device_, window_)) {
    log_error(2002U, "SDL_GPU window claim failed");
    return false;
  }
  window_claimed_ = true;

  SDL_GPUShader* vertex = load_shader(true);
  SDL_GPUShader* fragment = load_shader(false);
  if (vertex == nullptr || fragment == nullptr) {
    if (vertex != nullptr)
      SDL_ReleaseGPUShader(device_, vertex);
    if (fragment != nullptr)
      SDL_ReleaseGPUShader(device_, fragment);
    return false;
  }

  SDL_GPUColorTargetDescription color_target{};
  color_target.format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
  color_target.blend_state.enable_blend = true;
  color_target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
  color_target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  color_target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
  color_target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  color_target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  color_target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.vertex_shader = vertex;
  pipeline_info.fragment_shader = fragment;
  pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline_info.target_info.num_color_targets = 1;
  pipeline_info.target_info.color_target_descriptions = &color_target;
  pipeline_info.rasterizer_state.enable_depth_clip = true;
  pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
  SDL_ReleaseGPUShader(device_, vertex);
  SDL_ReleaseGPUShader(device_, fragment);
  if (pipeline_ == nullptr) {
    log_error(2003U, "SDL_GPU pipeline creation failed");
    return false;
  }
  return true;
}

SDL_GPUShader* GpuRenderer::load_shader(const bool vertex) const {
  const auto asset = shader_asset(vertex);
  const auto bytes = load_runtime_asset(asset.path, logger_);
  if (bytes.empty()) {
    log_shader_error(2010U, "SDL_GPU shader asset load failed", asset.path, vertex);
    return nullptr;
  }
  const auto reflection = vertex ? generated::bootstrap_vertex : generated::bootstrap_fragment;
  SDL_GPUShaderCreateInfo info{};
  info.code = reinterpret_cast<const std::uint8_t*>(bytes.data());
  info.code_size = bytes.size();
  info.entrypoint = vertex ? "vertex_main" : "fragment_main";
  info.format = asset.format;
  info.stage = vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
  info.num_samplers = reflection.samplers;
  info.num_storage_textures = reflection.storage_textures;
  info.num_storage_buffers = reflection.storage_buffers;
  info.num_uniform_buffers = reflection.uniform_buffers;
  SDL_GPUShader* shader = SDL_CreateGPUShader(device_, &info);
  if (shader == nullptr)
    log_shader_error(2011U, "SDL_GPU shader creation failed", asset.path, vertex);
  return shader;
}

bool GpuRenderer::render(const human_resources::ui::Size logical_size,
    const std::span<const human_resources::ui::DrawCommand> commands) {
  if (logical_size.width <= 0.0F || logical_size.height <= 0.0F)
    return true;
  SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device_);
  if (command == nullptr) {
    log_error(2005U, "SDL_GPU command-buffer acquisition failed");
    return false;
  }
  SDL_GPUTexture* swapchain = nullptr;
  Uint32 width = 0;
  Uint32 height = 0;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, window_, &swapchain, &width, &height)) {
    log_error(2006U, "SDL_GPU swapchain acquisition failed");
    SDL_CancelGPUCommandBuffer(command);
    return false;
  }
  (void)width;
  (void)height;
  if (swapchain == nullptr) {
    SDL_CancelGPUCommandBuffer(command);
    return true;
  }

  SDL_GPUColorTargetInfo color_target{};
  color_target.texture = swapchain;
  color_target.clear_color = {0.025F, 0.035F, 0.075F, 1.0F};
  color_target.load_op = SDL_GPU_LOADOP_CLEAR;
  color_target.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(command, &color_target, 1, nullptr);
  if (pass == nullptr) {
    log_error(2007U, "SDL_GPU render-pass creation failed");
    SDL_CancelGPUCommandBuffer(command);
    return false;
  }
  SDL_BindGPUGraphicsPipeline(pass, pipeline_);
  for (const human_resources::ui::DrawCommand& draw : commands) {
    const auto visible = human_resources::ui::intersect(draw.rect, draw.clip);
    if (visible.width <= 0.0F || visible.height <= 0.0F || draw.color.alpha <= 0.0F)
      continue;
    const UiDrawUniforms uniforms = uniforms_for(draw, logical_size);
    SDL_PushGPUVertexUniformData(command, 0, &uniforms, sizeof(uniforms));
    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
  }
  SDL_EndGPURenderPass(pass);
  if (!SDL_SubmitGPUCommandBuffer(command)) {
    log_error(2008U, "SDL_GPU command submission failed");
    return false;
  }
  return true;
}

std::string_view GpuRenderer::backend_name() const noexcept {
  if (device_ == nullptr)
    return "unavailable";
  const char* backend = SDL_GetGPUDeviceDriver(device_);
  return backend == nullptr ? std::string_view{"unknown"} : backend;
}

std::string_view GpuRenderer::device_name() const noexcept {
  if (device_ == nullptr)
    return "unavailable";
  return SDL_GetStringProperty(
      SDL_GetGPUDeviceProperties(device_), SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown");
}

std::string_view GpuRenderer::driver_version() const noexcept {
  if (device_ == nullptr)
    return "unavailable";
  return SDL_GetStringProperty(
      SDL_GetGPUDeviceProperties(device_), SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, "unknown");
}

void GpuRenderer::shutdown() {
  if (pipeline_ != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
    pipeline_ = nullptr;
  }
  if (device_ != nullptr && window_ != nullptr && window_claimed_) {
    SDL_ReleaseWindowFromGPUDevice(device_, window_);
  }
  window_claimed_ = false;
  if (device_ != nullptr) {
    SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
  }
  window_ = nullptr;
}

} // namespace human_resources::renderer
