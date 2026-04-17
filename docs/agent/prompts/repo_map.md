# Repository Map For Agents

This file is a fast orientation map for agents working in this repository.

## Top Level

- `CMakeLists.txt`: canonical build graph and CTest registration
- `Makefile`: convenience wrapper around the CMake workflow
- `README.md`: current repository summary and common commands
- `kernel/`: repository-owned Veya kernel bring-up tree; currently at the Phase 233 bounded delegation lease slice over the hosted reset-lane workflow surface
- `docs/agent/prompts/plan_spec.txt`: normative spec for the required structure of new per-phase plan documents
- `docs/plan/admin/canopus_repo_layout_and_test_policy.txt`: current repository policy for Veya source, build, and test placement
- `docs/plan/plan.txt`: authoritative multi-phase implementation plan
- `docs/plan/backlog.txt`: implementation backlog and recurring follow-up themes
- `docs/plan/decision_log.txt`: record of intentional limited solutions and deferred fuller fixes
- `docs/plan/phase3_bootstrap_finish.txt`: repository-specific bootstrap Phase 3 milestone note
- `docs/arch/veya/kernel_style_guide.txt`: current kernel service style, naming, module-shape, and main.mc constraints
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
  - repository-owned Veya kernel sources rather than disposable proof-only fixtures
  - `docs/arch/veya/kernel_style_guide.txt`: required style guide for kernel/src/ service modules and kernel main.mc orchestration
  - `build.toml`: hosted reset-lane manifest used by the active workflow suite and the maintained reset-lane tool regressions
  - `src/main.mc`: explicit architecture entry plus thin root orchestration over the landed bounded service graph, including the retained workflow owner, retained completion mailbox owner, and bounded delegation lease owner
  - `src/sched.mc`: scheduler-owned lifecycle validation for bounded spawn, wait, sleep, and wake follow-through
  - `src/lifecycle.mc`: lifecycle-owned task and process slot mutation for spawn, timer block, wake-to-ready, exit, and waited-child release follow-through
  - `src/debug/`: one logical `debug` module split through `module_sets.debug`, owning the running-system audit progression from the first running kernel slice through the current Phase 233 bounded delegation lease slice
  - `src/log_service.mc`: bounded log-service protocol state, retained in-memory log state, explicit overwrite-on-full policy, acknowledgment payload, retained-log observation records, and handshake observation records
  - `src/kv_service.mc`: bounded key-value-service retained table state, explicit missing-key, unavailable, and overwrite consequences, fixed key-value-write plus restart-pressure log markers, and retained-state observation records
  - `src/echo_service.mc`: bounded echo-service protocol state, request-derived reply payload, and exchange observation records
  - `src/transfer_service.mc`: bounded transfer-service protocol state, copied emit payload, and transfer observation records
  - `src/timer_service.mc`: bounded timer-service state, id-scoped create/cancel/query/expired operations, and explicit active/expired/cancelled classification
  - `src/task_service.mc`: bounded task-service state, id-scoped submit/query/complete/cancel/list operations, and explicit active/done/failed/cancelled classification
  - `src/workflow_service.mc`: bounded retained delayed-work owner tying timer expiry to task execution and restart-visible workflow state
  - `src/completion_mailbox_service.mc`: bounded retained completion owner for workflow terminal outcomes plus explicit fetch/ack/take behavior
  - `src/lease_service.mc`: bounded retained temporary delegation owner for one completion-bound workflow lease path
  - `src/serial_service.mc`: bounded serial-service ingress state plus one service-owned copied receive-frame log, one fixed forwarded composition request observation, one aggregate-reply observation, and one service-local malformed-input classification path
  - `src/state.mc`: kernel descriptor, process-slot, task-slot, ready-queue, and boot-log records
  - `src/address_space.mc`: bounded address-space, mapping, user-entry-frame, and child-bootstrap construction records with translation-root ownership delegated to `mmu`
  - `src/mmu.mc`: bounded translation-root construction, activation, and barrier-backed publish records for the landed first-user and spawn path
  - `src/timer.mc`: bounded timer state, sleep records, wake observations, and interrupt-tick delivery helpers for the landed timer-backed wake path
  - `src/capability/`: one logical `capability` module split through `module_sets.capability`, owning bounded bootstrap capability slots, per-process handle-table records, explicit wait-handle records, and handle-move helpers
  - `src/ipc/`: one logical `ipc` module split through `module_sets.ipc`, owning bounded endpoint-table, queued-message, attached-handle message, runtime queue helpers, close-or-death cleanup, and runtime publish observations
  - `src/syscall.mc`: bounded syscall gate, byte-plus-capability send-and-receive request, bounded spawn-and-wait request, and thin observation records over the landed owner modules
  - `src/interrupt.mc`: bounded interrupt controller, architecture-entry records, and generic dispatch classification for the landed timer-backed wake path plus the bounded UART receive and completion-backed paths
  - `src/uart.mc`: bounded UART receive-frame staging, one bounded completion-backed receive buffer path, copied publish, deterministic retirement, and bounded drop owner for the landed Phase 137 path
  - `src/init.mc`: bounded boot-bundled init image descriptor plus explicit init bootstrap-capability handoff records

