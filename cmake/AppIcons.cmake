# PNG-only launcher resources. Ensure the catalogue exists before Xcode plans
# actool, then validate/regenerate outputs before each app build.
function(game_add_ios_app_icon target)
  set(icon_root "${CMAKE_BINARY_DIR}/generated/app-icon")
  set(icon_producer "${PROJECT_SOURCE_DIR}/tools/generators/cook_app_icons.py")
  set(icon_catalogue "${icon_root}/ios/Assets.xcassets")
  set(icon_provenance "${icon_root}/ios/app-icon-provenance.json")
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${icon_producer}"
    --output-root "${icon_root}" COMMAND_ERROR_IS_FATAL ANY)
  add_custom_target(human_resources_app_icon
    COMMAND "${Python3_EXECUTABLE}" "${icon_producer}" --output-root "${icon_root}"
    BYPRODUCTS "${icon_catalogue}/Contents.json"
      "${icon_catalogue}/AppIcon.appiconset/Contents.json"
      "${icon_catalogue}/AppIcon.appiconset/AppIcon-1024.png" "${icon_provenance}"
    VERBATIM)
  add_dependencies(${target} human_resources_app_icon)
  target_sources(${target} PRIVATE "${icon_catalogue}" "${icon_provenance}")
  set_source_files_properties("${icon_catalogue}" PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources XCODE_EXPLICIT_FILE_TYPE folder.assetcatalog)
  set_source_files_properties("${icon_provenance}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_property(TARGET ${target} APPEND PROPERTY RESOURCE "${icon_catalogue}" "${icon_provenance}")
  set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon)
endfunction()
