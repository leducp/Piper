find_package(nlohmann_json REQUIRED)

set(PIPER_CORE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin_nodes.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/color.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/command_stack.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/commands.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/connect.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/diagnostic_helpers.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graph.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/registry.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rgba_io.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/serialize_v2.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/theme.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vec.cc
)

add_library(piper_core STATIC ${PIPER_CORE_SRCS})
target_include_directories(piper_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(piper_core PRIVATE nlohmann_json::nlohmann_json)
set_target_properties(piper_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Wheels (SKBUILD) install only the Python module.
if (NOT SKBUILD)
    install(TARGETS piper_core ARCHIVE DESTINATION lib)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include)
endif()
