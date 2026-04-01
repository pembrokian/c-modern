cmake_minimum_required(VERSION 3.24)

project(c_modern VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(mc_support STATIC
    compiler/support/diagnostics.cpp
    compiler/support/dump_paths.cpp
    compiler/support/source_manager.cpp
)

target_include_directories(mc_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(mc_frontend_stub STATIC
    compiler/ast/ast.cpp
    compiler/lex/lexer.cpp
    compiler/parse/parser.cpp
)

target_include_directories(mc_frontend_stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_frontend_stub PUBLIC mc_support)

add_library(mc_sema_bootstrap STATIC
    compiler/sema/check.cpp
    compiler/sema/type.cpp
)

target_include_directories(mc_sema_bootstrap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_sema_bootstrap PUBLIC mc_frontend_stub mc_support)

add_library(mc_mir STATIC
    compiler/mir/mir.cpp
)

target_include_directories(mc_mir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_mir PUBLIC mc_sema_bootstrap mc_frontend_stub mc_support)

add_library(mc_codegen_llvm STATIC
    compiler/codegen_llvm/backend.cpp
)

target_include_directories(mc_codegen_llvm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_codegen_llvm PUBLIC mc_mir mc_support)

add_executable(mc
    compiler/driver/driver.cpp
    compiler/driver/main.cpp
)

target_include_directories(mc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc PRIVATE mc_codegen_llvm mc_frontend_stub mc_mir mc_support)

add_executable(mc_bootstrap_tests
    tests/tool/bootstrap_tests.cpp
)

target_include_directories(mc_bootstrap_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_bootstrap_tests PRIVATE mc_frontend_stub mc_mir mc_support)

add_executable(mc_codegen_llvm_tests
    tests/tool/codegen_llvm_tests.cpp
)

target_include_directories(mc_codegen_llvm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_codegen_llvm_tests PRIVATE mc_codegen_llvm mc_mir mc_support)

add_executable(mc_codegen_fixture_tests
    tests/codegen/codegen_fixture_tests.cpp
)

target_include_directories(mc_codegen_fixture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_codegen_fixture_tests PRIVATE mc_codegen_llvm mc_frontend_stub mc_mir mc_support)

add_executable(mc_codegen_executable_tests
    tests/codegen/codegen_executable_tests.cpp
)

target_include_directories(mc_codegen_executable_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_codegen_executable_tests PRIVATE mc_support)

add_executable(mc_parser_fixture_tests
    tests/parser/parser_fixture_tests.cpp
)

target_include_directories(mc_parser_fixture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_parser_fixture_tests PRIVATE mc_frontend_stub mc_support)

add_executable(mc_mir_tests
    tests/mir/mir_tests.cpp
)

target_include_directories(mc_mir_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_mir_tests PRIVATE mc_mir mc_frontend_stub mc_support)

add_executable(mc_mir_fixture_tests
    tests/mir/mir_fixture_tests.cpp
)

target_include_directories(mc_mir_fixture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_mir_fixture_tests PRIVATE mc_mir mc_frontend_stub mc_support)

add_executable(mc_sema_fixture_tests
    tests/sema/sema_fixture_tests.cpp
)

target_include_directories(mc_sema_fixture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_sema_fixture_tests PRIVATE mc_sema_bootstrap mc_frontend_stub mc_support)

add_test(NAME mc_bootstrap_unit COMMAND mc_bootstrap_tests)
add_test(NAME mc_codegen_llvm_unit COMMAND mc_codegen_llvm_tests)
add_test(NAME mc_codegen_fixture_unit COMMAND mc_codegen_fixture_tests ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mc_codegen_executable_unit COMMAND mc_codegen_executable_tests ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME mc_parser_fixture_unit COMMAND mc_parser_fixture_tests ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mc_sema_fixture_unit COMMAND mc_sema_fixture_tests ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mc_mir_unit COMMAND mc_mir_tests)
add_test(NAME mc_mir_fixture_unit COMMAND mc_mir_fixture_tests ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mc_help_smoke COMMAND mc --help)
add_test(NAME mc_check_smoke COMMAND mc check ${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/hello.mc)
add_test(NAME mc_check_stdlib_import_smoke COMMAND mc check ${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/hello_stdout.mc)
add_test(NAME mc_build_smoke COMMAND mc build ${CMAKE_CURRENT_SOURCE_DIR}/tests/codegen/smoke_return_zero.mc --build-dir ${CMAKE_CURRENT_BINARY_DIR}/stage5_smoke)
add_test(NAME mc_check_import_root_smoke COMMAND mc check ${CMAKE_CURRENT_SOURCE_DIR}/tests/sema/import_root_ok_main.mc --import-root ${CMAKE_CURRENT_SOURCE_DIR}/tests/sema/import_roots)
add_test(NAME mc_check_dump_mir_smoke COMMAND mc check ${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/hello.mc --dump-mir --build-dir ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME mc_dump_paths_smoke COMMAND mc dump-paths ${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/hello.mc --build-dir ${CMAKE_CURRENT_BINARY_DIR})
