add_executable(piper_core_test
    basic-t.cc
    diagnostics-t.cc
    mode_roundtrip-t.cc
    registry_roundtrip-t.cc
    roundtrip-t.cc
    typecheck-t.cc
    undo-t.cc
)
target_link_libraries(piper_core_test PRIVATE
    piper_core
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(piper_core_test
    PROPERTIES TIMEOUT 60
)
