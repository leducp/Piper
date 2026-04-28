add_executable(canvas_demo main.cc)
target_link_libraries(canvas_demo PRIVATE
    piper_canvas
    imgui_backend
)