- `compiler/driver`
  - CLI entrypoint and pipeline orchestration
  - `driver.cpp` is where `mc check` options are parsed and parse -> sema -> MIR -> optional backend dump wiring lives
  - project mode now admits explicit `module_sets.<module>.files = [...]` source sets for ordinary modules; the driver and builder own logical-module graph discovery, artifact naming, and incremental reuse for that slice

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

- `tests/compiler/parser`
  - parser fixtures
  - success cases compare AST dumps
  - failure cases compare diagnostic substrings in `.errors.txt`

- `tests/compiler/sema`
  - semantic fixture coverage
  - success cases compare `DumpModule` output
  - includes import-resolution and layout fixtures

- `tests/compiler/mir`
  - MIR unit tests and MIR fixture dumps

- `tests/compiler/codegen`
  - deterministic backend inspectability fixtures
  - grouped executable coverage now lives in:
    - `tests/compiler/codegen/codegen_executable_tests.cpp`: shared grouped executable implementation
    - `tests/compiler/codegen/codegen_executable_core_tests.cpp`: core executable behavior driver
    - `tests/compiler/codegen/codegen_executable_stdlib_tests.cpp`: stdlib and canonical executable behavior driver
    - `tests/compiler/codegen/codegen_executable_project_tests.cpp`: project and imported-module executable behavior driver

- `tests/tool`
  - smoke and support-layer tests for the driver/tooling path
  - Veya-facing execution proofs still stay here for now; do not create a separate `tests/tool/veya/` subtree yet
  - grouped tool coverage now lives in:
    - `tests/tool/tool_suite_common.cpp`: shared grouped tool helpers
    - `tests/tool/tool_workflow_tests.cpp`: workflow validation driver
    - `tests/tool/tool_workflow_orchestrator.cpp`: thin workflow suite entry that dispatches the behavior-owned workflow families
    - `tests/tool/workflow/<case-name>/case.toml`: checked-in workflow case descriptors; CMake registration, grouped-runner dispatch, and `tools/select_tests.py` now derive grouped case identity from these descriptors
    - `tests/tool/tool_help_suite.cpp`: help text, mode selection, and direct-source versus project workflow validation
    - `tests/tool/tool_test_command_suite.cpp`: ordinary test and `mc test` workflow validation
    - `tests/tool/tool_project_validation_suite.cpp`: target selection, import-root, duplicate-root, and project-graph validation
    - `tests/tool/tool_multifile_module_suite.cpp`: module-set and multi-file module validation
    - `tests/tool/tool_kernel_reset_lane_suite.cpp`: reset-lane kernel workflow validation and table-driven fixture runs
    - `mc_tool_workflow_kernel_reset_lane_full_unit`: one maintained full reset-lane grouped case; performance work here should prefer per-scenario build reuse, not manual CTest shards
    - `tests/tool/tool_build_state_tests.cpp`: build-state and incremental rebuild driver
    - `tests/tool/tool_build_state_suite.cpp`: build-state grouped implementation
    - `tests/tool/tool_real_project_tests.cpp`: real-project workflow driver
    - `tests/tool/tool_real_project_suite.cpp`: thin real-project grouped dispatcher and generated case-table owner
    - `tests/tool/tool_real_project_suite_internal.h`: exported real-project case declarations shared by the grouped dispatcher and case-local implementation files
    - `tests/tool/real_projects/<case-name>/case.toml`: checked-in real-project descriptors; CMake registration, grouped-runner dispatch, and `tools/select_tests.py` now derive grouped case identity from these descriptors
    - `tests/tool/real_projects/<case-name>/test.cpp`: case-local real-project implementation owner beside the descriptor
    - `tests/tool/README.md`: local structure and validation note for the tool test family
    - `docs/arch/c-lang/tools/grouped_tool_test_operator_reference.txt`: short operator-facing maintenance reference for grouped tool and real-project case wiring
    - `docs/plan/active/phase202_legacy_archive_retirement_and_reset_lane_maintenance_refresh.txt`: records the completed retirement of the old freestanding proof archive and the live reset-lane maintenance boundary
  - if freestanding or Veya coverage grows further, prefer more focused suite filenames under `tests/tool/` before adding a deeper folder split
  - `tests/tool/tool_suite_tests.cpp` and `tests/tool/phase7_tool_tests.cpp` are thin compatibility runners only

