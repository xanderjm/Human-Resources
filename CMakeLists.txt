cmake_minimum_required(VERSION 3.27)

project(HumanResources VERSION 0.1.0 LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJCXX)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectOptions)
include(Dependencies)

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)

set(GAME_GENERATED_ROOT "${CMAKE_BINARY_DIR}/generated")
set(GAME_GENERATED_PROTOCOL_DIR "${GAME_GENERATED_ROOT}/protocol")
set(GAME_GENERATED_SHADER_DIR "${GAME_GENERATED_ROOT}/assets/shaders")
set(GAME_GENERATED_SHADER_INCLUDE_DIR "${GAME_GENERATED_ROOT}/shaders/include")
set(GAME_GENERATED_RUNTIME_DIR "${GAME_GENERATED_ROOT}/assets/runtime")

add_library(human_resources_diagnostics STATIC
  native/human_resources_diagnostics/src/diagnostics.cpp
)
target_include_directories(human_resources_diagnostics PUBLIC
  native/human_resources_diagnostics/include
)
target_link_libraries(human_resources_diagnostics PRIVATE Threads::Threads)
game_apply_cpp_defaults(human_resources_diagnostics)

add_library(human_resources_input STATIC
  native/human_resources_input/src/input.cpp
)
target_include_directories(human_resources_input PUBLIC native/human_resources_input/include)
game_apply_cpp_defaults(human_resources_input)

add_library(human_resources_lifecycle STATIC
  native/human_resources_lifecycle/src/lifecycle.cpp
)
target_include_directories(human_resources_lifecycle PUBLIC native/human_resources_lifecycle/include)
game_apply_cpp_defaults(human_resources_lifecycle)

add_library(human_resources_storage STATIC
  native/human_resources_storage/src/storage.cpp
)
target_include_directories(human_resources_storage PUBLIC native/human_resources_storage/include)
game_apply_cpp_defaults(human_resources_storage)

add_library(human_resources_assets STATIC
  native/human_resources_assets/src/assets.cpp
  native/human_resources_assets/src/preview_mesh.cpp
)
target_include_directories(human_resources_assets PUBLIC native/human_resources_assets/include)
game_apply_cpp_defaults(human_resources_assets)

add_library(human_resources_presentation STATIC
  native/human_resources_presentation/src/style_scene.cpp
  native/human_resources_presentation/src/scenery.cpp
  native/human_resources_presentation/src/scenery_compositions.cpp
  native/human_resources_presentation/src/scenery_roads.cpp
  native/human_resources_presentation/src/scenery_farmland.cpp
  native/human_resources_presentation/src/scenery_suburbia.cpp
  native/human_resources_presentation/src/scenery_desert.cpp)
target_include_directories(human_resources_presentation PUBLIC native/human_resources_presentation/include)
game_apply_cpp_defaults(human_resources_presentation)

add_library(human_resources_ui STATIC
  native/human_resources_ui/src/components.cpp
  native/human_resources_ui/src/draw_list.cpp
  native/human_resources_ui/src/layout.cpp
  native/human_resources_ui/src/semantics.cpp
)
target_include_directories(human_resources_ui PUBLIC native/human_resources_ui/include)
game_apply_cpp_defaults(human_resources_ui)

add_library(human_resources_accessibility STATIC
  native/human_resources_accessibility/src/session.cpp
  native/human_resources_accessibility/src/serialization.cpp
)
target_include_directories(human_resources_accessibility PUBLIC native/human_resources_accessibility/include)
target_link_libraries(human_resources_accessibility PUBLIC human_resources_ui PRIVATE Threads::Threads)
game_apply_cpp_defaults(human_resources_accessibility)

add_library(human_resources_audio STATIC
  native/human_resources_audio/src/audio_service.cpp
  native/human_resources_audio/src/product_audio.cpp
  native/human_resources_audio/src/product_haptics.cpp
)
target_include_directories(human_resources_audio PUBLIC native/human_resources_audio/include)
game_apply_cpp_defaults(human_resources_audio)

