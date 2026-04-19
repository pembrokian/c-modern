# Kernel Reset Lane

Purpose

This directory is the repository-owned kernel reset lane established in Phase 152c.

Rules

- Treat the pre-reset-lane freestanding proof tree as retired historical context; do not recreate it as a live validation lane.
- New runtime simplification work starts here instead of extending verifier-owned structures in the legacy tree.
- Services in this lane stay small: state, init, handlers, optional debug only.
- Do not copy `record_*`, `observe_*`, `*Observation`, phase-indexed audit structs, or proof-routing helpers into this tree.
- Keep compiler, sema, MIR, backend, ABI, target, runtime, and `hal` surfaces closed unless a narrow blocker forces a local change.
- Add modules only when a specific phase boundary needs them, at the lean behavioral minimum the four-section standard allows.

Current scope (through Phase 271)

- All three canonical service shapes are present: forwarding (`serial_service`, `shell_service`, `serial_shell_path`), append/tail (`log_service`), and key/value (`kv_service`).
- The compact shell route now reaches `log_service` and `kv_service` over the real reset-lane path instead of stopping at shell-local echo behavior.
- One bounded kv-write observation now flows into `log_service` through the explicit serial-shell composition seam.
- One bounded transferred-handle path now flows through `transfer_service` over an explicit transfer endpoint instead of widening ordinary named traffic.
- Ring-buffer observability lives in [`kernel/src/shell/serial_shell_event_log.mc`](src/shell/serial_shell_event_log.mc).
- The dispatch entry is [`kernel/src/boot/boot.mc`](src/boot/boot.mc): `kernel_init()` + `kernel_dispatch_step()`.
- The integration loop owner is [`kernel/src/scenarios/scenarios.mc`](src/scenarios/scenarios.mc): one scripted observation loop over the serial path.
- The kernel image entry is [`kernel/src/main.mc`](src/main.mc): thin init-plus-loop entry owned by the active reset-lane tree rather than the retired proof-shaped predecessor.
- The repo-owned project manifest is [`kernel/build.toml`](build.toml): direct workflow builds can target the active reset-lane tree without copying a fixture-local manifest.
- The lane is wired into the build through the repo-project entry plus the table-driven reset-lane workflow suite in `mc_tool_workflow_kernel_reset_lane_unit`.
- The current app-facing slice now includes one bounded launcher-visible manifest family for `issue_rollup`; durable installed bytes remain on `update_store_service`, launcher exposes only the admitted query surface, and the lane still does not claim a filesystem or general asset API.

Source tree layout (Phase 219)

- `src/` root: `main.mc` (entry), `kernel_dispatch.mc` (routing), `event_codes.mc` (shared shell-event constants)
- `src/boot/`: `boot.mc`, `boot_identity.mc`, `boot_update.mc`, `service_cell_helpers.mc`, `init.mc`
- `src/identity/`: `identity_taxonomy.mc`, `program_catalog.mc`, `service_identity.mc`, `service_state.mc`, `service_topology.mc`
- `src/services/`: `completion_mailbox_service.mc`, `connection_service.mc`, `echo_service.mc`, `file_service.mc`, `journal_service.mc`, `kv_service.mc`, `launcher_service.mc`, `lease_service.mc`, `log_service.mc`, `object_store_service.mc`, `queue_service.mc`, `task_service.mc`, `ticket_service.mc`, `timer_service.mc`, `transfer_grant.mc`, `transfer_service.mc`, `update_store_service.mc`, `workflow/service.mc`
- `src/shell/`: `serial_protocol.mc`, `serial_service.mc`, `serial_shell_event_log.mc`, `serial_shell_path.mc`, `shell_service.mc`
- `src/scenarios/`: `scenarios.mc` and all `scenario_*.mc` files
- `src/transport/`: `primitives.mc`, `syscall.mc`, `ipc.mc`, `service_effect.mc`
- `src/debug/`: reserved; empty

Module admission policy

The following modules are deferred until the named phase boundary forces them:

- `uart` — Phase 155 (temporal/backpressure); lean version only: staged payload + completion + one failure field, no `last_*` observation state
- `timer` — Phase 155; only if temporal dispatch semantics are under audit
- `sched` (ready-queue slice) — when lifecycle is needed; `ReadyQueue` + `TaskSlot` without `LifecycleAudit`
- `capability` — Phase 154 (identity/addressing); rights model + handle table ~80 lines, no validation-observation coupling
- `lifecycle/init` — Phase 153 or when Phase 149 restart needs spawn/exit; one struct + two handlers, not a `LifecycleAudit`
- `mmu` — probably never in this tree; hosted execution makes MMU stubs meaningless

Recent service targets

- Phase 271 (landed): bounded app asset and manifest follow-through. The first
  admitted non-code app payload stays on the existing installed-program bytes
  owned by `update_store_service`, while `launcher_service.mc` exposes one
  additive manifest query for the selected and installed `issue_rollup` entry.
  The hosted `issue_rollup` project now keeps render policy explicitly
  manifest-backed through an app-local helper without changing its default
  behavior. The focused reset-lane fixture is
  `tests/system/kernel_reset_lane_phase271_app_manifest`.

