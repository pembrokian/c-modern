# Repository Map For Agents

This file is a fast orientation map for agents working in this repository.

## Top Level

- `CMakeLists.txt`: canonical build graph and CTest registration
- `Makefile`: convenience wrapper around the CMake workflow
- `README.md`: current repository summary and common commands
- `stdlib/`: standard library modules; `stdlib/mem.mc` now owns the admitted fixed-size `mem.Pool` byte-slot contract from Phase 262, the explicit `Arena` init/reset/new/deinit scratch-work contract landed in Phase 263, and the page-rounded raw-byte `mem.Run` contract landed in Phase 264; Phase 265 then explicitly keeps slab-style small-object reuse deferred because the maintained consumers still fit pool, arena, and run ownership honestly, and Phase 266 closes Band B with explicit selection guidance for when to choose buffers, pools, arenas, and runs
- `kernel/`: repository-owned Veya kernel bring-up tree; currently through the completed Phase 282 surface-composition first slice over the hosted reset-lane workflow surface, with the Phase 255 through Phase 260 Band A compiler follow-through wave landed on adjacent boot/helper, topology, render-table, and record-construction owners, the Phase 261 audit classifying Band B as the stronger next owner, Phase 267 through Phase 272 closing Band C with explicit runtime program identity, one bounded launcher owner, one fixed installed-program slot, one bounded fresh-versus-resumed-versus-invalidated launch classification, one issue-rollup-only admitted manifest family, and one explicit launcher-owned installed-workflow status query for the first `issue_rollup` demo path, Phase 273 classifying the first UI work as next, Phase 274 then admitting one explicit foreground-input owner for the tiny `I K <byte> !` event family, Phase 275 admitting one explicit fixed display owner, Phase 276 adding one explicit foreground-routing seam that forwards the admitted input family to exactly one launcher-selected foreground app, Phase 277 replacing the old launcher-time display token shortcut with one app-owned present request path that keeps render policy in the `issue_rollup` render modules and surface update semantics in `display_surface.mc`, Phase 278 aligning the hosted and reset-lane `issue_rollup` paths on the same readable four-cell visible contract instead of admitting a second rendering family, Phase 279 then moving the first direct app interaction model into the repository-owned `issue_rollup` app tree so the visible UI proof now runs through app-local text-plus-parse-plus-render logic instead of a kernel-local placeholder state machine, Phase 280 finally spending the launcher-owned restart classification on that same app path through one launch-only `FRSH` or `RSUM` or `INVD` overlay that clears on the first app-owned state change rather than widening launcher, update_store, or display into retained UI-state owners, Phase 281 then closing Band D as one deliberately narrow human-facing lane, and Phase 282 then admitting one adjacent fixed two-region composition owner that spends a two-cell body plus two-cell status strip on the custom-manifest `issue_rollup` path without widening display into layout policy or launcher into UI coordination
- `docs/agent/prompts/plan_spec.txt`: normative spec for the required structure of new per-phase plan documents
- `docs/agent/prompts/change_touch_sets.txt`: short cross-reference matrix for common owner changes and their likely follow-through files
- `docs/agent/prompts/error_triage.txt`: tiny compiler and kernel compile-break triage sheet for first-failure repair
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
  - `src/main.mc`: explicit architecture entry that initializes boot state and delegates immediately to `kernel_entry`
  - `src/kernel_entry.mc`: thin reset-lane entry router that preserves scripted scenarios as the default path and admits one explicit `live` stdin-backed mode without turning `main.mc` into a policy owner
  - `src/live_receive.mc`: bounded hosted live receive owner that polls one real input fd, assembles one four-byte frame at a time, and feeds the existing dispatch path without owning shell decode or app-input policy
  - `src/input_event.mc`: explicit first UI input owner that recognizes the admitted `I K <byte> !` frame, maps it to one bounded key-event vocabulary, and routes it toward the current launcher-owned foreground app or one explicit no-foreground/unsupported outcome without becoming a broker, queue, or text-input framework
  - `src/foreground_input_route.mc`: explicit first foreground-app input routing seam that consumes parsed input facts from `input_event.mc`, asks `launcher_service.mc` for the current foreground app, and forwards the admitted input family to exactly that one app path or one explicit no-foreground/unsupported outcome without becoming a focus stack, registry, or session framework
  - `src/issue_rollup_app.mc`: explicit first app-owned present seam that now acts as a thin kernel-side wrapper over the repository-owned `issue_rollup` interactive app logic, derives visible four-cell tokens from the existing parse and render modules plus the installed manifest bytes, consumes the launcher-owned fresh-versus-resumed-versus-invalidated classification only as a launch-time overlay (`FRSH`, `RSUM`, `INVD`), and on the custom-manifest path spends one fixed two-region composition seam while keeping default-manifest rendering and launch overlay policy app-local
  - `src/services/display_surface.mc`: explicit first UI display owner for one fixed four-cell target with one present contract and one direct query path; it stores visible cells only and does not own app render policy, composition, regions, or multiple surfaces
  - `src/services/surface_composition.mc`: bounded Phase 282 composition owner for one fixed two-cell body region plus one fixed two-cell status region, translating that tiny region record into the existing four-cell `display_surface` contract without becoming a compositor, layout engine, or second surface family
  - `src/identity/service_topology.mc`: repository-owned static service-topology owner for boot-wired endpoint ids, slot records, restart classification, authority classification, and the same-module `ServiceDescriptor` compile-time table that now carries labels plus topology metadata in one bounded descriptor family
  - `src/identity/program_catalog.mc`: repository-owned fixed runtime program-identity owner for launchable descriptors `{ id, label, launch { package, target, kind } }`, bounded to the admitted `issue_rollup` and `review_board` entries and intentionally separate from launcher, workflow, and build-manifest policy
  - `src/services/object_store_service.mc`: bounded durable named-object owner with fixed-capacity create/read/replace semantics, one owner-local workflow-facing update step, one owner-local per-object version byte, owner-local artifact persistence, explicit conflict-aware delegated update checks, and explicit reload-on-restart follow-through
  - `src/services/update_store_service.mc`: bounded durable staged-update owner with one fixed artifact slot, one compact manifest record, one explicit installed-program slot, one minimal persisted launched-program comparison record, one owner-local classification and apply step, one owner-local artifact persistence, and explicit reload-on-restart follow-through; the same owner now also remains the durable source of one admitted three-byte `issue_rollup` manifest family carried in the installed bytes rather than a broader package schema
  - `src/services/launcher_service.mc`: bounded single-foreground launcher owner consuming the fixed `program_catalog` table plus installed-program truth from `update_store_service`, with explicit catalog listing, selection, identify, launch-one-foreground consequences, one bounded fresh-versus-resumed-versus-invalidated launch classification, and one additive installed-workflow status query while keeping real execution policy deferred; the owner also exposes one additive manifest query for the selected and installed `issue_rollup` entry without becoming a general asset, package, or status-registry API
  - `src/sched.mc`: scheduler-owned lifecycle validation for bounded spawn, wait, sleep, and wake follow-through
  - `src/lifecycle.mc`: lifecycle-owned task and process slot mutation for spawn, timer block, wake-to-ready, exit, and waited-child release follow-through
  - `src/debug/`: one logical `debug` module split through `module_sets.debug`, owning the running-system audit progression from the first running kernel slice through the current Phase 233 bounded delegation lease slice
  - `src/log_service.mc`: bounded log-service protocol state, retained in-memory log state, explicit overwrite-on-full policy, acknowledgment payload, retained-log observation records, and handshake observation records
  - `src/kv_service.mc`: bounded key-value-service retained table state, explicit missing-key, unavailable, and overwrite consequences, fixed key-value-write plus restart-pressure log markers, and retained-state observation records
  - `src/echo_service.mc`: bounded echo-service protocol state, request-derived reply payload, and exchange observation records
  - `src/transfer_service.mc`: bounded transfer-service protocol state, copied emit payload, and transfer observation records
  - `src/timer_service.mc`: bounded timer-service state, id-scoped create/cancel/query/expired operations, and explicit active/expired/cancelled classification
  - `src/task_service.mc`: bounded task-service state, id-scoped submit/query/complete/cancel/list operations, and explicit active/done/failed/cancelled classification
  - `src/services/connection_service.mc`: bounded connection-ingress owner with fixed-capacity open/receive/send/close semantics, explicit disconnected/timed-out outcomes, owner-local idle-budget timeout truth, reset-on-restart behavior, and the reserved delegated external request byte family consumed by the workflow owner
  - `src/services/workflow/service.mc`: bounded retained delayed-work owner tying timer expiry to task execution, one fixed-delay durable named-object update, one version-aware delegated durable-object update follow-through, one restart-safe staged-update apply step, one delegated update-apply step gated by installer leases, journal-backed restart continuity, completion-backed terminal outcomes, delegated durable-update follow-through after lease consumption, external-ingress delivery retry while mailbox pressure keeps the request live in `connection_service`, delegated external ticket-use execution on the connection-backed workflow lane, and restart-resumed deferred update reporting on the existing `WORKFLOW_STATE_DELIVERING` contract
  - `src/completion_mailbox_service.mc`: bounded retained completion owner for workflow terminal outcomes plus explicit fetch/ack/take behavior
  - `src/lease_service.mc`: bounded retained temporary delegation owner for one completion-bound workflow lease path, one delegated durable named-object update lease shape bound to existing restart-visible generation truth plus captured object version truth, one delegated external ticket-use lease shape bound to `ticket_service` generation truth, and one installer-apply lease shape bound to the retained update workflow stale boundary
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
  - `backend.cpp`: public executable-backend owner for module assembly, target validation, public entrypoints, and the single visible executable instruction dispatcher
  - `backend_executable_calls.cpp`: executable call lowering owner
  - `backend_executable_numeric.cpp`: executable numeric and compare lowering owner
  - `backend_executable_aggregate.cpp`: executable aggregate, slice, enum, and memory-container lowering owner
  - `backend_internal.h`: internal executable-backend cross-file contract for shared helper surfaces used by the split lowering owners

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
    - `tests/system/kernel_reset_lane_phase238_connection_service/`: focused reset-lane fixture for the bounded connection-ingress owner
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
- Phase 240 extends the Phase 239 connection-backed workflow lane by requiring external request occupancy to remain live until completion delivery succeeds under mailbox pressure.
- Phase 241 adds one delegated external request path by letting the connection-backed workflow lane consume a bounded lease that authorizes one ticket use at execution time.
- Phase 242 adds one product-shaped durable owner, `update_store_service`, so staged artifact bytes and manifest truth stay separate from both `object_store_service` and workflow logic.
- Phase 243 adds one restart-safe update-apply workflow lane so staged update data now produces one explicit durable applied-target consequence through the existing workflow and completion owners.
- Phase 244 adds one delegated installer-authority path so a bounded installer-apply lease now gates one retained update-apply workflow and yields explicit applied, stale, invalid, and already-consumed outcomes.
- Phase 245 adds one composed mailbox-pressure and restart-resumed reporting path so delegated update apply now proves deferred completion delivery without widening the completion or installer owners.
- Phase 254 adds one object-local version truth for delegated named-object mutation, so delegated durable updates now reject direct object changes deterministically at commit time without widening into transactions or a second workflow lane.
- Phase 255 completes the same-module value-shaped user-body generic-helper slice, so MIR now specializes direct `symbol_ref` and `call` targets with concrete procedure metadata for admitted value-shaped helper bodies and the real `ServiceCell<T>` helper pair in `kernel/src/boot/boot_update.mc` executes through the full compiler path without widening into general monomorphization.
- Phase 256 extends that helper family across one honest imported module boundary, so `kernel/src/boot/service_cell_helpers.mc` now owns `ServiceCell<T>` and its helper pair, while multi-module executable targets build one merged object before link so imported direct calls can execute through the existing MIR specialization path without widening into general imported generic-body completion.
- Phase 257 proves one same-module explicit typed descriptor-table family in `kernel/src/identity/service_topology.mc`, so compile-time data can now combine labels, nested records, and enum metadata in one maintained repository-owned descriptor shape without widening into a registry, procedure-valued descriptor fields, or imported transport.
- Phase 258 proves one same-module exact-signature procedure-table consumer in `examples/real/issue_rollup/src/render/rollup_render.mc`, so fixed render selection can now use one explicit `[4]func() i32` table while kernel dispatch, shell routing, and boot topology remain intentionally flat owners.
- Phase 259 now spends one imported app-local procedure table through `examples/real/issue_rollup/src/render/rollup_render_table.mc` and `examples/real/issue_rollup/src/render/rollup_render.mc`, so render selection uses one explicit imported `[4]func() i32` table while kernel dispatch, shell routing, boot topology, and descriptor-plus-procedure composition remain intentionally out of scope.
- Phase 260 admits one bounded omitted-value field shorthand, `field:`, meaning `field: field` inside existing named aggregate field lists and existing `with { ... }` record-update lists; the landing stays parser/sema/MIR-local, keeps MIR on aggregate rebuild, rejects dotted omitted-value paths such as `outer.inner:`, and spends the feature in `kernel/src/boot/service_cell_helpers.mc` plus `kernel/src/boot/boot_update.mc`.
- Phase 261 closes the mixed Band A compiler wave and records that the remaining deferred compiler fronts are broader generic-body execution beyond the landed helper path, imported descriptor transport plus descriptor-and-procedure composition, and broader compile-time or global-constant execution; none currently outranks Band B's app-facing substrate work.
- Phase 262 adds one stdlib-owned fixed-size pool allocator slice through `stdlib/mem.mc` plus `runtime/hosted/mc_hosted_runtime.c`, with the admitted first consumer living in `examples/real/pool_rows/` and the contract intentionally staying at raw byte-slot sizing until an ordinary `sizeof(T)`-style language surface exists.
- Phase 263 keeps `examples/real/arena_expr/` as the first maintained arena-backed scratch-work owner, adds additive `arena_reset` beside the existing init/new/deinit seam in `stdlib/mem.mc` and `runtime/hosted/mc_hosted_runtime.c`, and reuses one explicit arena across multiple line-local parse passes while keeping token text borrowed from the owned file buffer rather than widening into arena-backed owning text helpers or a region framework.
- Phase 264 adds one stdlib-owned page-rounded `mem.Run` owner through `stdlib/mem.mc` plus `runtime/hosted/mc_hosted_runtime.c`, keeps the public surface at explicit granule, capacity, slice, and release operations, and spends the first larger-granularity memory slice in `examples/real/bundle_stage/` so bundle-sized staging no longer hides behind raw buffers.
- Phase 265 explicitly defers slab-style small-object reuse after auditing the maintained Band B consumers: `examples/real/pool_rows/` already fits `mem.Pool`, `examples/real/arena_expr/` remains explicit scratch on `Arena`, and `examples/real/bundle_stage/` remains larger-granularity staging on `mem.Run`, so no new slab owner lands.
- Phase 266 closes Band B with one explicit selection rule set: use `Buffer<T>` for ordinary owning contiguous storage, `mem.Pool` for bounded fixed-size reuse, `Arena` for transient scratch, and `mem.Run` for larger-granularity staging; slab-style caches and allocator-adapter follow-through remain explicit later pressure, not current maintained truth.
- Phase 267 now lands one explicit runtime program catalog under `kernel/src/identity/program_catalog.mc`, with bounded descriptors for `issue_rollup` and `review_board` carrying only `id`, `label`, and launch `{ package, target, kind }` truth.
- Phase 268 adds one bounded `launcher_service` owner plus one shell-facing `P` command family, so the reset-lane kernel can enumerate a fixed catalog, select one program, and record one foreground launch without widening into discovery, sessions, install policy, or a process-management framework.
- Phase 269 makes one explicit installed-program slot live on `update_store_service` and lets launcher identify and gate launch against that durable truth, so `issue_rollup` can now be staged, activated, and named as an installed artifact without widening into package or bundle frameworks.
- Phase 270 adds one launcher-owned app-visible restart classification over the same installed path, so successful launch now reports `fresh`, `resumed`, or `invalidated` by comparing the installed slot against one minimal update-store-owned persisted launch record rather than widening into sessions, snapshots, or launcher-owned durable storage.
- Phase 271 adds one bounded app-manifest follow-through over that same installed path, so `issue_rollup` can now consume one admitted three-byte manifest family through `launcher_service` while durable bytes remain on `update_store_service` and the repository still does not claim a filesystem or general asset API.
- Phase 272 composes those owners into one explicit `issue_rollup` installed-workflow demo, so launcher now exposes one bounded status query over `(installed program, installed version, foreground program, visible state)` without adding a second orchestrator or launcher-owned durable storage.
- Phase 273 closes Band C as a planning audit: the current maintained app-substrate owners remain `program_catalog`, `launcher_service`, and `update_store_service`, while richer app status, second-app install policy, broader execution semantics, and broader asset families remain deferred until a later real consumer proves one of them stronger than Phase 274's first UI input owner.
- Phase 274 admits one explicit `input_event.mc` owner and spends it in the serial path through one tiny `I K <byte> !` key-event vocabulary. `live_receive.mc` still owns hosted stdin polling and frame assembly, `shell_service.mc` still owns shell-command decode, `launcher_service.mc` still owns single-foreground truth, and the new owner returns explicit delivered, no-foreground, or unsupported outcomes without introducing a broker, subscriptions, or device frameworks.
- Phase 275 admits one explicit `display_surface.mc` owner and spends it through one bounded launch-time handoff from `launcher_service` foreground truth to one fixed four-cell visible target. `display_surface.mc` owns the present/query contract, and the original landing kept content on a tiny `program_catalog.mc` token shortcut until a later app-owned present path could replace it without widening launcher or display.
- Phase 277 adds one explicit app-owned present contract for `issue_rollup`, so launch and admitted key-driven updates now reach the visible surface only through `issue_rollup_app.mc` plus the app-local render modules; `display_surface.mc` still owns only surface update semantics, `launcher_service.mc` still owns only foreground truth, and `program_catalog.mc` no longer carries display-token policy.
- Phase 278 keeps that rendering path bounded to the existing readable four-cell visible contract, aligns hosted `issue_rollup` output with the same `EMTY`/`STDY`/`BUSY`/`ATTN` cells, and adds one focused reset-lane fixed-cell fixture without widening display or launcher ownership.
- Phase 276 keeps `input_event.mc` on event-shape ownership, keeps `launcher_service.mc` on single-foreground truth, and adds `foreground_input_route.mc` as one explicit handoff seam that routes the admitted input family to exactly one foreground app or one explicit no-foreground/unsupported outcome while keeping `kernel_dispatch.mc` as a thin caller rather than a focus-policy owner.
- Phase 246 admits same-name local bindings in disjoint sibling branches and admits `_` as a discard target in local binding and assignment positions while keeping `_` unreadable and storage-free; MIR lowering now preserves that local-target truth through scoped visible-local mapping instead of assuming function-wide source-name uniqueness.
- Phase 247 admits trailing commas in the existing comma-delimited expression and aggregate-init list owners, and the repository now carries explicit parser and lowering regressions for chained `a || b || c` short-circuit forms instead of treating that surface as a deferred limitation.
- Phase 248 normalizes module-local compile-time integer array extents through sema and downstream local-annotation reconstruction, so `[CAP + 1]T` and `[5]T` now share one semantic fixed-array type when the extent evaluates to `5`.
- Phase 248 now also normalizes imported qualified constant extents in downstream MIR type reconstruction, so local annotations such as `[helper_extents.WIDTH + 1]T` lower with the same fixed-array identity as their checked extent values.
- Phase 249 admits `[]` only in existing expected-type owners and only for `[0]T` and `Slice<T>`; standalone `[]`, non-zero arrays, and owning `Buffer<T>` targets remain rejected so the phase does not widen into collection inference or hidden allocation policy.
- Phase 251 makes hosted executable top-level `const str` lower honestly through one dedicated string-global backing-data seam in `compiler/codegen_llvm`; the repair is now spent in `kernel/src/services/journal_service.mc`, while broader top-level global-constant families remain explicit follow-on work.
- Phase 253 adds one explicit live stdin-backed ingress path through `kernel/src/live_receive.mc` and `kernel/src/kernel_entry.mc` while preserving `scenarios.run(...)` as the default reset-lane path and keeping shell decode, dispatch, and service ownership unchanged.

