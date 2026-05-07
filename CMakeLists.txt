cmake_minimum_required(VERSION 3.28)

project(piper)

if (SKBUILD AND NOT CMAKE_CI_BUILD)
    execute_process(
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/setup_build.sh" "${CMAKE_BINARY_DIR}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_BINARY_DIR})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(flags)

enable_testing()

# Wheel builds (scikit-build-core sets SKBUILD) ship core + Python
# bindings only -- the editor and demos pull in ImGui/GLFW which the
# wheel has no use for.
if (SKBUILD)
    set(BUILD_TESTS_DEFAULT       OFF)
    set(BUILD_PY_BINDINGS_DEFAULT ON)
    set(BUILD_APP_DEFAULT         OFF)
else()
    set(BUILD_TESTS_DEFAULT       ON)
    set(BUILD_PY_BINDINGS_DEFAULT OFF)
    set(BUILD_APP_DEFAULT         ON)
endif()

option(BUILD_TESTS        "Build tests"             ${BUILD_TESTS_DEFAULT})
option(BUILD_PY_BINDINGS  "Build Python bindings"   ${BUILD_PY_BINDINGS_DEFAULT})
option(BUILD_APP          "Build the editor app"    ${BUILD_APP_DEFAULT})

add_subdirectory(core)
add_subdirectory(engine)
add_subdirectory(examples/filter_demo)
add_subdirectory(examples/am_radio)
add_subdirectory(examples/pid_demo)

if (BUILD_APP)
    add_subdirectory(canvas)
    add_subdirectory(vendor/imgui_backends)
    add_subdirectory(vendor/implot)
    add_subdirectory(app)
    add_subdirectory(migrate)
    add_subdirectory(examples/canvas_demo)
endif()

if (BUILD_PY_BINDINGS)
    add_subdirectory(py_bindings)
endif()

if (BUILD_TESTS)
    add_subdirectory(tests)
endif()
