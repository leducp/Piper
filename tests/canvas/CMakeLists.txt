add_executable(piper_canvas_test
    aabb-t.cc
    cull-t.cc
    hit_test-t.cc
    link_routing-t.cc
    pin_layout-t.cc
    selection-t.cc
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
