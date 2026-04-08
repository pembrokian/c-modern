if(NOT DEFINED MC OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "MC, SOURCE_ROOT, and BINARY_DIR must be defined")
endif()

function(run_phase_script label)
  set(options)
  set(oneValueArgs SCRIPT)
  set(multiValueArgs DEFINES)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  execute_process(
    COMMAND "${CMAKE_COMMAND}" ${ARG_DEFINES} -P "${ARG_SCRIPT}"
    RESULT_VARIABLE script_result
    OUTPUT_VARIABLE script_stdout
    ERROR_VARIABLE script_stderr)

  if(NOT script_result EQUAL 0)
    message(STATUS "stdout:\n${script_stdout}")
    message(STATUS "stderr:\n${script_stderr}")
    message(FATAL_ERROR "${label} failed")
  endif()
endfunction()

run_phase_script(
  "release-readiness first-use smoke"
  SCRIPT "${SOURCE_ROOT}/tests/cmake/run_first_use_smoke.cmake"
  DEFINES
    -DMC=${MC}
    -DSOURCE_ROOT=${SOURCE_ROOT}
    -DBINARY_DIR=${BINARY_DIR})

run_phase_script(
  "release-readiness supported-slice doc audit"
  SCRIPT "${SOURCE_ROOT}/tests/cmake/run_supported_slice_doc_audit.cmake"
  DEFINES
    -DSOURCE_ROOT=${SOURCE_ROOT})

run_phase_script(
  "release-readiness public-cut smoke"
  SCRIPT "${SOURCE_ROOT}/tests/cmake/run_public_cut_smoke.cmake"
  DEFINES
    -DMC=${MC}
    -DSOURCE_ROOT=${SOURCE_ROOT}
    -DBINARY_DIR=${BINARY_DIR})