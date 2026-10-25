set(HR_GAMEPLAY_GENERATED_INCLUDE "${GAME_GENERATED_ROOT}/gameplay/include")
set(HR_GAMEPLAY_TUNING "${HR_GAMEPLAY_GENERATED_INCLUDE}/human_resources/gameplay/tuning.h")
add_custom_command(
  OUTPUT "${HR_GAMEPLAY_TUNING}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_tuning.py"
    --root "${PROJECT_SOURCE_DIR}" --output "${HR_GAMEPLAY_TUNING}"
  DEPENDS tools/generate_tuning.py runtime-spatial.json
    "${PROJECT_SOURCE_DIR}/assets/production/vehicle-weapons.json"
    "${PROJECT_SOURCE_DIR}/assets/animation/characters.json"
    "${PROJECT_SOURCE_DIR}/assets/animation/infantry.json"
    "${PROJECT_SOURCE_DIR}/design/balance/archero-ufo-system.json"
    "${PROJECT_SOURCE_DIR}/design/balance/endless-upgrade-system.json"
    "${PROJECT_SOURCE_DIR}/design/balance/endless-encounter-assumptions.json"
  VERBATIM)
add_library(human_resources_gameplay STATIC
  src/run.cpp src/director.cpp src/military.cpp src/minigun.cpp src/cannon.cpp src/body_separation.cpp src/capture.cpp src/combat.cpp src/upgrades.cpp src/save_state.cpp
  "${HR_GAMEPLAY_TUNING}")
target_include_directories(human_resources_gameplay PUBLIC include "${HR_GAMEPLAY_GENERATED_INCLUDE}")
game_apply_cpp_defaults(human_resources_gameplay)
if(HUMAN_RESOURCES_BUILD_TESTS)
  enable_testing()
  add_executable(human_resources_gameplay_save_tests "${PROJECT_SOURCE_DIR}/native/tests/gameplay_save_tests.cpp")
  target_link_libraries(human_resources_gameplay_save_tests PRIVATE human_resources_gameplay)
  game_apply_cpp_defaults(human_resources_gameplay_save_tests)
  add_test(NAME gameplay_save_contracts COMMAND human_resources_gameplay_save_tests)
  # These isolated private-state QA fixtures use the established Clang/GNU
  # access-control switch. Public gameplay/save tests remain portable everywhere.
  if(NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_executable(human_resources_harvest_gameplay_tests "${PROJECT_SOURCE_DIR}/native/tests/harvest_gameplay_tests.cpp")
    target_include_directories(human_resources_harvest_gameplay_tests PRIVATE src)
    target_link_libraries(human_resources_harvest_gameplay_tests PRIVATE human_resources_gameplay)
    target_compile_options(human_resources_harvest_gameplay_tests PRIVATE -fno-access-control)
    game_apply_cpp_defaults(human_resources_harvest_gameplay_tests)
    add_test(NAME harvest_gameplay_contracts COMMAND human_resources_harvest_gameplay_tests)
    add_executable(human_resources_encounter_gameplay_tests "${PROJECT_SOURCE_DIR}/native/tests/encounter_gameplay_tests.cpp")
    target_include_directories(human_resources_encounter_gameplay_tests PRIVATE src)
    target_link_libraries(human_resources_encounter_gameplay_tests PRIVATE human_resources_gameplay)
    target_compile_options(human_resources_encounter_gameplay_tests PRIVATE -fno-access-control)
    game_apply_cpp_defaults(human_resources_encounter_gameplay_tests)
    add_test(NAME encounter_gameplay_contracts COMMAND human_resources_encounter_gameplay_tests)
    add_executable(human_resources_military_gameplay_tests "${PROJECT_SOURCE_DIR}/native/tests/military_gameplay_tests.cpp")
    target_include_directories(human_resources_military_gameplay_tests PRIVATE src)
    target_link_libraries(human_resources_military_gameplay_tests PRIVATE human_resources_gameplay)
    target_compile_options(human_resources_military_gameplay_tests PRIVATE -fno-access-control)
    game_apply_cpp_defaults(human_resources_military_gameplay_tests)
    add_test(NAME military_gameplay_contracts COMMAND human_resources_military_gameplay_tests)
    add_executable(human_resources_vehicle_gameplay_tests "${PROJECT_SOURCE_DIR}/native/tests/vehicle_gameplay_tests.cpp")
    target_include_directories(human_resources_vehicle_gameplay_tests PRIVATE src)
    target_link_libraries(human_resources_vehicle_gameplay_tests PRIVATE human_resources_gameplay)
    target_compile_options(human_resources_vehicle_gameplay_tests PRIVATE -fno-access-control)
    game_apply_cpp_defaults(human_resources_vehicle_gameplay_tests)
    add_test(NAME vehicle_gameplay_contracts COMMAND human_resources_vehicle_gameplay_tests)
  endif()
endif()
