set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WARNINGS_FLAGS -Wall -Wextra -pedantic -Wconversion -Wunused -Wshadow -Wnull-dereference)
set(WARNINGS_FLAGS ${WARNINGS_FLAGS} -Wunused-parameter -Wunused-variable -Wuninitialized)
set(WARNINGS_FLAGS ${WARNINGS_FLAGS} -Wreturn-type -Wsequence-point -Wmisleading-indentation)
set(WARNINGS_FLAGS ${WARNINGS_FLAGS} -Wmissing-noreturn -Wcast-qual -Wcast-align)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(WARNINGS_FLAGS ${WARNINGS_FLAGS} -Wbool-compare -Wno-maybe-uninitialized -Wlogical-op -Wduplicated-cond)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
   CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options(${WARNINGS_FLAGS})
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}. Only GCC, Clang, and AppleClang are supported: please add your definitions here.")
endif()

if(MINGW)
    # Statically link the gcc/libstdc++/winpthread runtimes so the binary
    # can't load a shadowing libstdc++-6.dll from another toolchain on
    # PATH -- a mismatched one corrupts libstdc++'s locale/iostream state
    # and crashes std::ifstream while leaving simpler ops working.
    add_link_options(-static)
endif()
