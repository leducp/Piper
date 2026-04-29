add_executable(piper_engine_test
    build_smoke-t.cc
    cycle-t.cc
    declare_io_failure-t.cc
    missing_factory-t.cc
    motor_control_smoke-t.cc
    multi_stage_tick-t.cc
    single_stage_tick-t.cc
)
target_link_libraries(piper_engine_test PRIVATE
    piper_core
    piper_engine
    piper_test_fixtures
    GTest::gtest
    GTest::gtest_main
)
target_compile_definitions(piper_engine_test PRIVATE
    PIPER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

gtest_discover_tests(piper_engine_test
    PROPERTIES TIMEOUT 60
)
