find_package(Python 3.10 COMPONENTS Interpreter Development.Module REQUIRED)

# Discover nanobind via the active Python interpreter (pip-installed
# package), matching the wheel's expected build environment. CMake's
# find_package then locates the shipped CMake config files.
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE nanobind_DIR
)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(piper_py
    NB_STATIC
    STABLE_ABI
    ${CMAKE_CURRENT_SOURCE_DIR}/src/piper_module.cc
)
target_link_libraries(piper_py PRIVATE piper_core)
set_target_properties(piper_py PROPERTIES OUTPUT_NAME piper)

if (SKBUILD)
    install(TARGETS piper_py LIBRARY DESTINATION .)
endif()

if (BUILD_TESTS)
    add_test(NAME piper_py_bindings
        COMMAND ${Python_EXECUTABLE} -m unittest discover
                -s ${CMAKE_CURRENT_SOURCE_DIR}/tests
                -p "test_*.py")
    set_tests_properties(piper_py_bindings PROPERTIES
        ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:piper_py>")
endif()
