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

add_executable(mc
    compiler/driver/driver.cpp
    compiler/driver/main.cpp
)

target_include_directories(mc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc PRIVATE mc_frontend_stub mc_support)

add_executable(mc_bootstrap_tests
    tests/tool/bootstrap_tests.cpp
)

target_include_directories(mc_bootstrap_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_bootstrap_tests PRIVATE mc_frontend_stub mc_support)

add_executable(mc_parser_fixture_tests
    tests/parser/parser_fixture_tests.cpp
)

target_include_directories(mc_parser_fixture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mc_parser_fixture_tests PRIVATE mc_frontend_stub mc_support)

add_test(NAME mc_bootstrap_unit COMMAND mc_bootstrap_tests)
add_test(NAME mc_parser_fixture_unit COMMAND mc_parser_fixture_tests ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mc_help_smoke COMMAND mc --help)
add_test(NAME mc_check_smoke COMMAND mc check ${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/hello.mc)
add_test(NAME mc_dump_paths_smoke COMMAND mc dump-paths ${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/hello.mc --build-dir ${CMAKE_CURRENT_BINARY_DIR})
