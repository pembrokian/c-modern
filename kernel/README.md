# Kernel Reset Lane

Purpose

This directory is the repository-owned kernel reset lane established in Phase 152c.

Rules

- Treat `kernel_old/` as the frozen legacy proof-shaped tree.
- New runtime simplification work starts here instead of extending verifier-owned structures in the legacy tree.
- Services in this lane stay small: state, init, handlers, optional debug only.
- Do not copy `record_*`, `observe_*`, `*Observation`, phase-indexed audit structs, or proof-routing helpers into this tree.
- Keep compiler, sema, MIR, backend, ABI, target, runtime, and `hal` surfaces closed unless a narrow blocker forces a local change.
- Add modules only when a specific phase boundary needs them, at the lean behavioral minimum the four-section standard allows.

Current scope (Phase 152e)

- All three canonical service shapes are present: forwarding (`serial_service`, `shell_service`, `serial_shell_path`), append/tail (`log_service`), and key/value (`kv_service`).
- Ring-buffer observability lives in [`kernel/src/serial_shell_event_log.mc`](src/serial_shell_event_log.mc).
- The dispatch entry is [`kernel/src/boot.mc`](src/boot.mc): `kernel_init()` + `kernel_dispatch_step()`.
- The kernel image entry is [`kernel/src/main.mc`](src/main.mc): ~55 lines vs `kernel_old/src/main.mc` at 1,706 lines.
- Six tests cover all paths: three smoke tests and three system tests.
- The lane is wired into the build via the workflow test suite.

Module admission policy

The following modules are deferred until the named phase boundary forces them:

- `uart` — Phase 155 (temporal/backpressure); lean version only: staged payload + completion + one failure field, no `last_*` observation state
- `timer` — Phase 155; only if temporal dispatch semantics are under audit
- `sched` (ready-queue slice) — when lifecycle is needed; `ReadyQueue` + `TaskSlot` without `LifecycleAudit`
- `capability` — Phase 154 (identity/addressing); rights model + handle table ~80 lines, no validation-observation coupling
- `transfer_service` — Phase 154; lean grant/emit dispatch without `last_*` observation fields
- `lifecycle/init` — Phase 153 or when Phase 149 restart needs spawn/exit; one struct + two handlers, not a `LifecycleAudit`
- `mmu` — probably never in this tree; hosted execution makes MMU stubs meaningless

Next target

- Phase 153 composition audit: does `boot.mc`'s flat endpoint-id dispatch or `serial_shell_path.mc`'s explicit forwarding expose any glue pressure that the isolated module tests missed?
