add_executable(engine_demo main.cc)
target_link_libraries(engine_demo PRIVATE
    piper_core
    piper_engine
)
target_compile_definitions(engine_demo PRIVATE
    PIPER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
