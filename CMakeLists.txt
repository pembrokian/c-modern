cmake_minimum_required(VERSION 3.24)

project(c_modern VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MC_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(MC_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MC_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${MC_LIBRARY_OUTPUT_DIRECTORY}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${MC_LIBRARY_OUTPUT_DIRECTORY}")

foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${MC_RUNTIME_OUTPUT_DIRECTORY}")
    set("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper}" "${MC_LIBRARY_OUTPUT_DIRECTORY}")
    set("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper}" "${MC_LIBRARY_OUTPUT_DIRECTORY}")
endforeach()

enable_testing()

add_library(mc_support STATIC
    compiler/support/diagnostics.cpp
    compiler/support/dump_paths.cpp
    compiler/support/source_manager.cpp
)

target_include_directories(mc_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(mc_test_support STATIC
    tests/support/fixture_utils.cpp
    tests/support/process_utils.cpp
)

target_include_directories(mc_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(mc_frontend_stub STATIC
    compiler/ast/ast.cpp
    compiler/lex/lexer.cpp
    compiler/parse/parser.cpp
    compiler/parse/parser_decls.cpp
    compiler/parse/parser_expr.cpp
    compiler/parse/parser_statements.cpp
)

target_include_directories(mc_frontend_stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_frontend_stub PUBLIC mc_support)

add_library(mc_sema_bootstrap STATIC
    compiler/sema/check.cpp
    compiler/sema/const_eval.cpp
    compiler/sema/module_resolver.cpp
    compiler/sema/type.cpp
    compiler/sema/type_utils.cpp
)

target_include_directories(mc_sema_bootstrap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_sema_bootstrap PUBLIC mc_frontend_stub mc_support)

add_library(mc_mci STATIC
    compiler/mci/mci.cpp
)

target_include_directories(mc_mci PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_mci PUBLIC mc_sema_bootstrap mc_support)

add_library(mc_mir STATIC
    compiler/mir/mir.cpp
    compiler/mir/mir_dump.cpp
    compiler/mir/mir_lower.cpp
    compiler/mir/mir_text.cpp
    compiler/mir/mir_type_utils.cpp
    compiler/mir/mir_validate_flow.cpp
    compiler/mir/mir_validate.cpp
)

target_include_directories(mc_mir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_mir PUBLIC mc_sema_bootstrap mc_frontend_stub mc_support)

add_library(mc_codegen_llvm STATIC
    compiler/codegen_llvm/backend_dump.cpp
    compiler/codegen_llvm/backend_inspect.cpp
    compiler/codegen_llvm/backend_names.cpp
    compiler/codegen_llvm/backend_types.cpp
    compiler/codegen_llvm/backend.cpp
)

target_include_directories(mc_codegen_llvm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_codegen_llvm PUBLIC mc_mir mc_support)

add_executable(mc
    compiler/driver/builder.cpp
    compiler/driver/project.cpp
    compiler/driver/driver.cpp
    compiler/driver/ops.cpp
    compiler/driver/main.cpp
)

target_include_directories(mc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc PRIVATE mc_codegen_llvm mc_frontend_stub mc_mci mc_mir mc_support)

add_executable(mc_bootstrap_tests
    tests/tool/bootstrap_tests.cpp
)

target_include_directories(mc_bootstrap_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_bootstrap_tests PRIVATE mc_frontend_stub mc_mci mc_mir mc_support)

add_executable(mc_codegen_llvm_tests
    tests/tool/codegen_llvm_tests.cpp
)

target_include_directories(mc_codegen_llvm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_codegen_llvm_tests PRIVATE mc_codegen_llvm mc_mir mc_support)

add_executable(mc_tool_workflow_tests
    tests/tool/tool_workflow_tests.cpp
    tests/tool/tool_suite_common.cpp
    tests/tool/tool_workflow_suite.cpp
)

target_include_directories(mc_tool_workflow_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_tool_workflow_tests PRIVATE mc_support mc_test_support)

add_executable(mc_tool_build_state_tests
    tests/tool/tool_build_state_tests.cpp
    tests/tool/tool_suite_common.cpp
    tests/tool/tool_build_state_suite.cpp
)

target_include_directories(mc_tool_build_state_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_tool_build_state_tests PRIVATE mc_support mc_test_support)

add_executable(mc_tool_real_project_tests
    tests/tool/tool_real_project_tests.cpp
    tests/tool/tool_suite_common.cpp
    tests/tool/tool_real_project_suite.cpp
)

target_include_directories(mc_tool_real_project_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_tool_real_project_tests PRIVATE mc_support mc_test_support)

add_executable(mc_tool_freestanding_tests
    tests/tool/tool_freestanding_tests.cpp
    tests/tool/tool_suite_common.cpp
    tests/tool/freestanding/bootstrap/suite.cpp
    tests/tool/freestanding/kernel/phase85_endpoint_queue.cpp
    tests/tool/freestanding/kernel/phase86_task_lifecycle.cpp
    tests/tool/freestanding/kernel/phase87_static_data.cpp
    tests/tool/freestanding/kernel/phase88_build_integration.cpp
    tests/tool/freestanding/kernel/phase103_init_bootstrap_handoff.cpp
    tests/tool/freestanding/kernel/suite.cpp
    tests/tool/freestanding/system/suite.cpp
    tests/tool/freestanding/suite.cpp
)

target_include_directories(mc_tool_freestanding_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_tool_freestanding_tests PRIVATE mc_support mc_test_support)

add_executable(mc_codegen_fixture_tests
    tests/codegen/codegen_fixture_tests.cpp
)

target_include_directories(mc_codegen_fixture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_codegen_fixture_tests PRIVATE mc_codegen_llvm mc_frontend_stub mc_mir mc_support mc_test_support)

add_executable(mc_codegen_executable_core_tests
    tests/codegen/codegen_executable_core_tests.cpp
)

target_include_directories(mc_codegen_executable_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_codegen_executable_core_tests PRIVATE mc_support mc_test_support)

add_executable(mc_codegen_executable_stdlib_tests
    tests/codegen/codegen_executable_stdlib_tests.cpp
)

target_include_directories(mc_codegen_executable_stdlib_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_codegen_executable_stdlib_tests PRIVATE mc_support mc_test_support)

add_executable(mc_codegen_executable_project_tests
    tests/codegen/codegen_executable_project_tests.cpp
)

target_include_directories(mc_codegen_executable_project_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_codegen_executable_project_tests PRIVATE mc_support mc_test_support)

add_executable(mc_parser_fixture_tests
    tests/parser/parser_fixture_tests.cpp
)

target_include_directories(mc_parser_fixture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_parser_fixture_tests PRIVATE mc_frontend_stub mc_support mc_test_support)

add_executable(mc_mir_tests
    tests/mir/mir_tests.cpp
)

target_include_directories(mc_mir_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_mir_tests PRIVATE mc_mir mc_frontend_stub mc_support)

add_executable(mc_mir_fixture_tests
    tests/mir/mir_fixture_tests.cpp
)

target_include_directories(mc_mir_fixture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_mir_fixture_tests PRIVATE mc_mir mc_frontend_stub mc_support mc_test_support)

add_executable(mc_sema_fixture_tests
    tests/sema/sema_fixture_tests.cpp
)

target_include_directories(mc_sema_fixture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_sema_fixture_tests PRIVATE mc_sema_bootstrap mc_frontend_stub mc_support mc_test_support)

add_test(NAME mc_bootstrap_unit COMMAND mc_bootstrap_tests)
add_test(NAME mc_codegen_llvm_unit COMMAND mc_codegen_llvm_tests)
add_test(NAME mc_tool_workflow_unit COMMAND mc_tool_workflow_tests ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME mc_tool_build_state_unit COMMAND mc_tool_build_state_tests ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME mc_tool_real_project_unit COMMAND mc_tool_real_project_tests ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME mc_tool_freestanding_unit COMMAND mc_tool_freestanding_tests ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME mc_codegen_fixture_unit COMMAND mc_codegen_fixture_tests ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mc_codegen_executable_core_unit COMMAND mc_codegen_executable_core_tests ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME mc_codegen_executable_stdlib_unit COMMAND mc_codegen_executable_stdlib_tests ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME mc_codegen_executable_project_unit COMMAND mc_codegen_executable_project_tests ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME mc_parser_fixture_unit COMMAND mc_parser_fixture_tests ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mc_sema_fixture_unit COMMAND mc_sema_fixture_tests ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mc_mir_unit COMMAND mc_mir_tests)
add_test(NAME mc_mir_fixture_unit COMMAND mc_mir_fixture_tests ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mc_help_smoke COMMAND mc --help)
add_test(NAME mc_check_smoke
     COMMAND ${CMAKE_COMMAND}
         -DMC=$<TARGET_FILE:mc>
         -DMODE=check
         -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/hello.mc
         -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/stage5_check_smoke
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_clean_mc_build_dir.cmake)
add_test(NAME mc_check_stdlib_import_smoke
     COMMAND ${CMAKE_COMMAND}
         -DMC=$<TARGET_FILE:mc>
         -DMODE=check
         -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/hello_stdout.mc
         -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/stage5_stdlib_import_smoke
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_clean_mc_build_dir.cmake)
add_test(NAME mc_build_smoke
     COMMAND ${CMAKE_COMMAND}
         -DMC=$<TARGET_FILE:mc>
         -DMODE=build
         -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/codegen/smoke_return_zero.mc
         -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/stage5_smoke
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_clean_mc_build_dir.cmake)
add_test(NAME mc_check_import_root_smoke
     COMMAND ${CMAKE_COMMAND}
         -DMC=$<TARGET_FILE:mc>
         -DMODE=check
         -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/sema/import_root_ok_main.mc
         -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/stage5_import_root_smoke
         -DIMPORT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/tests/sema/import_roots
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_clean_mc_build_dir.cmake)
add_test(NAME mc_check_dump_mir_smoke COMMAND mc check ${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/hello.mc --dump-mir --build-dir ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME mc_dump_paths_smoke COMMAND mc dump-paths ${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/hello.mc --build-dir ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME mc_first_use_smoke
     COMMAND ${CMAKE_COMMAND}
         -DMC=$<TARGET_FILE:mc>
         -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
         -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_first_use_smoke.cmake)
add_test(NAME mc_supported_slice_doc_audit
     COMMAND ${CMAKE_COMMAND}
         -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_supported_slice_doc_audit.cmake)
add_test(NAME mc_freestanding_support_audit
     COMMAND ${CMAKE_COMMAND}
         -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_freestanding_support_audit.cmake)
add_test(NAME mc_public_cut_smoke
     COMMAND ${CMAKE_COMMAND}
         -DMC=$<TARGET_FILE:mc>
         -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
         -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_public_cut_smoke.cmake)
add_test(NAME mc_release_readiness_audit
     COMMAND ${CMAKE_COMMAND}
         -DMC=$<TARGET_FILE:mc>
         -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
         -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_release_readiness_audit.cmake)
add_test(NAME mc_v0_2_gate
     COMMAND ${CMAKE_COMMAND}
         -DMC=$<TARGET_FILE:mc>
         -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
         -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_v0_2_gate.cmake)
add_test(NAME mc_release_snapshot_prep
     COMMAND ${CMAKE_COMMAND}
         -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
         -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_release_snapshot_prep.cmake)
