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
  - executable coverage will expand here later in Phase 5

- `tests/tool`
  - smoke and support-layer tests for the driver/tooling path

- `tests/cases`
  - small end-to-end example inputs used by smoke tests

## Important Current Facts

- The repo is beyond parser-only bring-up.
- Bootstrap sema is real and runs before MIR.
- MIR lowering consumes the semantic module instead of rebuilding semantic facts ad hoc.
- Configured import roots are supported through the sema API and `mc check --import-root`.
- Struct layout dumps now include deterministic size, alignment, and field offsets.
- Full Phase 3 from `docs/plan/plan.txt` is still not complete.

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
  - tool smoke tests and possibly `CMakeLists.txt`

## Safe Default Workflow For Agents

1. Read `docs/plan/plan.txt` and confirm the target phase boundary.
2. Check whether the request is about bootstrap behavior or full plan completion.
3. Change the smallest correct layer.
4. Update the narrowest relevant fixtures.
5. Run `cmake --build build/debug -j4 && ctest --test-dir build/debug --output-on-failure` when behavior changes.
