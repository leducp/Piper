find_package(imgui REQUIRED)

set(PIPER_CANVAS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cull.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hit_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/log.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/selection.cc
)

add_library(piper_canvas STATIC ${PIPER_CANVAS_SRCS})
target_include_directories(piper_canvas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Layering rule: piper_canvas links ONLY imgui. It must NOT link
# piper_core, GLFW, or any GUI windowing dep — keeps the framework
# domain-agnostic and reusable.
target_link_libraries(piper_canvas PUBLIC imgui::imgui)

set_target_properties(piper_canvas PROPERTIES POSITION_INDEPENDENT_CODE ON)
