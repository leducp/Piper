find_package(imgui REQUIRED)

# GLFW from source via FetchContent — Conan's xorg system-requirements
# are painful and require sudo for first-time setup.
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
    GIT_TAG        7b6aead9fb88b3623e3b3725ebb42670cbe4c579 # tag 3.4
)
FetchContent_MakeAvailable(glfw)

find_package(OpenGL REQUIRED)

add_library(imgui_backend STATIC
    imgui_impl_glfw.cpp
    imgui_impl_opengl3.cpp
    imgui_stdlib.cpp
)
target_include_directories(imgui_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(imgui_backend PUBLIC imgui::imgui glfw OpenGL::GL)

# Vendored upstream code: suppress style warnings on these TUs only.
set_source_files_properties(
    imgui_impl_glfw.cpp
    imgui_impl_opengl3.cpp
    imgui_stdlib.cpp
    PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-cast-qual"
)

set_target_properties(imgui_backend PROPERTIES POSITION_INDEPENDENT_CODE ON)
