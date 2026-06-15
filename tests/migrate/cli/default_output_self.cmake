# A valid V1 file named *.piper with no -o: the default output
# (input with .piper extension) resolves to the input path itself.
# Expect non-zero exit and the input left intact. The fixture is
# copied to the binary dir so the source tree is never at risk.
# Args: MIGRATE_BIN, INPUT, WORK_DIR
set(work_input "${WORK_DIR}/self.piper")
file(REMOVE "${work_input}")
configure_file("${INPUT}" "${work_input}" COPYONLY)

execute_process(
    COMMAND "${MIGRATE_BIN}" "${work_input}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out_text
    ERROR_VARIABLE  err_text
)
if(rc EQUAL 0)
    message(FATAL_ERROR "expected non-zero exit when default output is the input path")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files "${INPUT}" "${work_input}"
    RESULT_VARIABLE same
)
if(NOT same EQUAL 0)
    message(FATAL_ERROR "input file was overwritten")
endif()
