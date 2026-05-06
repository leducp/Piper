add_executable(filter_demo main.cc)
target_link_libraries(filter_demo PRIVATE
    piper_core
    piper_engine
)
target_compile_definitions(filter_demo PRIVATE
    PIPER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
