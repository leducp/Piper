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

option(BUILD_TESTS        "Build tests"             ON)
option(BUILD_PY_BINDINGS  "Build Python bindings"   OFF)

add_subdirectory(core)
add_subdirectory(canvas)
add_subdirectory(vendor/imgui_backends)
add_subdirectory(app)
add_subdirectory(migrate)
add_subdirectory(examples/canvas_demo)

if (BUILD_TESTS)
    add_subdirectory(tests)
endif()
