# Feeding an already-migrated v2/v3 .piper file: non-zero exit, no
# output written, original file byte-identical afterwards. The fixture
# is copied to the binary dir so the source tree is never at risk.
# Args: MIGRATE_BIN, INPUT, WORK_DIR
set(work_input "${WORK_DIR}/already_v3_copy.piper")
set(out        "${WORK_DIR}/already_v3_out.piper")
file(REMOVE "${work_input}" "${out}")
configure_file("${INPUT}" "${work_input}" COPYONLY)

execute_process(
    COMMAND "${MIGRATE_BIN}" "${work_input}" -o "${out}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out_text
    ERROR_VARIABLE  err_text
)
if(rc EQUAL 0)
    message(FATAL_ERROR "expected non-zero exit on already-migrated input")
endif()
if(EXISTS "${out}")
    message(FATAL_ERROR "output file was written for already-migrated input")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files "${INPUT}" "${work_input}"
    RESULT_VARIABLE same
)
if(NOT same EQUAL 0)
    message(FATAL_ERROR "input file was modified")
endif()
