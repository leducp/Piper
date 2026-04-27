add_executable(piper_canvas_test
    transform-t.cc
)
target_link_libraries(piper_canvas_test PRIVATE
    piper_canvas
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(piper_canvas_test
    PROPERTIES TIMEOUT 60
)
