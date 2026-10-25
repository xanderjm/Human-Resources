function(game_add_shaders target output_variable)
  if(NOT EXISTS "${GAME_SHADERCROSS_EXECUTABLE}")
    message(FATAL_ERROR "Host shadercross is missing. Run scripts/build-host-tools.sh: ${GAME_SHADERCROSS_EXECUTABLE}")
  endif()

  if(NOT ARGC EQUAL 2 AND NOT ARGC EQUAL 4)
    message(FATAL_ERROR "game_add_shaders expects target, files, and optional source/output roots")
  endif()
  # Optional source/output roots allow development renderers to share the
  # offline compiler without adding their shaders to the shipping allowlist.
  set(shader_source_root "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
  set(shader_generated_root "${GAME_GENERATED_ROOT}")
  if(ARGC GREATER 2)
    set(shader_source_root "${ARGV2}")
    set(shader_generated_root "${ARGV3}")
  endif()
  set(shader_asset_root "${shader_generated_root}/assets/shaders")
  set(shader_include_root "${shader_generated_root}/shaders/include")

  set(shader_platform "${GAME_SHADER_PLATFORM}")
  if(shader_platform STREQUAL "auto")
    if(ANDROID)
      set(shader_platform android)
    elseif(IOS)
      set(shader_platform ios)
    elseif(APPLE)
      set(shader_platform macos)
    elseif(WIN32)
      set(shader_platform windows)
    else()
      set(shader_platform linux)
    endif()
  endif()

  set(shader_stamp "${shader_generated_root}/shaders/${shader_platform}.stamp")
  set(apple_ios_sdk iphoneos)
  if(CMAKE_OSX_SYSROOT MATCHES "iphonesimulator|iPhoneSimulator")
    set(apple_ios_sdk iphonesimulator)
  endif()
  set(shader_manifest "${shader_source_root}/shader_manifest.json")
  file(READ "${shader_manifest}" shader_manifest_json)
  string(JSON shader_program_count LENGTH "${shader_manifest_json}" programs)
  math(EXPR shader_program_last "${shader_program_count} - 1")
  set(shader_package_files "${shader_asset_root}/package-manifest.json")
  foreach(program_index RANGE 0 ${shader_program_last})
    string(JSON program_name GET "${shader_manifest_json}" programs ${program_index} name)
    string(JSON shader_stage_count LENGTH "${shader_manifest_json}" programs ${program_index} stages)
    math(EXPR shader_stage_last "${shader_stage_count} - 1")
    foreach(stage_index RANGE 0 ${shader_stage_last})
      string(JSON stage_name GET "${shader_manifest_json}" programs ${program_index} stages ${stage_index} stage)
      set(shader_stem "${program_name}.${stage_name}")
      list(APPEND shader_package_files
        "${shader_asset_root}/reflection/${shader_stem}.json")
      if(shader_platform STREQUAL "ios")
        list(APPEND shader_package_files
          "${shader_asset_root}/metal/ios/${shader_stem}.metallib")
      elseif(shader_platform STREQUAL "macos")
        list(APPEND shader_package_files
          "${shader_asset_root}/metal/macos/${shader_stem}.metallib")
      elseif(shader_platform STREQUAL "windows")
        list(APPEND shader_package_files
          "${shader_asset_root}/dxil/${shader_stem}.dxil")
      elseif(shader_platform STREQUAL "android" OR shader_platform STREQUAL "linux")
        list(APPEND shader_package_files
          "${shader_asset_root}/spirv/${shader_stem}.spv")
      else()
        message(FATAL_ERROR "Unsupported GAME_SHADER_PLATFORM: ${shader_platform}")
      endif()
    endforeach()
  endforeach()
  set_source_files_properties(${shader_package_files} PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
  file(GLOB_RECURSE shader_sources CONFIGURE_DEPENDS
    "${shader_source_root}/src/*.hlsl"
    "${shader_source_root}/include/*.hlsli")
  file(GENERATE OUTPUT "${shader_generated_root}/shader-sources.txt"
    CONTENT "$<JOIN:${shader_sources},\n>\n")
  add_custom_command(
    OUTPUT "${shader_stamp}"
    # Reflection consumers must rebuild in the same invocation as shader cooking.
    BYPRODUCTS ${shader_package_files}
               "${shader_include_root}/shader_manifest.generated.h"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generators/compile_shaders.py"
            --shadercross "${GAME_SHADERCROSS_EXECUTABLE}"
            --source-root "${shader_source_root}"
            --output-root "${shader_asset_root}"
            --include-output "${shader_include_root}"
            --platform "${shader_platform}"
            --apple-ios-sdk "${apple_ios_sdk}"
            --apple-deployment-target "$<IF:$<BOOL:${CMAKE_OSX_DEPLOYMENT_TARGET}>,${CMAKE_OSX_DEPLOYMENT_TARGET},17.0>"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_generated_root}/shaders"
    COMMAND "${CMAKE_COMMAND}" -E touch "${shader_stamp}"
    DEPENDS ${shader_sources}
            "${shader_generated_root}/shader-sources.txt"
            "${shader_manifest}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generators/compile_shaders.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generators/source_inventory.py"
    VERBATIM
  )
  add_custom_target(${target} DEPENDS "${shader_stamp}")
  set(${output_variable} "${shader_package_files}" PARENT_SCOPE)
endfunction()
