add_executable(am_radio main.cc)
target_link_libraries(am_radio PRIVATE
    piper_core
    piper_engine
)
target_compile_definitions(am_radio PRIVATE
    PIPER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
