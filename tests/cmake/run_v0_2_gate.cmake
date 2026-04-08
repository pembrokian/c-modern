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
  "v0.2 gate release-readiness audit"
  SCRIPT "${SOURCE_ROOT}/tests/cmake/run_release_readiness_audit.cmake"
  DEFINES
    -DMC=${MC}
    -DSOURCE_ROOT=${SOURCE_ROOT}
    -DBINARY_DIR=${BINARY_DIR})

file(READ "${SOURCE_ROOT}/README.md" README_CONTENT)
file(READ "${SOURCE_ROOT}/docs/plan/release_hardening_hosted_slice.txt" RELEASE_NOTE_CONTENT)
file(READ "${SOURCE_ROOT}/docs/plan/phase79_v0_2_gate.txt" PHASE79_CONTENT)

function(require_contains label content needle)
  string(FIND "${content}" "${needle}" found_index)
  if(found_index EQUAL -1)
    message(FATAL_ERROR "${label} is missing required text: ${needle}")
  endif()
endfunction()

require_contains(
  "README"
  "${README_CONTENT}"
  "the one remaining plateau that still blocks that tag is maintainer-owned release execution")
require_contains(
  "release_hardening_hosted_slice.txt"
  "${RELEASE_NOTE_CONTENT}"
  "the one remaining plateau blocking that tag is maintainer-owned release")
require_contains(
  "phase79_v0_2_gate.txt"
  "${PHASE79_CONTENT}"
  "Chosen outcome: defer the in-repository `v0.2` tag")
require_contains(
  "phase79_v0_2_gate.txt"
  "${PHASE79_CONTENT}"
  "maintainer-owned release execution")