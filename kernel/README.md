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

Current scope (Phase 153)

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

Source tree layout (Phase 219)

- `src/` root: `main.mc` (entry), `kernel_dispatch.mc` (routing), `event_codes.mc` (shared shell-event constants)
- `src/boot/`: `boot.mc`, `boot_identity.mc`, `boot_update.mc`, `init.mc`
- `src/identity/`: `service_identity.mc`, `service_state.mc`, `service_topology.mc`, `identity_taxonomy.mc`
- `src/services/`: `echo_service.mc`, `file_service.mc`, `kv_service.mc`, `log_service.mc`, `queue_service.mc`, `task_service.mc`, `ticket_service.mc`, `timer_service.mc`, `transfer_grant.mc`, `transfer_service.mc`
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

- Phase 240 (landed): external ingress completion pressure. External
  connection-backed workflow results now keep request occupancy truth in
  `connection_service` until completion delivery actually succeeds, so
  mailbox-full retry does not falsely free the ingress slot early.
  `workflow_service.mc` still owns bounded retry and terminal-outcome truth,
  `completion_mailbox_service.mc` still owns queue-full `Exhausted` then
  `WouldBlock`, and the phase proves one external backlog-plus-stall lane
  through `scenario_connection_completion_pressure.mc`. The focused reset-lane
  fixture is
  `tests/system/kernel_reset_lane_phase240_external_ingress_completion_pressure`.

- Phase 239 (landed): connection-backed workflow execution. `connection_service.mc`,
  `workflow_service.mc`, and `kernel_dispatch.mc` now admit one explicit
  `connection execute` handoff that keeps connection lifetime truth owner-local
  while routing one bounded ingress request shape into the retained workflow
  substrate. The workflow path delivers explicit executed, cancelled, and
  restart-cancelled outcomes through the existing completion mailbox without
  widening the four-byte shell frame or the workflow journal lane. The focused
  reset-lane fixture is
  `tests/system/kernel_reset_lane_phase239_connection_backed_workflow`.

- Phase 235 (landed): restart-safe named object update workflow. `workflow_service.mc`
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

- Next open: delegated durable-object mutation is now the strongest adjacent
  pressure, but it should stay deferred until one bounded authority follow-
  through can land without widening into sessions, ACLs, or a capability
  framework.
