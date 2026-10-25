#include "test_support.h"
#include "style_animation.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
using test_support::require;
namespace {
struct TemporaryPairs final {
  std::filesystem::path path = std::filesystem::temp_directory_path() / ("hr-animation-preload-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  TemporaryPairs() { std::filesystem::create_directory(path); }
  ~TemporaryPairs() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};
void copy_pair(const std::filesystem::path& source, const std::filesystem::path& destination, std::string_view id) {
  for (const char* extension : {".hra", ".hrm"})
    std::filesystem::copy_file(source / (std::string(id) + extension), destination / (std::string(id) + extension));
}
void corrupt(const std::filesystem::path& path) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << "BAD!";
  require(static_cast<bool>(file), "write malformed fixture");
}
} // namespace
int main(int argc, char** argv) {
  using human_resources::renderer::StyleAnimationStream;
  require(argc == 2, "cooked gameplay pair root is required");
  require(SDL_Init(0), "SDL IO initialization without a video/GPU device");
  constexpr std::array<std::string_view, 8> ids{"cow", "pig", "sheep", "farmer-body", "resident-body", "infantry-rifleman-body", "infantry-shotgunner-body", "infantry-rocketeer-body"};
  const std::filesystem::path source(argv[1]);
  TemporaryPairs fixtures;
  StyleAnimationStream preview;
  require(preview.preload({}, (fixtures.path / "absent-static-preview").string()), "static preview needs no HRA filesystem content");
  constexpr std::array<std::string_view, 9> excessive{"a", "b", "c", "d", "e", "f", "g", "h", "i"};
  require(!preview.preload(excessive, fixtures.path.string()), "animation preload count is bounded before reading");
  constexpr std::array<std::string_view, 1> unsafe{"../cow"};
  require(!preview.preload(unsafe, fixtures.path.string()), "invalid preload ID fails before reading");
  constexpr std::array<std::string_view, 1> missing{"missing"};
  require(!preview.preload(missing, fixtures.path.string()), "missing required HRA fails startup");
  require(std::string(SDL_GetError()).find("missing.hra") != std::string::npos, "missing HRA reports its package path");
  copy_pair(source, fixtures.path, "cow");
  constexpr std::array<std::string_view, 1> cow{"cow"};
  corrupt(fixtures.path / "cow.hra");
  require(!preview.preload(cow, fixtures.path.string()) && !preview.contains("cow"), "malformed required HRA is never accepted as static content");
  std::filesystem::copy_file(source / "cow.hra", fixtures.path / "cow.hra", std::filesystem::copy_options::overwrite_existing);
  corrupt(fixtures.path / "cow.hrm");
  require(!preview.preload(cow, fixtures.path.string()) && !preview.contains("cow"), "malformed paired HRM fails startup");
  std::filesystem::remove(fixtures.path / "cow.hrm");
  require(!preview.preload(cow, fixtures.path.string()), "missing companion HRM fails startup");
  require(std::string(SDL_GetError()).find("cow.hrm") != std::string::npos, "missing companion reports its package path");
  std::filesystem::copy_file(source / "pig.hrm", fixtures.path / "cow.hrm");
  require(!preview.preload(cow, fixtures.path.string()) && !preview.contains("cow"), "mismatched animation/mesh pair fails startup");
  std::filesystem::resize_file(fixtures.path / "cow.hra", human_resources::animation::max_asset_bytes + 1);
  require(!preview.preload(cow, fixtures.path.string()) && !preview.contains("cow"), "oversized HRA is rejected before allocation/read");
  require(std::string(SDL_GetError()).find("excessive byte count") != std::string::npos, "oversized input has a bounded-load diagnostic");
  std::filesystem::remove_all(fixtures.path);
  std::filesystem::create_directory(fixtures.path);
  for (const auto id : ids)
    copy_pair(source, fixtures.path, id);
  StyleAnimationStream game;
  require(game.preload(ids, fixtures.path.string()), "all eight consumed gameplay animation pairs prepare before a run");
  std::filesystem::remove_all(fixtures.path);
  for (const auto id : ids)
    require(game.contains(id), "frame lookup remains prepared after package files disappear");
  require(!game.contains("unknown-body"), "unrequested animation is not implicitly loaded");
  require(game.preload(ids, fixtures.path.string()), "already prepared pairs need no repeated file access or decode");
  StyleAnimationStream launch_cache;
  human_resources::presentation::StyleFrame frame;
  for(std::uint64_t id=1;id<=96;++id) {
    human_resources::presentation::StyleInstance part;
    part.launch_id=id;part.launch_origin={static_cast<float>(id),1,2};
    frame.instances.push_back(part);
  }
  require(launch_cache.stage(nullptr,nullptr,frame),"96 neutral projectile launch records need no GPU or source actor");
  for(auto& part:frame.instances) part.launch_id+=96;
  require(launch_cache.stage(nullptr,nullptr,frame),"full cache retires absent IDs before allocating96 replacement IDs");
  require(!launch_cache.projectile_launch(1) && launch_cache.projectile_launch(97),"expired launch identity cannot leak into replacement");
  const auto frozen=*launch_cache.projectile_launch(97);
  frame.instances[0].launch_origin={999,999,999};
  require(launch_cache.stage(nullptr,nullptr,frame) && launch_cache.projectile_launch(97)->x==frozen.x,"existing projectile retains original launch after source/context change");
  launch_cache.release(nullptr);
  require(!launch_cache.projectile_launch(97),"new/restored run clears projectile launch history with actor cache");
  SDL_Quit();
  std::cout << "animation startup preload contracts passed\n";
}
