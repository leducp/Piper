add_executable(piper_migrate_test
    v1_reader-t.cc
)
target_link_libraries(piper_migrate_test PRIVATE
    piper_core
    piper_migrate
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(piper_migrate_test
    PROPERTIES TIMEOUT 60
)
