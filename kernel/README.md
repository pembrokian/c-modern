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
- Ring-buffer observability lives in [`kernel/src/serial_shell_event_log.mc`](src/serial_shell_event_log.mc).
- The dispatch entry is [`kernel/src/boot.mc`](src/boot.mc): `kernel_init()` + `kernel_dispatch_step()`.
- The integration loop owner is [`kernel/src/scenarios.mc`](src/scenarios.mc): one scripted observation loop over the serial path.
- The kernel image entry is [`kernel/src/main.mc`](src/main.mc): thin init-plus-loop entry owned by the active reset-lane tree rather than the retired proof-shaped predecessor.
- The repo-owned project manifest is [`kernel/build.toml`](build.toml): direct workflow builds can target the active reset-lane tree without copying a fixture-local manifest.
- The lane is wired into the build through the repo-project entry plus the table-driven reset-lane workflow suite in `mc_tool_workflow_kernel_reset_lane_unit`.

Module admission policy

The following modules are deferred until the named phase boundary forces them:

- `uart` — Phase 155 (temporal/backpressure); lean version only: staged payload + completion + one failure field, no `last_*` observation state
- `timer` — Phase 155; only if temporal dispatch semantics are under audit
- `sched` (ready-queue slice) — when lifecycle is needed; `ReadyQueue` + `TaskSlot` without `LifecycleAudit`
- `capability` — Phase 154 (identity/addressing); rights model + handle table ~80 lines, no validation-observation coupling
- `lifecycle/init` — Phase 153 or when Phase 149 restart needs spawn/exit; one struct + two handlers, not a `LifecycleAudit`
- `mmu` — probably never in this tree; hosted execution makes MMU stubs meaningless

Next target

- Phase 218 (landed): narrow file-service shape probe. `file_service.mc` admitted
  as the ninth boot-wired service (FILE_ENDPOINT_ID = 18, pid = 9). One compact
  CMD_F protocol family (create, write, read, count) routes through shell dispatch.
  Service is retained (Reload on restart); 1-byte-per-slot capacity is a probe
  constraint, not a permanent design. `run_file_service_probe()` in
  `scenario_218.mc` exercises the full create/write/read/count/error path.
  SERVICE_COUNT is now 9.

- Next open: decide whether to widen the file-service data model (requires 2D
  array support or a redesigned state record), or advance another service-system
  boundary identified in Phases 213–217.
