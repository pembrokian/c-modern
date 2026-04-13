# Repository Map For Agents

This file is a fast orientation map for agents working in this repository.

## Top Level

- `CMakeLists.txt`: canonical build graph and CTest registration
- `Makefile`: convenience wrapper around the CMake workflow
- `README.md`: current repository summary and common commands
- `kernel/`: repository-owned Veya kernel bring-up tree; currently a Phase 150 one-system-rebuilt-cleanly target
- `docs/plan/admin/canopus_repo_layout_and_test_policy.txt`: current repository policy for Veya source, build, and test placement
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
  - repository-owned Veya kernel sources rather than disposable proof-only fixtures
  - `build.toml`: freestanding kernel manifest for the active bring-up slice
  - `src/main.mc`: explicit architecture entry plus thin root orchestration over the landed first-user-entry, endpoint core, syscall IPC, interrupt classification, timer-owned tick delivery, MMU-owned translation-root construction, bounded init-owned multi-service orchestration, one bounded delegated request-reply follow-through, one bounded fixed service-directory publication step, one bounded image-contract hardening step, one bounded target-surface audit, one bounded next-plateau audit, one bounded delegation-chain stress step, one bounded invalidation and rejection audit step, one bounded authority lifetime classification step, one bounded service-death observation step, one bounded partial-failure propagation step, one bounded UART receive-frame ownership-boundary audit step, one bounded device-failure-containment audit step, one bounded optional completion-backed follow-through step, one bounded serial-ingress composed service graph step, one bounded serial shell command-routing step, one bounded long-lived log-service follow-through step, one bounded service-shape consolidation step, one bounded IPC-shape audit under real usage step, one bounded authority-ergonomics-under-reuse step, one bounded restart-contract step, one bounded rebuilt-system clean-second-pass step, and thin root orchestration across the owned scheduler, lifecycle, bootstrap helper, and debug audit modules
  - `src/sched.mc`: scheduler-owned lifecycle validation for bounded spawn, wait, sleep, and wake follow-through
  - `src/lifecycle.mc`: lifecycle-owned task and process slot mutation for spawn, timer block, wake-to-ready, exit, and waited-child release follow-through
  - `src/debug/`: one logical `debug` module split through `module_sets.debug`, owning Phase 108 image/program-cap audit, Phase 109 first-running-kernel-slice audit, Phase 110 ownership-split audit, Phase 111 lifecycle-ownership audit, Phase 112 syscall-boundary audit, Phase 113 interrupt-boundary audit, Phase 114 address-space/MMU audit, Phase 115 timer-ownership audit, Phase 116 MMU activation-barrier audit, Phase 117 init-orchestrated multi-service audit, Phase 118 delegated request-reply audit, Phase 119 namespace-pressure audit, Phase 120 running-system support audit, Phase 121 kernel image-contract hardening audit, Phase 122 target-surface audit, Phase 123 next-plateau audit, Phase 124 delegation-chain stress audit, Phase 125 invalidation and rejection audit, Phase 126 authority lifetime classification audit, Phase 128 service-death observation audit, Phase 129 partial-failure propagation audit, Phase 134 minimal device-service handoff audit, Phase 135 buffer ownership boundary audit, Phase 136 device failure containment audit, Phase 137 optional completion-backed follow-through audit, Phase 140 serial-ingress composed service-graph audit, Phase 141 interactive-service scope-freeze audit, Phase 142 serial shell command-routing audit, Phase 143 long-lived log-service audit, Phase 144 stateful key-value audit, Phase 145 service restart and usage-pressure audit, Phase 146 service-shape consolidation audit, Phase 147 IPC-shape audit under real usage, Phase 148 authority-ergonomics under reuse audit, Phase 149 restart-contract audit, and Phase 150 rebuilt-system audit
  - `src/log_service.mc`: bounded log-service protocol state, retained in-memory log state, explicit overwrite-on-full policy, acknowledgment payload, retained-log observation records, and handshake observation records
  - `src/kv_service.mc`: bounded key-value-service retained table state, explicit missing-key and overwrite consequences, fixed key-value-write log markers, and retained-state observation records
  - `src/kv_service.mc`: bounded key-value-service retained table state, explicit missing-key, unavailable, and overwrite consequences, fixed key-value-write plus restart-pressure log markers, and retained-state observation records
  - `src/echo_service.mc`: bounded echo-service protocol state, request-derived reply payload, and exchange observation records
  - `src/transfer_service.mc`: bounded transfer-service protocol state, copied emit payload, and transfer observation records
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
    - `tests/tool/tool_workflow_tests.cpp`: workflow and CLI/project validation driver
    - `tests/tool/tool_workflow_suite.cpp`: workflow and CLI/project grouped implementation
    - `tests/tool/tool_build_state_tests.cpp`: build-state and incremental rebuild driver
    - `tests/tool/tool_build_state_suite.cpp`: build-state grouped implementation
    - `tests/tool/tool_real_project_tests.cpp`: real-project workflow driver
    - `tests/tool/tool_real_project_suite.cpp`: real-project grouped implementation
    - `tests/tool/tool_freestanding_tests.cpp`: freestanding proof driver
    - `tests/tool/freestanding/suite.cpp`: freestanding top-level orchestrator
    - `tests/tool/freestanding/bootstrap/suite.cpp`: freestanding bootstrap and narrow `hal` grouped implementation
    - `tests/tool/freestanding/kernel/suite.cpp`: kernel freestanding orchestrator for the descriptor-driven runtime surface plus the separate synthetic phases85-88 surface
    - `tests/tool/freestanding/kernel/runtime/`: descriptor-owned kernel runtime proofs, one phase directory per runtime case
    - `tests/tool/freestanding/kernel/docs/`: descriptor-owned kernel documentation audits
    - `tests/tool/freestanding/kernel/artifact_specs/`: descriptor-owned kernel artifact audits
    - `tests/tool/freestanding/kernel/synthetic/suite.cpp`: standalone synthetic kernel proofs for phases 85-88
    - `tests/tool/freestanding/kernel/runtime/legacy_goldens/`: preserved early narrow MIR proof slices kept as regression references alongside the active descriptor-owned runtime surface
    - late ownership-hardening kernel audits now keep checked-in expectations adjacent to their owning surfaces under `tests/tool/freestanding/kernel/runtime/`, `tests/tool/freestanding/kernel/synthetic/`, and `tests/tool/freestanding/kernel/artifact_specs/`; runtime, synthetic, docs, and artifact validation all run through top-level surfaces rather than shard targets
    - `tests/tool/tool_suite_common.cpp`: `ExpectMirFirstMatchProjectionFile` is the shared helper for those projected MIR goldens
    - `tests/tool/freestanding/system/suite.cpp`: init, user-space policy, and integrated-system grouped implementation
    - `tests/tool/README.md`: local structure and validation note for the tool test family
  - if freestanding or Veya coverage grows further, prefer more focused suite filenames under `tests/tool/` before adding a deeper folder split
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
- Direct-source workflows remain single-file, but project mode now supports explicit multi-file ordinary modules through target-owned `module_sets`.
- Configured import roots are supported through the sema API and `mc check --import-root`.
- Struct layout dumps now include deterministic size, alignment, and field offsets.
- Full Phase 3 from `docs/plan/plan.txt` is still not complete.
- Primary build products now belong under `build/debug/bin/...` and `build/debug/lib/...`.
- Repository-owned smoke and regression outputs should prefer semantic build-tree roots such as `build/debug/audit/...`, `build/debug/probes/...`, `build/debug/tmp/...`, `build/debug/tool/...`, and `build/debug/codegen/executable/...`.
- Veya-specific disposable outputs should stay under those same roots, usually beneath `build/debug/tool/freestanding/...` for repo-owned regressions or `build/debug/probes/veya/...` for manual experiments.
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

- `ctest --test-dir build/debug -R mc_tool_workflow_unit --output-on-failure`
- `ctest --test-dir build/debug -R mc_tool_build_state_unit --output-on-failure`
- `ctest --test-dir build/debug -R mc_tool_real_project_unit --output-on-failure`
- `ctest --test-dir build/debug -R 'mc_tool_freestanding_(bootstrap|kernel_(docs|shard[1-9])|system)_unit' -j8 --output-on-failure`
