# Repository Map For Agents

This file is a fast orientation map for agents working in this repository.

## Top Level

- `CMakeLists.txt`: canonical build graph and CTest registration
- `Makefile`: convenience wrapper around the CMake workflow
- `README.md`: current repository summary and common commands
- `kernel/`: repository-owned Canopus kernel bring-up tree; currently a Phase 100 capability-carrying IPC transfer kernel target
- `docs/plan/admin/canopus_repo_layout_and_test_policy.txt`: current repository policy for Canopus source, build, and test placement
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

- `kernel`
  - repository-owned Canopus kernel sources rather than disposable proof-only fixtures
  - `build.toml`: freestanding kernel manifest for the active bring-up slice
  - `src/main.mc`: explicit architecture entry, bounded first-user-entry, bounded endpoint-and-handle-core validation, bounded syscall-byte-IPC validation, and bounded capability-carrying transfer validation
  - `src/state.mc`: kernel descriptor, process-slot, task-slot, ready-queue, and boot-log records
  - `src/address_space.mc`: bounded address-space, mapping, and user-entry-frame records
  - `src/capability.mc`: bounded bootstrap capability slots, per-process handle-table records, and handle-move helpers
  - `src/endpoint.mc`: bounded endpoint-table, queued-message, and attached-handle message records
  - `src/syscall.mc`: bounded syscall gate, byte-plus-capability send-and-receive request, and receive-observation records
  - `src/interrupt.mc`, `src/init.mc`: bounded subsystem skeleton records for later kernel phases

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
  - Canopus-facing execution proofs still stay here for now; do not create a separate `tests/tool/canopus/` subtree yet
  - grouped tool coverage now lives in:
    - `tests/tool/tool_suite_common.cpp`: shared grouped tool helpers
    - `tests/tool/tool_workflow_tests.cpp`: workflow and CLI/project validation driver
    - `tests/tool/tool_workflow_suite.cpp`: workflow and CLI/project grouped implementation
    - `tests/tool/tool_build_state_tests.cpp`: build-state and incremental rebuild driver
    - `tests/tool/tool_build_state_suite.cpp`: build-state grouped implementation
    - `tests/tool/tool_real_project_tests.cpp`: real-project workflow driver
    - `tests/tool/tool_real_project_suite.cpp`: real-project grouped implementation
    - `tests/tool/tool_freestanding_tests.cpp`: freestanding proof driver
    - `tests/tool/freestanding/suite.cpp`: freestanding top-level orchestrator
    - `tests/tool/freestanding/bootstrap/suite.cpp`: freestanding bootstrap and narrow `hal` grouped implementation
    - `tests/tool/freestanding/kernel/suite.cpp`: kernel freestanding orchestrator
    - `tests/tool/freestanding/kernel/phase85_endpoint_queue.cpp`, `phase86_task_lifecycle.cpp`, `phase87_static_data.cpp`, `phase88_build_integration.cpp`, `phase97_user_entry.cpp`, `phase98_endpoint_handle_core.cpp`, `phase100_capability_transfer.cpp`: one kernel proof per file
    - `tests/tool/freestanding/system/suite.cpp`: init, user-space policy, and integrated-system grouped implementation
    - `tests/tool/README.md`: local structure and validation note for the tool test family
  - if freestanding or Canopus coverage grows further, prefer more focused suite filenames under `tests/tool/` before adding a deeper folder split
  - `tests/tool/tool_suite_tests.cpp` and `tests/tool/phase7_tool_tests.cpp` are thin compatibility runners only

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
- Primary build products now belong under `build/debug/bin/...` and `build/debug/lib/...`.
- Repository-owned smoke and regression outputs should prefer semantic build-tree roots such as `build/debug/audit/...`, `build/debug/probes/...`, `build/debug/tmp/...`, `build/debug/tool/...`, and `build/debug/codegen/executable/...`.
- Canopus-specific disposable outputs should stay under those same roots, usually beneath `build/debug/tool/freestanding/...` for repo-owned regressions or `build/debug/probes/canopus/...` for manual experiments.
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