- `cmake/ToolTestRegistration.cmake`
  - focused grouped tool and real-project CTest registration surface
  - calls the shared descriptor tool instead of keeping parser logic in the root build graph

- `tools/tool_case_manifest.py`
  - repository-owned grouped tool descriptor implementation path
  - validates `case.toml`, emits grouped runner include data, and emits CTest registration fragments

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
- Direct-source workflows remain single-file, but project mode now supports explicit multi-file ordinary modules through target-owned `module_sets`.
- Configured import roots are supported through the sema API and `mc check --import-root`.
- Struct layout dumps now include deterministic size, alignment, and field offsets.
- Full Phase 3 from `docs/plan/plan.txt` is still not complete.
- Primary build products now belong under `build/debug/bin/...` and `build/debug/lib/...`.
- Repository-owned smoke and regression outputs should prefer semantic build-tree roots such as `build/debug/audit/...`, `build/debug/probes/...`, `build/debug/tmp/...`, `build/debug/tool/...`, and `build/debug/codegen/executable/...`.
- Veya-specific disposable outputs should stay under those same roots, usually beneath `build/debug/tool/workflow/...` for repo-owned regressions or `build/debug/probes/veya/...` for manual experiments.
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
5. Run `cmake --build build/debug -j4 && ctest --test-dir build/debug --output-on-failure` when behavior changes, or use `ctest --test-dir build/debug -j4 --output-on-failure` for an explicit parallel full-suite pass.

Focused grouped-suite checks that are often enough for tool or workflow work:

- `ctest --test-dir build/debug -R 'mc_tool_workflow_.*_unit' --output-on-failure`
- `ctest --test-dir build/debug -R '^mc_tool_workflow_kernel_reset_lane_full_unit$' --output-on-failure`
- `ctest --test-dir build/debug -R mc_tool_build_state_unit --output-on-failure`
- `ctest --test-dir build/debug -R 'mc_tool_real_project_.*_unit' --output-on-failure`
- `ctest --test-dir build/debug -R 'mc_tool_(workflow|build_state|real_project)_unit' --output-on-failure`

Performance-sensitive test guidance:

- Prefer `make select-tests` and the owning grouped target before broader suites.
- If a tool or workflow change touches build reuse, compare a first run and an immediate second run to catch reuse regressions.
- The reset-lane full workflow suite reports per-scenario `cache=hit` or `cache=miss` and aggregate build or run timing from `tests/tool/tool_kernel_reset_lane_suite.cpp`; use that summary to diagnose test-time spikes before changing registration shape.
- Keep reset-lane full coverage as one maintained case unless the repository intentionally changes the ownership model; do not reintroduce manual shard-count maintenance as the default answer to slower tests.