## Where To Change Things

- New syntax or grammar change:
  - `compiler/lex`
  - `compiler/parse`
  - `compiler/ast`
  - matching parser fixtures
  - typed empty collection literals now also require the existing sema expected-type hook because parser ownership stops at recognizing the dedicated `[]` surface

- New semantic rule or type/layout rule:
  - `compiler/sema/check.cpp`
  - possibly `compiler/sema/type.cpp`
  - matching sema fixtures

- New lowered control-flow or MIR rule:
  - `compiler/mir/mir.cpp`
  - matching MIR fixtures and MIR unit tests
  - same-module executable specialization for admitted value-shaped generic helper bodies also stays MIR-owned here; keep parser, sema, driver, and backend policy narrow

- New command-line behavior:
  - `compiler/driver/driver.cpp`
  - grouped tool regression drivers under `tests/tool/`
  - possibly `CMakeLists.txt`

## Safe Default Workflow For Agents

1. Read `docs/plan/plan.txt` and confirm the target phase boundary.
2. Check whether the request is about bootstrap behavior or full plan completion.
3. Change the smallest correct layer.
4. Land the controlling owner cleanly before editing predicted transitive consumers.
5. Prefer sequential dependency-ordered edits over broad synchronized patches.
6. Use `docs/agent/prompts/change_touch_sets.txt` and `docs/agent/prompts/error_triage.txt` to stage the next local hop instead of reopening broad exploration after the first compile break.
7. Update the narrowest relevant fixtures after the owner and its direct consumers stabilize.
8. Run the narrowest relevant validation target after each owner-level batch.
9. Run `cmake --build build/debug -j4 && ctest --test-dir build/debug --output-on-failure` when behavior changes, or use `ctest --test-dir build/debug -j4 --output-on-failure` for an explicit parallel full-suite pass.

Repair-loop guidance:

- Prefer the earliest root-cause compile or type error over later fallout.
- Fix the owner and its direct fallout before widening the edit set.
- Large synchronized patches tend to fail in this repository; avoid predictive mass edits.

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
