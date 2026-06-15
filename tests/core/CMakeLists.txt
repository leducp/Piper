add_executable(piper_core_test
    basic-t.cc
    bundle-t.cc
    builtin_nodes-t.cc
    color-t.cc
    command_regression-t.cc
    diagnostics-t.cc
    example_load-t.cc
    format_pass-t.cc
    label_links-t.cc
    load_validation-t.cc
    mode_roundtrip-t.cc
    registry_roundtrip-t.cc
    roundtrip-t.cc
    stage_order-t.cc
    theme-t.cc
    typecheck-t.cc
    undo-t.cc
    vec-t.cc
)
target_link_libraries(piper_core_test PRIVATE
    piper_core
    piper_test_fixtures
    GTest::gtest
    GTest::gtest_main
)
target_compile_definitions(piper_core_test PRIVATE
    PIPER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

gtest_discover_tests(piper_core_test
    PROPERTIES TIMEOUT 60
)
