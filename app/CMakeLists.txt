find_package(portable-file-dialogs REQUIRED)

add_executable(piper_editor
    ${CMAKE_CURRENT_SOURCE_DIR}/canvas_adapter.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/document.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/main_window.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/inspector_panel.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/modes_panel.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/stages_panel.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/theme_loader.cc
)
target_include_directories(piper_editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(piper_editor PRIVATE
    piper_canvas
    piper_core
    imgui_backend
    portable-file-dialogs::portable-file-dialogs
)
set_target_properties(piper_editor PROPERTIES OUTPUT_NAME piper-editor)
