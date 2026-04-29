find_package(argparse REQUIRED)

add_library(piper_migrate STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/v1_reader.cc
)
target_include_directories(piper_migrate PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(piper_migrate PUBLIC
    piper_core
)

add_executable(piper_migrate_cli
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cc
)
target_link_libraries(piper_migrate_cli PRIVATE
    piper_migrate
    argparse::argparse
)
set_target_properties(piper_migrate_cli PROPERTIES OUTPUT_NAME piper-migrate)
