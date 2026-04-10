# Repository Map For Agents

This file is a fast orientation map for agents working in this repository.

## Top Level

- `CMakeLists.txt`: canonical build graph and CTest registration
- `Makefile`: convenience wrapper around the CMake workflow
- `README.md`: current repository summary and common commands
- `kernel/`: repository-owned Canopus kernel bring-up tree; currently a Phase 118 delegated-request-reply kernel target
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
  - `src/main.mc`: explicit architecture entry plus thin root orchestration over the landed first-user-entry, endpoint core, syscall IPC, interrupt classification, timer-owned tick delivery, MMU-owned translation-root construction, bounded init-owned multi-service orchestration, one bounded delegated request-reply follow-through, scheduler, lifecycle, and debug audit owners through the Phase 118 delegated-request-reply boundary
  - `src/sched.mc`: scheduler-owned lifecycle validation for bounded spawn, wait, sleep, and wake follow-through
  - `src/lifecycle.mc`: lifecycle-owned task and process slot mutation for spawn, timer block, wake-to-ready, exit, and waited-child release follow-through
  - `src/debug.mc`: debug-owned Phase 108 image/program-cap audit, Phase 109 first-running-kernel-slice audit, Phase 110 ownership-split audit, Phase 111 lifecycle-ownership audit, Phase 112 syscall-boundary audit, Phase 113 interrupt-boundary audit, Phase 114 address-space/MMU audit, Phase 115 timer-ownership audit, Phase 116 MMU activation-barrier audit, Phase 117 init-orchestrated multi-service audit, and Phase 118 delegated request-reply audit
  - `src/log_service.mc`: bounded log-service protocol state, acknowledgment payload, and handshake observation records
  - `src/echo_service.mc`: bounded echo-service protocol state, request-derived reply payload, and exchange observation records
  - `src/transfer_service.mc`: bounded transfer-service protocol state, copied emit payload, and transfer observation records
  - `src/state.mc`: kernel descriptor, process-slot, task-slot, ready-queue, and boot-log records
  - `src/address_space.mc`: bounded address-space, mapping, user-entry-frame, and child-bootstrap construction records with translation-root ownership delegated to `mmu`
  - `src/mmu.mc`: bounded translation-root construction, activation, and barrier-backed publish records for the landed first-user and spawn path
  - `src/timer.mc`: bounded timer state, sleep records, wake observations, and interrupt-tick delivery helpers for the landed timer-backed wake path
  - `src/capability.mc`: bounded bootstrap capability slots, per-process handle-table records, explicit wait-handle records, and handle-move helpers
  - `src/endpoint.mc`: bounded endpoint-table, queued-message, attached-handle message, and runtime queue helper records
  - `src/syscall.mc`: bounded syscall gate, byte-plus-capability send-and-receive request, bounded spawn-and-wait request, and thin observation records over the landed owner modules
  - `src/interrupt.mc`: bounded interrupt controller, architecture-entry records, and generic dispatch classification for the landed timer-backed wake path
  - `src/init.mc`: bounded boot-bundled init image descriptor plus explicit init bootstrap-capability handoff records

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
    - `tests/tool/freestanding/kernel/phase85_endpoint_queue.cpp`, `phase86_task_lifecycle.cpp`, `phase87_static_data.cpp`, `phase88_build_integration.cpp`, `phase97_user_entry.cpp`, `phase98_endpoint_handle_core.cpp`, `phase103_init_bootstrap_handoff.cpp`, `phase105_real_log_service_handshake.cpp`, `phase106_real_echo_service_request_reply.cpp`, `phase107_real_user_to_user_capability_transfer.cpp`, `phase108_kernel_image_program_cap_audit.cpp`, `phase109_first_running_kernel_slice_audit.cpp`, `phase110_kernel_ownership_split_audit.cpp`, `phase111_scheduler_lifecycle_ownership_clarification.cpp`, `phase112_syscall_boundary_thinness_audit.cpp`, `phase113_interrupt_entry_and_generic_dispatch_boundary.cpp`, `phase114_address_space_and_mmu_ownership_split.cpp`, `phase115_timer_ownership_hardening.cpp`, `phase116_mmu_activation_barrier_follow_through.cpp`, `phase117_init_orchestrated_multi_service_bring_up.cpp`, `phase118_request_reply_and_delegation_follow_through.cpp`: one kernel proof per file
    - late ownership-hardening kernel audits now keep one `.cpp` proof owner plus one adjacent `.mirproj.txt` projected MIR golden; the `.cpp` owns behavior and publication checks while the `.mirproj.txt` owns the expected MIR projection
    - `tests/tool/tool_suite_common.cpp`: `ExpectMirFirstMatchProjectionFile` is the shared helper for those projected MIR goldens
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
