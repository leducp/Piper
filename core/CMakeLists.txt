set(PIPER_CORE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graph.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/registry.cc
)

add_library(piper_core STATIC ${PIPER_CORE_SRCS})
target_include_directories(piper_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(piper_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
