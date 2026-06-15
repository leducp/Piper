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

# ---- piper-migrate CLI end-to-end ----
set(MIGRATE_CLI_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/cli)
set(MIGRATE_DATA        ${CMAKE_CURRENT_SOURCE_DIR}/data)

add_test(NAME migrate_cli.unknown_type_aborts_without_output
    COMMAND ${CMAKE_COMMAND}
        -DMIGRATE_BIN=$<TARGET_FILE:piper_migrate_cli>
        -DINPUT=${MIGRATE_DATA}/v1_unknown_type.json
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${MIGRATE_CLI_SCRIPTS}/unknown_type.cmake
)

add_test(NAME migrate_cli.dry_run_writes_nothing
    COMMAND ${CMAKE_COMMAND}
        -DMIGRATE_BIN=$<TARGET_FILE:piper_migrate_cli>
        -DINPUT=${MIGRATE_DATA}/v1_good.json
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${MIGRATE_CLI_SCRIPTS}/dry_run.cmake
)

add_test(NAME migrate_cli.already_migrated_refused_input_intact
    COMMAND ${CMAKE_COMMAND}
        -DMIGRATE_BIN=$<TARGET_FILE:piper_migrate_cli>
        -DINPUT=${MIGRATE_DATA}/already_v3.piper
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${MIGRATE_CLI_SCRIPTS}/already_migrated.cmake
)

add_test(NAME migrate_cli.default_output_self_refused_input_intact
    COMMAND ${CMAKE_COMMAND}
        -DMIGRATE_BIN=$<TARGET_FILE:piper_migrate_cli>
        -DINPUT=${MIGRATE_DATA}/v1_good.json
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${MIGRATE_CLI_SCRIPTS}/default_output_self.cmake
)

set_tests_properties(
    migrate_cli.unknown_type_aborts_without_output
    migrate_cli.dry_run_writes_nothing
    migrate_cli.already_migrated_refused_input_intact
    migrate_cli.default_output_self_refused_input_intact
    PROPERTIES TIMEOUT 60
)
