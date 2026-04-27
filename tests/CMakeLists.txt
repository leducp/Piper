find_package(GTest REQUIRED)

add_executable(piper_test
    core_basic_test.cc
)
target_link_libraries(piper_test PRIVATE
    piper_core
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(piper_test
    PROPERTIES TIMEOUT 60
)
