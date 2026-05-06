add_library(piper_test_fixtures STATIC
    build_motor_dual_jacobian.cc
    test_helpers.cc
)
target_link_libraries(piper_test_fixtures PUBLIC piper_core)
target_include_directories(piper_test_fixtures PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(piper_build_motor_dual_jacobian main_dual_jacobian.cc)
target_link_libraries(piper_build_motor_dual_jacobian PRIVATE piper_test_fixtures)
