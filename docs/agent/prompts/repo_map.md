# Repository Map For Agents

This file is a fast orientation map for agents working in this repository.

## Top Level

- `CMakeLists.txt`: canonical build graph and CTest registration
- `Makefile`: convenience wrapper around the CMake workflow
- `README.md`: current repository summary and common commands
- `docs/plan/plan.txt`: authoritative multi-phase implementation plan
- `docs/plan/backlog.txt`: implementation backlog and recurring follow-up themes
- `docs/plan/decision_log.txt`: record of intentional limited solutions and deferred fuller fixes
- `docs/plan/phase3_bootstrap_finish.txt`: repository-specific bootstrap Phase 3 milestone note
- `build/`: disposable build output

## Compiler Tree

### Pipeline Order

1. `compiler/lex`
2. `compiler/parse`
3. `compiler/sema`
4. `compiler/mir`
5. `compiler/codegen_llvm`

### Key Directories

- `compiler/driver`
  - CLI entrypoint and pipeline orchestration
  - `driver.cpp` is where `mc check` options are parsed and parse -> sema -> MIR -> optional backend dump wiring lives

- `compiler/ast`
  - AST node definitions and AST dump support

- `compiler/lex`
  - token model and handwritten lexer

- `compiler/parse`
  - handwritten parser for module syntax, declarations, statements, and expressions

- `compiler/sema`
  - bootstrap semantic checking
  - `type.h/.cpp`: semantic type model and formatting
  - `check.h/.cpp`: program checking, import resolution, semantic module construction, first-cut layout computation, semantic dumps

- `compiler/mir`
  - typed MIR data structures, lowering from sema, MIR validation, MIR dumps

- `compiler/codegen_llvm`
  - bootstrap LLVM backend scaffolding
  - frozen hosted Darwin arm64 target configuration and backend preflight entrypoints

- `compiler/support`
  - diagnostics, source-manager utilities, deterministic dump-path helpers

- `compiler/resolve`
  - reserved for future dedicated import/name-resolution infrastructure

- `compiler/mci`
  - reserved for future interface-artifact support

## Tests

- `tests/parser`
  - parser fixtures
  - success cases compare AST dumps
  - failure cases compare diagnostic substrings in `.errors.txt`

- `tests/sema`
  - semantic fixture coverage
  - success cases compare `DumpModule` output
  - includes import-resolution and layout fixtures

- `tests/mir`
  - MIR unit tests and MIR fixture dumps

- `tests/codegen`
  - deterministic backend inspectability fixtures
  - grouped executable coverage now lives in:
    - `tests/codegen/codegen_executable_tests.cpp`: shared grouped executable implementation
    - `tests/codegen/codegen_executable_core_tests.cpp`: core executable behavior driver
    - `tests/codegen/codegen_executable_stdlib_tests.cpp`: stdlib and canonical executable behavior driver
    - `tests/codegen/codegen_executable_project_tests.cpp`: project and imported-module executable behavior driver

- `tests/tool`
  - smoke and support-layer tests for the driver/tooling path
  - grouped tool coverage now lives in:
    - `tests/tool/tool_suite_tests.cpp`: shared grouped tool regression implementation
    - `tests/tool/tool_workflow_tests.cpp`: workflow and CLI/project validation driver
    - `tests/tool/tool_build_state_tests.cpp`: build-state and incremental rebuild driver
    - `tests/tool/tool_real_project_tests.cpp`: real-project workflow driver
    - `tests/tool/tool_freestanding_tests.cpp`: freestanding proof driver
  - `tests/tool/phase7_tool_tests.cpp` is now only a compatibility shim that includes `tool_suite_tests.cpp`

- `tests/support`
  - local test harness support code
  - `fixture_utils.*`: adjacent fixture discovery, normalization, and diagnostic substring helpers
  - `process_utils.*`: subprocess, socket, timeout, and temp-tree helpers for grouped integration suites

- `tests/cases`
  - small end-to-end example inputs used by smoke tests

## Important Current Facts

- The repo is beyond parser-only bring-up.
- Bootstrap sema is real and runs before MIR.
- MIR lowering consumes the semantic module instead of rebuilding semantic facts ad hoc.
- Configured import roots are supported through the sema API and `mc check --import-root`.
- Struct layout dumps now include deterministic size, alignment, and field offsets.
- Full Phase 3 from `docs/plan/plan.txt` is still not complete.
- Repository-owned smoke and regression outputs should prefer semantic build-tree roots such as `build/debug/audit/...`, `build/debug/probes/...`, `build/debug/tmp/...`, `build/debug/tool/...`, and the grouped codegen suite work roots.
- Remaining top-level `build/debug/phase*` paths are preserved manual or probe areas rather than active regression output policy.

## Where To Change Things

- New syntax or grammar change:
  - `compiler/lex`
  - `compiler/parse`
  - `compiler/ast`
  - matching parser fixtures

- New semantic rule or type/layout rule:
  - `compiler/sema/check.cpp`
  - possibly `compiler/sema/type.cpp`
  - matching sema fixtures

- New lowered control-flow or MIR rule:
  - `compiler/mir/mir.cpp`
  - matching MIR fixtures and MIR unit tests

- New command-line behavior:
  - `compiler/driver/driver.cpp`
  - grouped tool regression drivers under `tests/tool/`
  - possibly `CMakeLists.txt`

## Safe Default Workflow For Agents

1. Read `docs/plan/plan.txt` and confirm the target phase boundary.
2. Check whether the request is about bootstrap behavior or full plan completion.
3. Change the smallest correct layer.
4. Update the narrowest relevant fixtures.
5. Run `cmake --build build/debug -j4 && ctest --test-dir build/debug --output-on-failure` when behavior changes.

Focused grouped-suite checks that are often enough for tool or workflow work:

- `ctest --test-dir build/debug -R mc_tool_workflow_unit --output-on-failure`
- `ctest --test-dir build/debug -R mc_tool_build_state_unit --output-on-failure`
- `ctest --test-dir build/debug -R mc_tool_real_project_unit --output-on-failure`
- `ctest --test-dir build/debug -R mc_tool_freestanding_unit --output-on-failure`
