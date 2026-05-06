add_executable(pid_demo main.cc)
target_link_libraries(pid_demo PRIVATE
    piper_core
    piper_engine
)
target_compile_definitions(pid_demo PRIVATE
    PIPER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
