set(PIPER_ENGINE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/registry.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/step.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin_steps.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/steps/constant_step.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/steps/sin_wave_step.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/steps/low_pass_step.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/steps/probe_step.cc
)

add_library(piper_engine STATIC ${PIPER_ENGINE_SRCS})
target_include_directories(piper_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(piper_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(piper_engine PUBLIC piper_core)
set_target_properties(piper_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
