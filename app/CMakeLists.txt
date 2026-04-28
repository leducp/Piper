add_executable(piper_editor
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cc
)
target_include_directories(piper_editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(piper_editor PRIVATE
    piper_canvas
    piper_core
    imgui_backend
)
set_target_properties(piper_editor PROPERTIES OUTPUT_NAME piper-editor)
