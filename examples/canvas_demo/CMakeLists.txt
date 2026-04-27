find_package(imgui REQUIRED)

# GLFW from source — Conan's xorg system-requirements are painful;
# FetchContent matches real-time-monitor's working setup.
set(GLFW_BUILD_NULL     ON  CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11      ON  CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND  ON  CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(GLFW_LIBRARY_TYPE   "STATIC" CACHE STRING "" FORCE)

include(FetchContent)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

find_package(OpenGL REQUIRED)

add_executable(canvas_demo
    main.cc
    backend/imgui_impl_glfw.cpp
    backend/imgui_impl_opengl3.cpp
)
target_include_directories(canvas_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)
target_link_libraries(canvas_demo PRIVATE
    piper_canvas
    imgui::imgui
    glfw
    OpenGL::GL
)

# Vendored backends are upstream code; suppress style warnings only on
# those translation units. main.cc must stay clean under full warnings.
set_source_files_properties(
    backend/imgui_impl_glfw.cpp
    backend/imgui_impl_opengl3.cpp
    PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-cast-qual"
)
