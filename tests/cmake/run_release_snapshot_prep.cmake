if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "SOURCE_ROOT and BINARY_DIR must be defined")
endif()

set(HELPER "${SOURCE_ROOT}/tools/release/prepare_v0_2_snapshot.sh")
set(REPORT_PATH "${BINARY_DIR}/release/v0.2-snapshot.txt")

execute_process(
  COMMAND "${HELPER}" --source-root "${SOURCE_ROOT}" --build-dir "${BINARY_DIR}" --allow-dirty
  RESULT_VARIABLE helper_result
  OUTPUT_VARIABLE helper_stdout
  ERROR_VARIABLE helper_stderr)

if(NOT helper_result EQUAL 0)
  message(STATUS "stdout:\n${helper_stdout}")
  message(STATUS "stderr:\n${helper_stderr}")
  message(FATAL_ERROR "release snapshot preparation helper failed")
endif()

function(require_contains label content needle)
  string(FIND "${content}" "${needle}" found_index)
  if(found_index EQUAL -1)
    message(FATAL_ERROR "${label} is missing required text: ${needle}")
  endif()
endfunction()

if(NOT EXISTS "${REPORT_PATH}")
  message(FATAL_ERROR "release snapshot report was not created: ${REPORT_PATH}")
endif()

file(READ "${REPORT_PATH}" REPORT_CONTENT)
file(READ "${SOURCE_ROOT}/README.md" README_CONTENT)
file(READ "${SOURCE_ROOT}/docs/plan/release_hardening_hosted_slice.txt" RELEASE_NOTE_CONTENT)
file(READ "${SOURCE_ROOT}/docs/plan/phase80_release_snapshot_preparation.txt" PHASE80_CONTENT)

require_contains("helper stdout" "${helper_stdout}" "Release snapshot candidate commit:")
require_contains("helper stdout" "${helper_stdout}" "git -C")
require_contains("snapshot report" "${REPORT_CONTENT}" "git -C \"")
require_contains("snapshot report" "${REPORT_CONTENT}" "tag -a v0.2")
require_contains("README" "${README_CONTENT}" "make release-snapshot-prep")
require_contains("README" "${README_CONTENT}" "build/debug/release/v0.2-snapshot.txt")
require_contains("release_hardening_hosted_slice.txt" "${RELEASE_NOTE_CONTENT}" "make release-snapshot-prep")
require_contains("release_hardening_hosted_slice.txt" "${RELEASE_NOTE_CONTENT}" "build/debug/release/v0.2-snapshot.txt")
require_contains("phase80_release_snapshot_preparation.txt" "${PHASE80_CONTENT}" "make release-snapshot-prep")
require_contains("phase80_release_snapshot_preparation.txt" "${PHASE80_CONTENT}" "prepare_v0_2_snapshot.sh")
