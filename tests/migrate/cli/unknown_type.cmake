# V1 input with an unknown node type: exit 1, no output file written.
# Args: MIGRATE_BIN, INPUT, WORK_DIR
set(out "${WORK_DIR}/unknown_type_out.piper")
file(REMOVE "${out}")

execute_process(
    COMMAND "${MIGRATE_BIN}" "${INPUT}" -o "${out}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out_text
    ERROR_VARIABLE  err_text
)
if(NOT rc EQUAL 1)
    message(FATAL_ERROR "expected exit code 1, got '${rc}'\nstderr: ${err_text}")
endif()
if(EXISTS "${out}")
    message(FATAL_ERROR "output file was written despite critical diagnostics")
endif()
