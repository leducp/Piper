add_executable(piper_core_test
    basic_test.cc
    typecheck_test.cc
)
target_link_libraries(piper_core_test PRIVATE
    piper_core
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(piper_core_test
    PROPERTIES TIMEOUT 60
)
