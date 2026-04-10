if(NOT DEFINED MC OR NOT DEFINED MODE OR NOT DEFINED SOURCE OR NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "MC, MODE, SOURCE, and BUILD_DIR must be defined")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E rm -rf "${BUILD_DIR}"
                RESULT_VARIABLE clean_result)
if(NOT clean_result EQUAL 0)
  message(FATAL_ERROR "failed to remove build dir: ${BUILD_DIR}")
endif()

set(command_args
  "${MC}"
  "${MODE}"
  "${SOURCE}"
  --build-dir
  "${BUILD_DIR}")

if(DEFINED IMPORT_ROOT)
  list(APPEND command_args --import-root "${IMPORT_ROOT}")
endif()

if(DEFINED EXTRA_ARGS)
  separate_arguments(extra_args NATIVE_COMMAND "${EXTRA_ARGS}")
  list(APPEND command_args ${extra_args})
endif()

execute_process(
  COMMAND ${command_args}
  RESULT_VARIABLE mc_result
  OUTPUT_VARIABLE mc_stdout
  ERROR_VARIABLE mc_stderr)

if(NOT mc_result EQUAL 0)
  message(STATUS "stdout:\n${mc_stdout}")
  message(STATUS "stderr:\n${mc_stderr}")
  message(FATAL_ERROR "mc ${MODE} failed for ${SOURCE}")
endif()