- Phase 268 (landed): bounded launcher service first slice. `program_catalog.mc`
  now owns one fixed launch descriptor table for `issue_rollup` and
  `review_board`, while `launcher_service.mc` owns explicit selection,
  foreground, and launch-count truth over that catalog. The launcher remains
  boot-wired, restart-reset, and single-foreground; shell and dispatch stay
  thin routing seams. The focused reset-lane fixture is
  `tests/system/kernel_reset_lane_phase268_launcher`.

- Phase 245 (landed): update recovery and completion reporting pressure.
  Delegated update apply now proves one composed backlog-plus-restart path on
  the existing owners: `workflow/service.mc` keeps the pending update outcome
  on `WORKFLOW_STATE_DELIVERING`, `completion_mailbox_service.mc` still owns
  first-full `Exhausted` then repeated-full `WouldBlock`,
  `update_store_service.mc` keeps durable applied-target truth visible while
  reporting is deferred, and workflow restart resumes delivery honestly
  without widening completion, installer, or rollback policy. The focused
  reset-lane fixture is
  `tests/system/kernel_reset_lane_phase245_update_recovery_completion_pressure`.

- Phase 242 (landed): update manifest and staged artifact store first slice.
  `update_store_service.mc` is now the bounded durable owner for one staged
  artifact plus one compact manifest truth record. The owner supports stage,
  manifest, query, and clear operations, persists owner-local state to
  `mc_update_store_service.bin`, reloads explicitly on restart, and stays
  separate from both `object_store_service` and workflow ownership. The focused
  reset-lane fixture is
  `tests/system/kernel_reset_lane_phase242_update_store`.

- Phase 240 (landed): external ingress completion pressure. External
  connection-backed workflow results now keep request occupancy truth in
  `connection_service` until completion delivery actually succeeds, so
  mailbox-full retry does not falsely free the ingress slot early.
  `workflow/service.mc` still owns bounded retry and terminal-outcome truth,
  `completion_mailbox_service.mc` still owns queue-full `Exhausted` then
  `WouldBlock`, and the phase proves one external backlog-plus-stall lane
  through `scenario_connection_completion_pressure.mc`. The focused reset-lane
  fixture is
  `tests/system/kernel_reset_lane_phase240_external_ingress_completion_pressure`.

- Phase 239 (landed): connection-backed workflow execution. `connection_service.mc`,
  `workflow/service.mc`, and `kernel_dispatch.mc` now admit one explicit
  `connection execute` handoff that keeps connection lifetime truth owner-local
  while routing one bounded ingress request shape into the retained workflow
  substrate. The workflow path delivers explicit executed, cancelled, and
  restart-cancelled outcomes through the existing completion mailbox without
  widening the four-byte shell frame or the workflow journal lane. The focused
  reset-lane fixture is
  `tests/system/kernel_reset_lane_phase239_connection_backed_workflow`.

- Phase 235 (landed): restart-safe named object update workflow. `workflow/service.mc`
  now admits one compact `OW<name><value>` path that waits on the existing
  restart-safe workflow substrate, applies one owner-local update through
  `object_store_service.object_update(...)`, and delivers explicit updated,
  rejected, or restart-cancelled outcomes through the existing completion
  mailbox. The focused reset-lane fixture is
  `tests/system/kernel_reset_lane_phase235_named_object_update_workflow`.

- Phase 218 (landed): narrow file-service shape probe. `file_service.mc` admitted
  as the ninth boot-wired service (FILE_ENDPOINT_ID = 18, pid = 9). One compact
  CMD_F protocol family (create, write, read, count) routes through shell dispatch.
  Service is retained (Reload on restart); the original one-byte-per-slot cap was
  a probe constraint, not a permanent design. `run_file_service_probe()` in
  `scenario_file_service.mc` exercises the full create/write/read/count/error path.
  SERVICE_COUNT is now 9.

- Phase 225 (landed): bounded task completion follow-through. `task_service.mc`
  now exposes a real `DONE` path through `JD<id>!` without widening into process,
  session, or worker-pool policy.

- Phase 226 (landed): bounded file-service payload growth. `file_service.mc`
  now appends one byte at a time up to four retained bytes per file and returns
  the retained prefix on read. This used the already-landed explicit `FileSlot`
  shape and did not require 2D arrays, compiler work, or filesystem-framework
  growth. The focused reset-lane fixture is
  `tests/system/kernel_reset_lane_phase226_file_growth`.

- Phase 220 (landed): bounded timer and task service follow-through. Added
  `timer_service.mc` and `task_service.mc` as the tenth and eleventh
  boot-wired services (TIMER_ENDPOINT_ID = 19, TASK_ENDPOINT_ID = 20;
  SERVICE_COUNT = 11). Added compact shell command families for timer
  create/cancel/query/expired and task submit/query/cancel/list. Timer restart
  policy is retained reload; task restart policy is explicit reset. The
  scenario probe is `run_timer_task_probe()` in `scenario_timer_task_service.mc` and the
  focused reset-lane fixture is
  `tests/system/kernel_reset_lane_phase220_timer_task_service`.

- Next open: only widen beyond the bounded remote update lane if one real
  admitted consumer needs broader package, distribution, install-policy, or
  reporting semantics that cannot stay honest on the current
  `update_store_service` plus `workflow_service` plus
  `completion_mailbox_service` plus `lease_service` split.