if(HUMAN_RESOURCES_FEATURE_DYNAMIC_TEXT)
  add_library(human_resources_text STATIC native/human_resources_text/src/sdl_ttf_text.cpp)
  target_include_directories(human_resources_text PUBLIC native/human_resources_text/include)
  target_link_libraries(human_resources_text PUBLIC human_resources_ui SDL3_ttf::SDL3_ttf SDL3::SDL3)
  game_apply_cpp_defaults(human_resources_text)
endif()

if(HUMAN_RESOURCES_FEATURE_DETERMINISTIC_AUTHORITY)
  if(NOT EXISTS "${GAME_FLATC_EXECUTABLE}")
    message(FATAL_ERROR "Host flatc is missing. Run tools/project.py bootstrap: ${GAME_FLATC_EXECUTABLE}")
  endif()
  set(GAME_PROTOCOL_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/native/human_resources_protocol/schemas/game.fbs")
  set(GAME_PROTOCOL_GENERATED_HEADER "${GAME_GENERATED_PROTOCOL_DIR}/game_generated.h")
  add_custom_command(
    OUTPUT "${GAME_PROTOCOL_GENERATED_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GAME_GENERATED_PROTOCOL_DIR}"
    COMMAND "${GAME_FLATC_EXECUTABLE}" --cpp --scoped-enums --gen-compare --gen-object-api
            -o "${GAME_GENERATED_PROTOCOL_DIR}" "${GAME_PROTOCOL_SCHEMA}"
    DEPENDS "${GAME_PROTOCOL_SCHEMA}"
    VERBATIM
  )
  add_custom_target(human_resources_generate_protocol DEPENDS "${GAME_PROTOCOL_GENERATED_HEADER}")

  add_library(human_resources_protocol STATIC
    native/human_resources_protocol/src/protocol.cpp
    "${GAME_PROTOCOL_GENERATED_HEADER}"
  )
  add_dependencies(human_resources_protocol human_resources_generate_protocol)
  target_include_directories(human_resources_protocol PUBLIC
    native/human_resources_protocol/include
    "${GAME_GENERATED_PROTOCOL_DIR}"
  )
  target_link_libraries(human_resources_protocol PUBLIC FlatBuffers::FlatBuffers)
  game_apply_cpp_defaults(human_resources_protocol)

  add_library(human_resources_core STATIC
    native/human_resources_core/src/sim_rng.cpp
    native/human_resources_core/src/simulation.cpp
    native/human_resources_core/src/state_hash.cpp
  )
  target_include_directories(human_resources_core PUBLIC native/human_resources_core/include)
  target_link_libraries(human_resources_core PUBLIC human_resources_protocol)
  game_apply_cpp_defaults(human_resources_core)

  add_library(human_resources_runtime STATIC
    native/human_resources_runtime/src/local_authority.cpp
    native/human_resources_runtime/src/replay.cpp
  )
  target_include_directories(human_resources_runtime PUBLIC native/human_resources_runtime/include)
  target_link_libraries(human_resources_runtime PUBLIC human_resources_core)
  game_apply_cpp_defaults(human_resources_runtime)

  add_library(human_resources_server STATIC native/human_resources_server/src/authority_service.cpp)
  target_include_directories(human_resources_server PUBLIC native/human_resources_server/include)
  target_link_libraries(human_resources_server PUBLIC human_resources_runtime)
  game_apply_cpp_defaults(human_resources_server)
endif()

add_subdirectory(native/human_resources_animation)
add_library(human_resources_animation_instances STATIC native/human_resources_renderer/src/animation_instances.cpp)
target_include_directories(human_resources_animation_instances PUBLIC native/human_resources_renderer/include)
target_link_libraries(human_resources_animation_instances PUBLIC human_resources_animation human_resources_assets human_resources_presentation)
game_apply_cpp_defaults(human_resources_animation_instances)

add_subdirectory(native/human_resources_gameplay)
add_subdirectory(native/human_resources_progression)

if(HUMAN_RESOURCES_BUILD_TESTS)
  enable_testing()
  foreach(product_test gameplay gameplay_effect progression progression_save account_session product_flow audio_feedback)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/native/tests/${product_test}_tests.cpp")
      add_executable(human_resources_${product_test}_tests "native/tests/${product_test}_tests.cpp")
      target_link_libraries(human_resources_${product_test}_tests PRIVATE human_resources_progression)
      game_apply_cpp_defaults(human_resources_${product_test}_tests)
      add_test(NAME ${product_test}_contracts COMMAND human_resources_${product_test}_tests)
    endif()
  endforeach()
  add_test(NAME gameplay_tuning_contracts COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/native/tests/gameplay_tuning_tests.py")
  target_link_libraries(human_resources_audio_feedback_tests PRIVATE human_resources_audio)
  add_executable(human_resources_animation_instances_tests native/tests/animation_instances_tests.cpp)
  target_link_libraries(human_resources_animation_instances_tests PRIVATE human_resources_animation_instances)
  game_apply_cpp_defaults(human_resources_animation_instances_tests)
  add_test(NAME animation_instances_contracts COMMAND human_resources_animation_instances_tests)
  add_executable(human_resources_style_scene_tests native/tests/style_scene_tests.cpp)
  target_include_directories(human_resources_style_scene_tests PRIVATE native/human_resources_renderer/include)
  target_link_libraries(human_resources_style_scene_tests PRIVATE human_resources_presentation human_resources_assets)
  game_apply_cpp_defaults(human_resources_style_scene_tests)
  add_test(NAME style_scene_contracts COMMAND human_resources_style_scene_tests)
  add_executable(human_resources_scenery_tests native/tests/scenery_tests.cpp)
  target_link_libraries(human_resources_scenery_tests PRIVATE human_resources_presentation)
  game_apply_cpp_defaults(human_resources_scenery_tests)
  add_test(NAME scenery_generation_contracts COMMAND human_resources_scenery_tests)
  add_test(NAME tooling_contracts
    COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tools/tests" -p "test_*.py")

  if(HUMAN_RESOURCES_FEATURE_DETERMINISTIC_AUTHORITY)
    add_executable(human_resources_deterministic_tests native/tests/deterministic_tests.cpp)
    target_link_libraries(human_resources_deterministic_tests PRIVATE human_resources_server)
    game_apply_cpp_defaults(human_resources_deterministic_tests)
    add_test(NAME deterministic_authority_contracts COMMAND human_resources_deterministic_tests)
  endif()

  add_executable(human_resources_ui_tests native/tests/ui_tests.cpp)
  target_link_libraries(human_resources_ui_tests PRIVATE human_resources_ui)
  game_apply_cpp_defaults(human_resources_ui_tests)
  add_test(NAME ui_contracts COMMAND human_resources_ui_tests)

  add_executable(human_resources_accessibility_tests native/tests/accessibility_tests.cpp)
  target_link_libraries(human_resources_accessibility_tests PRIVATE human_resources_accessibility)
  game_apply_cpp_defaults(human_resources_accessibility_tests)
  add_test(NAME accessibility_contracts COMMAND human_resources_accessibility_tests)

  add_executable(human_resources_product_haptics_tests native/tests/product_haptics_tests.cpp)
  target_link_libraries(human_resources_product_haptics_tests PRIVATE human_resources_audio)
  game_apply_cpp_defaults(human_resources_product_haptics_tests)
  add_test(NAME product_haptics_contracts COMMAND human_resources_product_haptics_tests)

  add_executable(human_resources_diagnostics_tests native/tests/diagnostics_tests.cpp)
  target_link_libraries(human_resources_diagnostics_tests PRIVATE human_resources_diagnostics)
  game_apply_cpp_defaults(human_resources_diagnostics_tests)
  add_test(NAME diagnostics_contracts COMMAND human_resources_diagnostics_tests)

  add_executable(human_resources_input_tests native/tests/input_tests.cpp)
  target_link_libraries(human_resources_input_tests PRIVATE human_resources_input)
  game_apply_cpp_defaults(human_resources_input_tests)
  add_test(NAME input_contracts COMMAND human_resources_input_tests)

  add_executable(human_resources_lifecycle_tests native/tests/lifecycle_tests.cpp)
  target_link_libraries(human_resources_lifecycle_tests PRIVATE human_resources_lifecycle)
  game_apply_cpp_defaults(human_resources_lifecycle_tests)
  add_test(NAME lifecycle_contracts COMMAND human_resources_lifecycle_tests)

  add_executable(human_resources_storage_tests native/tests/storage_tests.cpp)
  target_link_libraries(human_resources_storage_tests PRIVATE human_resources_storage)
  game_apply_cpp_defaults(human_resources_storage_tests)
  add_test(NAME storage_contracts COMMAND human_resources_storage_tests)

  add_executable(human_resources_assets_tests native/tests/assets_tests.cpp)
  target_link_libraries(human_resources_assets_tests PRIVATE human_resources_assets)
  game_apply_cpp_defaults(human_resources_assets_tests)
  add_test(NAME asset_contracts COMMAND human_resources_assets_tests)

  add_executable(human_resources_presentation_boundary_tests
    native/tests/presentation_boundary_tests.cpp
  )
  target_link_libraries(human_resources_presentation_boundary_tests PRIVATE
    human_resources_audio human_resources_diagnostics human_resources_ui
  )
  game_apply_cpp_defaults(human_resources_presentation_boundary_tests)
  add_test(NAME presentation_boundary_contracts COMMAND human_resources_presentation_boundary_tests)

  if(HUMAN_RESOURCES_FEATURE_DYNAMIC_TEXT)
    add_executable(human_resources_text_tests native/tests/text_tests.cpp)
    target_link_libraries(human_resources_text_tests PRIVATE human_resources_text)
    game_apply_cpp_defaults(human_resources_text_tests)
    add_test(NAME dynamic_text_contracts COMMAND human_resources_text_tests)
  endif()
endif()

if(HUMAN_RESOURCES_BUILD_CLIENT)
  include(Shaders)
  game_add_shaders(human_resources_shaders game_shader_package_files)

  file(GLOB_RECURSE game_runtime_asset_sources CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime/*")
  list(FILTER game_runtime_asset_sources EXCLUDE REGEX "(^|/)[.]gitkeep$")
  file(GLOB_RECURSE game_product_authoring_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/*.glb"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/graphics/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/graphics/fx-shield-shell/unit-sphere.glb"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/animation/*.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/design/balance/endless-upgrade-system.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/animation/*.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/*character_animation.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_shared_walks.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/human-resources-v3/ttf/*.ttf")
  set(game_runtime_package_files "${GAME_GENERATED_RUNTIME_DIR}/package-manifest.json")
  set(game_runtime_source_files)
  foreach(runtime_asset_relative IN LISTS game_runtime_asset_sources)
    list(APPEND game_runtime_package_files "${GAME_GENERATED_RUNTIME_DIR}/${runtime_asset_relative}")
    list(APPEND game_runtime_source_files
      "${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime/${runtime_asset_relative}")
  endforeach()
  # A removed source must invalidate cooking, even if every remaining source is older.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime/game-content.json")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime/game-content.json" product_content_recipe)
    string(JSON product_model_count LENGTH "${product_content_recipe}" models)
    math(EXPR product_model_last "${product_model_count} - 1")
    foreach(model_index RANGE 0 ${product_model_last})
      string(JSON model_name GET "${product_content_recipe}" models ${model_index})
      list(APPEND game_runtime_package_files "${GAME_GENERATED_RUNTIME_DIR}/game/${model_name}.hrm")
    endforeach()
    foreach(product_primitive plane cube ring beam ground-grass ground-dirt ground-soil ground-asphalt ground-pavement ground-sand ui-atlas)
      list(APPEND game_runtime_package_files "${GAME_GENERATED_RUNTIME_DIR}/game/${product_primitive}.hrm")
    endforeach()
    string(JSON product_animation_count LENGTH "${product_content_recipe}" animated_models)
    math(EXPR product_animation_last "${product_animation_count} - 1")
    foreach(animation_index RANGE 0 ${product_animation_last})
      string(JSON animated_body GET "${product_content_recipe}" animated_models ${animation_index})
      list(APPEND game_runtime_package_files "${GAME_GENERATED_RUNTIME_DIR}/game/${animated_body}.hra")
    endforeach()
    list(APPEND game_runtime_package_files "${GAME_GENERATED_RUNTIME_DIR}/game/animation-provenance.json")
    list(APPEND game_runtime_package_files "${GAME_GENERATED_RUNTIME_DIR}/game/FONT-LICENSE.txt"
      "${GAME_GENERATED_RUNTIME_DIR}/game/content-provenance.json")
  endif()
  file(GENERATE OUTPUT "${GAME_GENERATED_ROOT}/runtime-sources.txt"
    CONTENT "$<JOIN:${game_runtime_source_files},\n>\n")
  set_source_files_properties(${game_runtime_package_files} PROPERTIES
    GENERATED TRUE HEADER_FILE_ONLY TRUE)
  set(game_runtime_byproducts ${game_runtime_package_files})
  list(REMOVE_ITEM game_runtime_byproducts "${GAME_GENERATED_RUNTIME_DIR}/package-manifest.json")
  add_custom_command(
    OUTPUT "${GAME_GENERATED_RUNTIME_DIR}/package-manifest.json"
    BYPRODUCTS ${game_runtime_byproducts} "${GAME_GENERATED_ROOT}/product/include/product_glyphs.generated.h"
      "${GAME_GENERATED_ROOT}/product/include/product_mesh_bounds.generated.h"
      "${GAME_GENERATED_ROOT}/product/include/infantry_equipment.generated.h"
      "${GAME_GENERATED_ROOT}/product/include/vehicle_assembly.generated.h"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generators/cook_runtime_assets.py"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime"
            --output-root "${GAME_GENERATED_RUNTIME_DIR}"
            --project "${CMAKE_CURRENT_SOURCE_DIR}/project.json"
    DEPENDS ${game_runtime_source_files} ${game_product_authoring_sources}
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/cook_product_content.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/cook_vehicle_content.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/glb_geometry_repair.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/audit_glb.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/art_tools.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/art-toolchain.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/art-requirements.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/cook_infantry_animation.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_infantry_animation.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_infantry_animation.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/cook_style_preview.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/normalize_glb.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/production/inventory.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/production/vehicle-weapons.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/production/assemblies.json"
            "${GAME_GENERATED_ROOT}/runtime-sources.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/project.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generators/cook_runtime_assets.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generators/source_inventory.py"
    VERBATIM
  )
  add_custom_target(human_resources_runtime_assets
    DEPENDS "${GAME_GENERATED_RUNTIME_DIR}/package-manifest.json")

  add_library(human_resources_renderer STATIC
    native/human_resources_renderer/src/asset_loader.cpp
    native/human_resources_renderer/src/asset_loader.h
    native/human_resources_renderer/src/gpu_renderer.cpp
  )
  add_dependencies(human_resources_renderer human_resources_shaders)
  target_include_directories(human_resources_renderer
    PUBLIC native/human_resources_renderer/include
    PRIVATE native/human_resources_renderer/src "${GAME_GENERATED_SHADER_INCLUDE_DIR}"
  )
  target_link_libraries(human_resources_renderer PUBLIC
    human_resources_diagnostics human_resources_ui SDL3::SDL3
  )
  game_apply_cpp_defaults(human_resources_renderer)

  add_library(human_resources_platform STATIC
    native/human_resources_platform/src/sdl_platform.cpp
    native/human_resources_platform/src/window_metrics.cpp
    native/human_resources_platform/src/product_audio.cpp
    native/human_resources_platform/src/product_haptics.cpp
  )
  target_include_directories(human_resources_platform PUBLIC native/human_resources_platform/include)
  target_link_libraries(human_resources_platform PUBLIC
    human_resources_assets human_resources_diagnostics human_resources_input
    human_resources_lifecycle human_resources_storage human_resources_ui human_resources_accessibility human_resources_audio SDL3::SDL3
  )
  if(IOS)
    enable_language(OBJCXX)
    target_sources(human_resources_platform PRIVATE native/human_resources_platform/src/ios_accessibility.mm native/human_resources_platform/src/ios_haptics.mm)
    set_source_files_properties(native/human_resources_platform/src/ios_accessibility.mm native/human_resources_platform/src/ios_haptics.mm
      PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    set_target_properties(human_resources_platform PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
    target_link_libraries(human_resources_platform PRIVATE "-framework UIKit" "-framework CoreHaptics")
  elseif(ANDROID)
    target_sources(human_resources_platform PRIVATE native/human_resources_platform/src/android_accessibility.cpp native/human_resources_platform/src/android_haptics.cpp)
  elseif(APPLE)
    enable_language(OBJCXX)
    target_sources(human_resources_platform PRIVATE native/human_resources_platform/src/macos_accessibility.mm native/human_resources_platform/src/unavailable_haptics.cpp)
    set_source_files_properties(native/human_resources_platform/src/macos_accessibility.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    set_target_properties(human_resources_platform PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
    target_link_libraries(human_resources_platform PRIVATE "-framework AppKit")
  else()
    target_sources(human_resources_platform PRIVATE native/human_resources_platform/src/unavailable_accessibility.cpp native/human_resources_platform/src/unavailable_haptics.cpp)
  endif()
  game_apply_cpp_defaults(human_resources_platform)

  if(HUMAN_RESOURCES_BUILD_TESTS)
    add_executable(human_resources_platform_adapter_tests native/tests/platform_adapter_tests.cpp)
    target_link_libraries(human_resources_platform_adapter_tests PRIVATE human_resources_platform)
    game_apply_cpp_defaults(human_resources_platform_adapter_tests)
    add_test(NAME platform_adapter_contracts COMMAND human_resources_platform_adapter_tests)
  endif()

  add_subdirectory(native/human_resources_product)
  add_dependencies(human_resources_product human_resources_runtime_assets)
  if(HUMAN_RESOURCES_BUILD_TESTS)
    add_executable(human_resources_product_controls_tests native/tests/product_controls_tests.cpp)
    target_link_libraries(human_resources_product_controls_tests PRIVATE human_resources_product)
    game_apply_cpp_defaults(human_resources_product_controls_tests)
    add_test(NAME product_controls_contracts COMMAND human_resources_product_controls_tests)
    add_executable(human_resources_product_projection_tests native/tests/product_projection_tests.cpp)
    target_link_libraries(human_resources_product_projection_tests PRIVATE human_resources_product)
    game_apply_cpp_defaults(human_resources_product_projection_tests)
    add_test(NAME product_projection_contracts COMMAND human_resources_product_projection_tests)
    add_executable(human_resources_product_accessibility_tests native/tests/product_accessibility_tests.cpp)
    target_link_libraries(human_resources_product_accessibility_tests PRIVATE human_resources_product)
    game_apply_cpp_defaults(human_resources_product_accessibility_tests)
    add_test(NAME product_accessibility_contracts COMMAND human_resources_product_accessibility_tests)
    add_executable(human_resources_world_content_tests native/tests/world_content_tests.cpp)
    target_link_libraries(human_resources_world_content_tests PRIVATE human_resources_product)
    game_apply_cpp_defaults(human_resources_world_content_tests)
    add_test(NAME world_content_contracts COMMAND human_resources_world_content_tests "${GAME_GENERATED_RUNTIME_DIR}/game")
  endif()
  add_library(human_resources_game_renderer STATIC
    native/human_resources_renderer/src/style_renderer.cpp
    native/human_resources_renderer/src/style_resources.cpp
    native/human_resources_renderer/src/style_surfaces.cpp
    native/human_resources_renderer/src/style_animation.cpp)
  target_include_directories(human_resources_game_renderer PUBLIC native/human_resources_renderer/include
    PRIVATE "${GAME_GENERATED_SHADER_INCLUDE_DIR}")
  target_link_libraries(human_resources_game_renderer PUBLIC human_resources_presentation human_resources_assets human_resources_animation_instances SDL3::SDL3)
  add_dependencies(human_resources_game_renderer human_resources_shaders)
  game_apply_cpp_defaults(human_resources_game_renderer)
  if(HUMAN_RESOURCES_BUILD_TESTS)
    add_executable(human_resources_style_animation_preload_tests native/tests/style_animation_preload_tests.cpp)
    target_include_directories(human_resources_style_animation_preload_tests PRIVATE native/human_resources_renderer/src)
    target_link_libraries(human_resources_style_animation_preload_tests PRIVATE human_resources_game_renderer)
    add_dependencies(human_resources_style_animation_preload_tests human_resources_runtime_assets)
    game_apply_cpp_defaults(human_resources_style_animation_preload_tests)
    add_test(NAME style_animation_preload_contracts COMMAND human_resources_style_animation_preload_tests "${CMAKE_CURRENT_BINARY_DIR}/assets/runtime/game")
    add_executable(human_resources_military_animation_tests native/tests/military_animation_tests.cpp)
    target_link_libraries(human_resources_military_animation_tests PRIVATE human_resources_animation_instances)
    add_dependencies(human_resources_military_animation_tests human_resources_runtime_assets)
    game_apply_cpp_defaults(human_resources_military_animation_tests)
    add_test(NAME military_animation_contracts COMMAND human_resources_military_animation_tests "${CMAKE_CURRENT_BINARY_DIR}/assets/runtime/game")
  endif()

  set(game_client_sources native/human_resources_client/src/app.cpp
    native/human_resources_client/src/resume_qa.cpp)
  if(ANDROID)
    add_library(main SHARED ${game_client_sources})
    set(game_client_target main)
  elseif(IOS)
    add_executable(human_resources_player MACOSX_BUNDLE ${game_client_sources}
      ios/LaunchScreen.storyboard
      ios/PrivacyInfo.xcprivacy
    )
    set(game_client_target human_resources_player)
    set_source_files_properties(ios/LaunchScreen.storyboard ios/PrivacyInfo.xcprivacy
      PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(human_resources_player PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"
      XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.xanderjm.humanresources"
      XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
      RESOURCE "ios/LaunchScreen.storyboard;ios/PrivacyInfo.xcprivacy"
    )
    include(AppIcons)
    game_add_ios_app_icon(human_resources_player)
  else()
    add_executable(human_resources_player ${game_client_sources})
    if(WIN32)
      set_target_properties(human_resources_player PROPERTIES WIN32_EXECUTABLE ON)
    endif()
    set(game_client_target human_resources_player)
  endif()

  add_dependencies(${game_client_target} human_resources_shaders)
  add_dependencies(${game_client_target} human_resources_runtime_assets)
  target_link_libraries(${game_client_target} PRIVATE
    human_resources_product
    human_resources_game_renderer
    human_resources_audio
    human_resources_assets
    human_resources_input
    human_resources_lifecycle
    human_resources_platform
    human_resources_renderer
    human_resources_storage
    SDL3::SDL3
  )
  if(HUMAN_RESOURCES_FEATURE_DYNAMIC_TEXT)
    target_link_libraries(${game_client_target} PRIVATE SDL3_ttf::SDL3_ttf)
  endif()
  target_compile_definitions(${game_client_target} PRIVATE SDL_MAIN_USE_CALLBACKS=1)
  game_apply_cpp_defaults(${game_client_target})

  if(APPLE)
    set_target_properties(${game_client_target} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
  endif()

  if(NOT ANDROID)
    # Every asset consumer shares one staging owner; assets need no executable relink.
    # Xcode's per-file resource copies leave deleted files in an existing bundle.
    # Resolve Xcode's actual products directory at build time: generated target
    # paths contain a literal EFFECTIVE_PLATFORM_NAME placeholder in this phase.
    # CMP0112 NEW prevents TARGET_BUNDLE_DIR from adding a reverse dependency.
    if(IOS)
      set(game_runtime_stage_arguments --xcode-bundle-root "$<TARGET_BUNDLE_DIR:${game_client_target}>")
    else()
      set(game_runtime_stage_arguments --output-root "${CMAKE_CURRENT_BINARY_DIR}/assets")
    endif()
    add_custom_target(human_resources_stage_runtime_assets
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generators/stage_runtime_assets.py"
              --source-root "${GAME_GENERATED_ROOT}/assets"
              ${game_runtime_stage_arguments}
      DEPENDS human_resources_runtime_assets human_resources_shaders
      VERBATIM)
    add_dependencies(${game_client_target} human_resources_stage_runtime_assets)
  endif()
endif()

# Development visual study has its own shader/content boundary and is excluded
# from default builds and shipped clients. No production asset approval implied.
if(HUMAN_RESOURCES_BUILD_CLIENT AND NOT ANDROID AND NOT IOS)
  set(style_generated_root "${GAME_GENERATED_ROOT}/style-preview")
  game_add_shaders(human_resources_style_shaders style_shader_files
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/style-preview/shaders" "${style_generated_root}")
  add_library(human_resources_style_renderer STATIC EXCLUDE_FROM_ALL
    native/human_resources_renderer/src/style_renderer.cpp
    native/human_resources_renderer/src/style_resources.cpp
    native/human_resources_renderer/src/style_surfaces.cpp
    native/human_resources_renderer/src/style_animation.cpp)
  target_include_directories(human_resources_style_renderer PUBLIC
    native/human_resources_renderer/include
    PRIVATE "${style_generated_root}/shaders/include")
  target_link_libraries(human_resources_style_renderer PUBLIC
    human_resources_presentation human_resources_assets human_resources_animation_instances SDL3::SDL3)
  add_dependencies(human_resources_style_renderer human_resources_style_shaders)
  game_apply_cpp_defaults(human_resources_style_renderer)
  add_custom_target(human_resources_stage_style_shaders
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${style_generated_root}/assets/shaders" "${CMAKE_CURRENT_BINARY_DIR}/style-preview-shaders"
    DEPENDS human_resources_style_shaders VERBATIM)
  add_executable(human_resources_style_preview EXCLUDE_FROM_ALL
    native/human_resources_client/src/style_preview.cpp)
  target_link_libraries(human_resources_style_preview PRIVATE human_resources_style_renderer)
  add_dependencies(human_resources_style_preview human_resources_stage_style_shaders)
  game_apply_cpp_defaults(human_resources_style_preview)
endif()

if(HUMAN_RESOURCES_BUILD_CLIENT AND APPLE AND NOT IOS)
  add_custom_target(human_resources_macos_app
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generators/stage_macos_app.py"
      --executable "$<TARGET_FILE:human_resources_player>"
      --assets "${CMAKE_CURRENT_BINARY_DIR}/assets"
      --output "${CMAKE_CURRENT_BINARY_DIR}/Human Resources.app"
    DEPENDS human_resources_player human_resources_stage_runtime_assets VERBATIM)
endif()
