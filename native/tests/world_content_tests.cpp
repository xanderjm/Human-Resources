#include "progression_test_backend.h"
#include "test_support.h"
#include <human_resources/product/accessibility.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <set>

namespace {
namespace hr = human_resources;
using test_support::require;
void locations_are_available_and_labelled() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store(backend);
  require(store.load().playable(), "new account is playable");
  hr::gameplay::Game game;
  game.start();
  hr::product::View view{.run = &game.snapshot(), .store = &store, .viewport = {0, 0, 390, 844}};
  require(view.theme == hr::presentation::SceneryTheme::farmland, "new session defaults to farmland");
  for (unsigned i = 0; i < 3; ++i) {
    const auto interface = hr::product::interface(view);
    const auto found = std::find_if(interface.buttons.begin(), interface.buttons.end(), [](const auto& b) { return b.command == hr::product::Command::location_next; });
    require(found != interface.buttons.end() && found->enabled, "all locations available without progression gate");
    require(found->label.find(hr::product::location_name(view.theme)) != std::string::npos, "selector displays selected location");
    const auto semantics = hr::product::accessibility_snapshot(view, interface);
    const auto spoken = std::find_if(semantics.begin(), semantics.end(), [](const auto& node) { return node.label == "Change location"; });
    require(spoken != semantics.end() && spoken->value.find(hr::product::location_name(view.theme)) != std::string::npos && spoken->value.find(hr::product::location_name(hr::product::next_location(view.theme))) != std::string::npos, "selector speaks current and next location");
    view.theme = hr::product::next_location(view.theme);
  }
  require(view.theme == hr::presentation::SceneryTheme::farmland, "three choices cycle back to farmland");
  for (auto screen : {hr::product::Screen::run, hr::product::Screen::pause, hr::product::Screen::results}) {
    view.screen = screen;
    const auto interface = hr::product::interface(view);
    require(std::none_of(interface.buttons.begin(), interface.buttons.end(), [](const auto& b) { return b.command == hr::product::Command::location_next; }), "location cannot change during a run or its results");
  }
}
void all_rendered_content_is_preloaded(const std::filesystem::path& root) {
  auto ids = hr::product::content_ids();
  const std::set<std::string_view> preloaded(ids.begin(), ids.end());
  require(ids.size() == preloaded.size(), "shared scenery assets are preloaded only once");
  for (auto id : ids)
    require(std::filesystem::is_regular_file(root / (std::string(id) + ".hrm")), "every consumed mesh is actually cooked");
  for (auto id : {"cow", "pig", "sheep", "farmer-body", "resident-body", "infantry-rifleman-body", "infantry-shotgunner-body", "infantry-rocketeer-body"})
    require(std::filesystem::is_regular_file(root / (std::string(id) + ".hra")), "all eight required animation companions ship");
  require(!std::filesystem::exists(root/"soldier-body.hra") && !std::filesystem::exists(root/"soldier-body.hrm"),"replaced legacy soldier pair does not ship alongside consumed infantry");
  hr::gameplay::Snapshot run;
  run.entities.push_back({.id = 17, .kind = hr::gameplay::EntityKind::farmer});
  for (unsigned i = 0; i < 3; ++i) {
    const auto theme = static_cast<hr::presentation::SceneryTheme>(i);
    for (float distance : {0.F, 63.5F, 64.F, 129.F, 100000.F}) {
      run.runner_distance = distance;
      run.player.z = static_cast<float>(distance);
      run.entities[0].position.z = static_cast<float>(distance);
      const auto frame = hr::product::scene(run, 390.F / 844.F, false, false, nullptr, theme);
      require(frame.location == i, "scene consumes chosen location");
      for (const auto& instance : frame.instances)
        require(preloaded.contains(instance.mesh), "all three streamed scenes use the package preload inventory");
      const auto body = std::find_if(frame.instances.begin(), frame.instances.end(), [](const auto& a) { return a.stable_id == 17; });
      require(body != frame.instances.end() && body->animation.enabled, "human target retains authored skeletal animation");
      require(body->mesh == (i == 1 ? "resident-body" : "farmer-body"), "suburbia uses resident and other themes use farmer");
      const auto hat = std::find_if(frame.instances.begin(), frame.instances.end(), [](const auto& a) { return a.attachment_parent_id == 17; });
      require(hat != frame.instances.end() && hat->mesh == (i == 1 ? "nightcap" : "straw-hat"), "correct headwear follows animated human socket");
    }
  }
  require(!std::filesystem::exists(root / "ground-wood.hrm"), "unused authoring ground texture is excluded");
}
} // namespace
int main(int argc, char** argv) {
  require(argc == 2, "cooked content path required");
  locations_are_available_and_labelled();
  all_rendered_content_is_preloaded(argv[1]);
  std::cout << "Three-location content and selection contracts passed\n";
}
