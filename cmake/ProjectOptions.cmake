option(HUMAN_RESOURCES_BUILD_CLIENT "Build the SDL3 player" ON)
option(HUMAN_RESOURCES_BUILD_TESTS "Build foundation contract tests" ON)
option(HUMAN_RESOURCES_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(HUMAN_RESOURCES_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(HUMAN_RESOURCES_ENABLE_CLANG_TIDY "Run the pinned clang-tidy on game-owned C++" OFF)
option(HUMAN_RESOURCES_ENABLE_COVERAGE "Enable LLVM source coverage instrumentation" OFF)

set(HUMAN_RESOURCES_FEATURE_DETERMINISTIC_AUTHORITY OFF
    CACHE BOOL "Generated deterministic-authority feature selection" FORCE)
set(HUMAN_RESOURCES_FEATURE_DYNAMIC_TEXT OFF
    CACHE BOOL "Generated dynamic-text feature selection" FORCE)

set(GAME_LLVM_TOOL_VERSION "22.1.8" CACHE STRING "Required clang-format/clang-tidy/coverage tool version")

if(HUMAN_RESOURCES_ENABLE_CLANG_TIDY)
  find_program(GAME_CLANG_TIDY_EXECUTABLE NAMES clang-tidy-22 clang-tidy REQUIRED)
  execute_process(COMMAND "${GAME_CLANG_TIDY_EXECUTABLE}" --version
    OUTPUT_VARIABLE GAME_CLANG_TIDY_VERSION_OUTPUT ERROR_VARIABLE GAME_CLANG_TIDY_VERSION_OUTPUT)
  if(NOT GAME_CLANG_TIDY_VERSION_OUTPUT MATCHES "${GAME_LLVM_TOOL_VERSION}")
    message(FATAL_ERROR "clang-tidy ${GAME_LLVM_TOOL_VERSION} is required: ${GAME_CLANG_TIDY_VERSION_OUTPUT}")
  endif()
  set(GAME_CLANG_TIDY_COMMAND
    "${GAME_CLANG_TIDY_EXECUTABLE}"
    "--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
  )
  if(APPLE AND NOT IOS)
    execute_process(
      COMMAND xcrun --sdk macosx --show-sdk-path
      OUTPUT_VARIABLE GAME_MACOS_SDK_PATH
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE GAME_MACOS_SDK_RESULT
    )
    if(NOT GAME_MACOS_SDK_RESULT EQUAL 0 OR NOT IS_DIRECTORY "${GAME_MACOS_SDK_PATH}")
      message(FATAL_ERROR "clang-tidy requires a valid macOS SDK from xcrun")
    endif()
    list(APPEND GAME_CLANG_TIDY_COMMAND
      "--extra-arg=-isysroot"
      "--extra-arg=${GAME_MACOS_SDK_PATH}"
    )
  endif()
endif()

if(HUMAN_RESOURCES_ENABLE_COVERAGE AND HUMAN_RESOURCES_ENABLE_SANITIZERS)
  message(FATAL_ERROR "Coverage and sanitizer instrumentation use separate qualified presets")
endif()
if(HUMAN_RESOURCES_ENABLE_COVERAGE AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "The coverage preset requires Clang/LLVM")
endif()

if(CMAKE_HOST_WIN32)
  set(GAME_HOST_EXECUTABLE_SUFFIX ".exe")
else()
  set(GAME_HOST_EXECUTABLE_SUFFIX "")
endif()
set(GAME_FLATC_EXECUTABLE "${CMAKE_SOURCE_DIR}/build/host-tools/flatbuffers/flatc${GAME_HOST_EXECUTABLE_SUFFIX}"
    CACHE FILEPATH "Host flatc executable")
set(GAME_SHADERCROSS_EXECUTABLE "${CMAKE_SOURCE_DIR}/build/host-tools/shadercross/shadercross${GAME_HOST_EXECUTABLE_SUFFIX}"
    CACHE FILEPATH "Host SDL_shadercross executable")
set(GAME_SHADER_PLATFORM "auto" CACHE STRING "Shader package: auto, macos, ios, android, windows, linux")

function(game_apply_cpp_defaults target)
  target_compile_features(${target} PUBLIC cxx_std_20)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(HUMAN_RESOURCES_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      # Sparse C++20 designated initializers intentionally use member defaults
      # for event/config value objects.
      target_compile_options(${target} PRIVATE -Wno-missing-designated-field-initializers)
    endif()
    if(HUMAN_RESOURCES_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(HUMAN_RESOURCES_ENABLE_SANITIZERS)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
    if(HUMAN_RESOURCES_ENABLE_COVERAGE)
      target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
      target_link_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    endif()
  endif()
  if(HUMAN_RESOURCES_ENABLE_CLANG_TIDY)
    set_property(TARGET ${target} PROPERTY CXX_CLANG_TIDY "${GAME_CLANG_TIDY_COMMAND}")
  endif()
endfunction()
