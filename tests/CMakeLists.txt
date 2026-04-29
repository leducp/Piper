find_package(GTest REQUIRED)

include(GoogleTest)

add_subdirectory(fixtures)
add_subdirectory(core)
add_subdirectory(engine)
add_subdirectory(canvas)
add_subdirectory(migrate)
