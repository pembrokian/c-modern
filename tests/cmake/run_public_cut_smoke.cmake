if(NOT DEFINED MC OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "MC, SOURCE_ROOT, and BINARY_DIR must be defined")
endif()

include(CMakeParseArguments)

set(PUBLIC_CUT_ROOT "${BINARY_DIR}/audit/public_cut_smoke")
set(HELLO_SOURCE "${SOURCE_ROOT}/tests/cases/hello.mc")
set(SMOKE_BUILD_SOURCE "${SOURCE_ROOT}/tests/codegen/smoke_return_zero.mc")
set(HELLO_BUILD_DIR "${PUBLIC_CUT_ROOT}/hello_check")
set(SMOKE_BUILD_DIR "${PUBLIC_CUT_ROOT}/smoke_return_zero")
set(ISSUE_ROLLUP_PROJECT "${SOURCE_ROOT}/examples/real/issue_rollup/build.toml")
set(ISSUE_ROLLUP_SAMPLE "${SOURCE_ROOT}/examples/real/issue_rollup/tests/sample.txt")
set(ISSUE_ROLLUP_BUILD_DIR "${PUBLIC_CUT_ROOT}/issue_rollup")
set(WORKER_QUEUE_PROJECT "${SOURCE_ROOT}/examples/real/worker_queue/build.toml")
set(WORKER_QUEUE_BUILD_DIR "${PUBLIC_CUT_ROOT}/worker_queue")
set(PIPE_HANDOFF_PROJECT "${SOURCE_ROOT}/examples/real/pipe_handoff/build.toml")
set(PIPE_HANDOFF_BUILD_DIR "${PUBLIC_CUT_ROOT}/pipe_handoff")
set(PIPE_READY_PROJECT "${SOURCE_ROOT}/examples/real/pipe_ready/build.toml")
set(PIPE_READY_BUILD_DIR "${PUBLIC_CUT_ROOT}/pipe_ready")
set(LINE_FILTER_RELAY_PROJECT "${SOURCE_ROOT}/examples/real/line_filter_relay/build.toml")
set(LINE_FILTER_RELAY_BUILD_DIR "${PUBLIC_CUT_ROOT}/line_filter_relay")
set(EVENTED_ECHO_PROJECT "${SOURCE_ROOT}/examples/real/evented_echo/build.toml")
set(EVENTED_ECHO_BUILD_DIR "${PUBLIC_CUT_ROOT}/evented_echo")

function(run_and_require_success label)
  set(options)
  set(oneValueArgs EXPECT_STDOUT)
  set(multiValueArgs COMMAND)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  execute_process(
    COMMAND ${ARG_COMMAND}
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_stdout
    ERROR_VARIABLE command_stderr)

  if(NOT command_result EQUAL 0)
    message(STATUS "stdout:\n${command_stdout}")
    message(STATUS "stderr:\n${command_stderr}")
    message(FATAL_ERROR "${label} failed")
  endif()

  if(DEFINED ARG_EXPECT_STDOUT)
    string(FIND "${command_stdout}" "${ARG_EXPECT_STDOUT}" output_index)
    if(output_index EQUAL -1)
      message(STATUS "stdout:\n${command_stdout}")
      message(STATUS "stderr:\n${command_stderr}")
      message(FATAL_ERROR "${label} did not contain expected output: ${ARG_EXPECT_STDOUT}")
    endif()
  endif()
endfunction()

execute_process(COMMAND "${CMAKE_COMMAND}" -E rm -rf "${PUBLIC_CUT_ROOT}"
                RESULT_VARIABLE clean_result)
if(NOT clean_result EQUAL 0)
  message(FATAL_ERROR "failed to remove public-cut smoke build dir: ${PUBLIC_CUT_ROOT}")
endif()

run_and_require_success(
  "public-cut direct-source check"
  COMMAND "${MC}" check "${HELLO_SOURCE}" --build-dir "${HELLO_BUILD_DIR}")

run_and_require_success(
  "public-cut direct-source build"
  COMMAND "${MC}" build "${SMOKE_BUILD_SOURCE}" --build-dir "${SMOKE_BUILD_DIR}")

run_and_require_success(
  "public-cut issue_rollup default run"
  EXPECT_STDOUT "issue-rollup-steady"
  COMMAND "${MC}" run --project "${ISSUE_ROLLUP_PROJECT}" --build-dir "${ISSUE_ROLLUP_BUILD_DIR}" -- "${ISSUE_ROLLUP_SAMPLE}")

run_and_require_success(
  "public-cut issue_rollup report run"
  EXPECT_STDOUT "issue-rollup-steady"
  COMMAND "${MC}" run --project "${ISSUE_ROLLUP_PROJECT}" --target issue-rollup-report --build-dir "${ISSUE_ROLLUP_BUILD_DIR}" -- "${ISSUE_ROLLUP_SAMPLE}")

run_and_require_success(
  "public-cut issue_rollup test"
  EXPECT_STDOUT "PASS target issue-rollup"
  COMMAND "${MC}" test --project "${ISSUE_ROLLUP_PROJECT}" --build-dir "${ISSUE_ROLLUP_BUILD_DIR}")

run_and_require_success(
  "public-cut worker_queue test"
  EXPECT_STDOUT "PASS target worker-queue"
  COMMAND "${MC}" test --project "${WORKER_QUEUE_PROJECT}" --build-dir "${WORKER_QUEUE_BUILD_DIR}")

run_and_require_success(
  "public-cut pipe_handoff test"
  EXPECT_STDOUT "PASS target pipe-handoff"
  COMMAND "${MC}" test --project "${PIPE_HANDOFF_PROJECT}" --build-dir "${PIPE_HANDOFF_BUILD_DIR}")

run_and_require_success(
  "public-cut pipe_ready test"
  EXPECT_STDOUT "PASS target pipe-ready"
  COMMAND "${MC}" test --project "${PIPE_READY_PROJECT}" --build-dir "${PIPE_READY_BUILD_DIR}")

run_and_require_success(
  "public-cut line_filter_relay test"
  EXPECT_STDOUT "PASS target line-filter-relay"
  COMMAND "${MC}" test --project "${LINE_FILTER_RELAY_PROJECT}" --build-dir "${LINE_FILTER_RELAY_BUILD_DIR}")

run_and_require_success(
  "public-cut evented_echo test"
  EXPECT_STDOUT "PASS target echo"
  COMMAND "${MC}" test --project "${EVENTED_ECHO_PROJECT}" --build-dir "${EVENTED_ECHO_BUILD_DIR}")