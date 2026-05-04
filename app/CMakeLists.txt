find_package(portable-file-dialogs REQUIRED)
find_package(nlohmann_json REQUIRED)

file(GLOB PIPER_BUNDLED_FONTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.otf"
)
set(PIPER_BUNDLED_FONTS_CC "${CMAKE_CURRENT_BINARY_DIR}/bundled_fonts.cc")
add_custom_command(
    OUTPUT  "${PIPER_BUNDLED_FONTS_CC}"
    COMMAND ${CMAKE_COMMAND}
            -DOUTPUT=${PIPER_BUNDLED_FONTS_CC}
            "-DFONTS=${PIPER_BUNDLED_FONTS}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_fonts.cmake"
    DEPENDS ${PIPER_BUNDLED_FONTS}
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_fonts.cmake"
    VERBATIM
)

file(GLOB PIPER_BUNDLED_LICENSES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/licenses/*.txt"
)
set(PIPER_BUNDLED_LICENSES_CC "${CMAKE_CURRENT_BINARY_DIR}/bundled_licenses.cc")
add_custom_command(
    OUTPUT  "${PIPER_BUNDLED_LICENSES_CC}"
    COMMAND ${CMAKE_COMMAND}
            -DOUTPUT=${PIPER_BUNDLED_LICENSES_CC}
            "-DLICENSES=${PIPER_BUNDLED_LICENSES}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_licenses.cmake"
    DEPENDS ${PIPER_BUNDLED_LICENSES}
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_licenses.cmake"
    VERBATIM
)

set(PIPER_PROJECT_LICENSE      "${CMAKE_SOURCE_DIR}/LICENCE")
set(PIPER_PROJECT_LICENSE_CC   "${CMAKE_CURRENT_BINARY_DIR}/project_license.cc")
add_custom_command(
    OUTPUT  "${PIPER_PROJECT_LICENSE_CC}"
    COMMAND ${CMAKE_COMMAND}
            -DOUTPUT=${PIPER_PROJECT_LICENSE_CC}
            -DSOURCE=${PIPER_PROJECT_LICENSE}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_project_license.cmake"
    DEPENDS ${PIPER_PROJECT_LICENSE}
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_project_license.cmake"
    VERBATIM
)

add_executable(piper_editor
    ${CMAKE_CURRENT_SOURCE_DIR}/autosave.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/canvas_adapter.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/document.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/main_window.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/minimap.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/inspector_panel.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/modes_panel.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/stages_panel.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/settings.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/theme_loader.cc
    ${PIPER_BUNDLED_FONTS_CC}
    ${PIPER_BUNDLED_LICENSES_CC}
    ${PIPER_PROJECT_LICENSE_CC}
)
target_include_directories(piper_editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(piper_editor PRIVATE
    piper_canvas
    piper_core
    imgui_backend
    nlohmann_json::nlohmann_json
    portable-file-dialogs::portable-file-dialogs
)
set_target_properties(piper_editor PROPERTIES OUTPUT_NAME piper-editor)
