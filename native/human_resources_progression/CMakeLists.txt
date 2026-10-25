add_library(human_resources_progression STATIC
  src/account_session.cpp
  src/account.cpp
  src/account_store.cpp
  src/serialization.cpp
  src/continuation.cpp
)
target_include_directories(human_resources_progression PUBLIC include)
target_link_libraries(human_resources_progression PUBLIC human_resources_storage human_resources_gameplay)
game_apply_cpp_defaults(human_resources_progression)
if(HUMAN_RESOURCES_BUILD_TESTS)
  enable_testing()
  add_executable(human_resources_account_resume_tests "${PROJECT_SOURCE_DIR}/native/tests/account_resume_tests.cpp")
  target_link_libraries(human_resources_account_resume_tests PRIVATE human_resources_progression)
  game_apply_cpp_defaults(human_resources_account_resume_tests)
  add_test(NAME account_resume_contracts COMMAND human_resources_account_resume_tests)
endif()
