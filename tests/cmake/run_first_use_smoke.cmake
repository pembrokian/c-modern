if(NOT DEFINED MC OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "MC, SOURCE_ROOT, and BINARY_DIR must be defined")
endif()

set(FIRST_USE_ROOT "${BINARY_DIR}/audit/first_use_smoke")
set(HELLO_BUILD_DIR "${FIRST_USE_ROOT}/hello_check")
set(ISSUE_ROLLUP_BUILD_DIR "${FIRST_USE_ROOT}/issue_rollup")
set(ISSUE_ROLLUP_PROJECT "${SOURCE_ROOT}/examples/real/issue_rollup/build.toml")
set(ISSUE_ROLLUP_SAMPLE "${SOURCE_ROOT}/examples/real/issue_rollup/tests/sample.txt")
set(HELLO_SOURCE "${SOURCE_ROOT}/tests/cases/hello.mc")

execute_process(COMMAND "${CMAKE_COMMAND}" -E rm -rf "${FIRST_USE_ROOT}"
                RESULT_VARIABLE clean_result)
if(NOT clean_result EQUAL 0)
  message(FATAL_ERROR "failed to remove first-use smoke build dir: ${FIRST_USE_ROOT}")
endif()

execute_process(
  COMMAND "${MC}" --help
  RESULT_VARIABLE help_result
  OUTPUT_VARIABLE help_stdout
  ERROR_VARIABLE help_stderr)
if(NOT help_result EQUAL 0)
  message(STATUS "stdout:\n${help_stdout}")
  message(STATUS "stderr:\n${help_stderr}")
  message(FATAL_ERROR "mc --help failed during first-use smoke")
endif()
string(FIND "${help_stdout}" "Usage:" help_usage_index)
if(help_usage_index EQUAL -1)
  message(FATAL_ERROR "mc --help output did not contain the usage banner")
endif()

execute_process(
  COMMAND "${MC}" check "${HELLO_SOURCE}" --build-dir "${HELLO_BUILD_DIR}"
  RESULT_VARIABLE check_result
  OUTPUT_VARIABLE check_stdout
  ERROR_VARIABLE check_stderr)
if(NOT check_result EQUAL 0)
  message(STATUS "stdout:\n${check_stdout}")
  message(STATUS "stderr:\n${check_stderr}")
  message(FATAL_ERROR "mc check failed during first-use smoke")
endif()

execute_process(
  COMMAND "${MC}" run --project "${ISSUE_ROLLUP_PROJECT}" --build-dir "${ISSUE_ROLLUP_BUILD_DIR}" -- "${ISSUE_ROLLUP_SAMPLE}"
  RESULT_VARIABLE run_result
  OUTPUT_VARIABLE run_stdout
  ERROR_VARIABLE run_stderr)
if(NOT run_result EQUAL 0)
  message(STATUS "stdout:\n${run_stdout}")
  message(STATUS "stderr:\n${run_stderr}")
  message(FATAL_ERROR "mc run issue_rollup failed during first-use smoke")
endif()
string(FIND "${run_stdout}" "STDY" run_output_index)
if(run_output_index EQUAL -1)
  message(STATUS "stdout:\n${run_stdout}")
  message(STATUS "stderr:\n${run_stderr}")
  message(FATAL_ERROR "issue_rollup run output did not contain the expected steady result")
endif()

execute_process(
  COMMAND "${MC}" test --project "${ISSUE_ROLLUP_PROJECT}" --build-dir "${ISSUE_ROLLUP_BUILD_DIR}"
  RESULT_VARIABLE test_result
  OUTPUT_VARIABLE test_stdout
  ERROR_VARIABLE test_stderr)
if(NOT test_result EQUAL 0)
  message(STATUS "stdout:\n${test_stdout}")
  message(STATUS "stderr:\n${test_stderr}")
  message(FATAL_ERROR "mc test issue_rollup failed during first-use smoke")
endif()
string(FIND "${test_stdout}" "PASS target issue-rollup" test_output_index)
if(test_output_index EQUAL -1)
  message(STATUS "stdout:\n${test_stdout}")
  message(STATUS "stderr:\n${test_stderr}")
  message(FATAL_ERROR "issue_rollup test output did not report a passing target verdict")
endif()