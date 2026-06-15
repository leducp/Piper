# --dry-run on a valid V1 input: exit 0, no output file written.
# Args: MIGRATE_BIN, INPUT, WORK_DIR
set(out "${WORK_DIR}/dry_run_out.piper")
file(REMOVE "${out}")

execute_process(
    COMMAND "${MIGRATE_BIN}" "${INPUT}" --dry-run -o "${out}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out_text
    ERROR_VARIABLE  err_text
)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "expected exit code 0, got '${rc}'\nstderr: ${err_text}")
endif()
if(EXISTS "${out}")
    message(FATAL_ERROR "--dry-run wrote an output file")
endif()
