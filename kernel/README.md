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

Human-facing support boundary

- The current supported user-facing claim for this lane is one bounded small-system path built through [`kernel/build.toml`](build.toml) and evidenced by `mc_tool_workflow_kernel_reset_lane_unit`.
- That claim is limited to one launcher-selected foreground app on one fixed four-cell visible surface, one admitted key-only input family, one installed `issue_rollup` path, one direct-launch `review_board` path with additive detail-selection and key-only filter-edit interaction, and one bounded persisted `review_board` reload path that now preserves the active detail view plus one in-progress filter-edit state.
- This lane does not currently claim a desktop shell, package manager, filesystem API, general app framework, alternate input family, or richer multi-surface UI model.

Current scope (through Phase 301)

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
- The current app-facing slice now includes one bounded launcher-owned installed-workflow status query over the admitted `issue_rollup` path; durable installed bytes and launch-record comparison truth remain on `update_store_service`, launcher exposes only the admitted app-facing policy surfaces, and the lane still does not claim a filesystem, package manager, or general asset API.
- Phase 273 then closes Band C as a planning audit and classifies the first UI input/display owners as the stronger next move rather than another launcher/update/object-store follow-through.
- Phase 274 adds one explicit `input_event.mc` owner for the first `I K <byte> !` foreground-input path.
- Phase 275 adds one explicit `display_surface.mc` owner for one fixed four-cell visible target and one bounded launcher-triggered foreground present handoff.
- Phase 276 adds one explicit `foreground_input_route.mc` seam that keeps event parsing in `input_event.mc`, keeps foreground truth in `launcher_service.mc`, and routes the admitted input family to exactly one current foreground app without widening launcher, display, or dispatch into a UI framework.
- Phase 277 adds one explicit `issue_rollup_app.mc` owner for the first app-driven present path, keeps render classification in the `issue_rollup` render modules, and removes the old launcher-time display-token shortcut so launch/input now reach the surface only through an explicit app-to-display present request.
- Phase 278 keeps the first rendering answer on that same app-to-display path by reusing the existing readable four-cell visible contract (`EMTY`, `STDY`, `BUSY`, `ATTN`) and aligning the hosted `issue_rollup` path to the same cell-shaped output rather than admitting a second rendering family.
- Phase 279 adds one directly usable `issue_rollup` app proof through an app-local interactive reducer that appends bounded issue lines and reuses the existing parse-plus-render owners instead of widening the kernel into a second app framework.
- Phase 280 keeps restart and update truth on the same visible path through one launch-only `FRSH` or `RSUM` or `INVD` overlay that reuses launcher and update-store truth without admitting retained UI state.
- Phase 281 closes Band D as a planning audit: the maintained first-UI truth is now one bounded input plus display plus routing plus present plus fixed-cell rendering plus app-proof lane, while windows, widgets, composition frameworks, alternate input families, and retained UI-state work remain deferred until a stronger later owner appears.
- Phase 283 keeps alternate input deferred after an evidence-based audit: the maintained app path still only needs key events, and one focused reject proof now confirms that non-key `I <event> <value> !` frames stay explicitly unsupported without changing visible app state.
- Phase 284 then keeps active-region truth app-local on the existing custom-manifest path, and Phase 285 explicitly declines to invent a broader shared control vocabulary before a second real consumer exists.
- Phase 286 through Phase 290 add `review_board` as the second directly usable UI proof, give it one bounded journal-backed persisted view-state lane, document the canonical UI workflow surfaces, freeze post-Band-E basic UI, and prove the integrated installed-`issue_rollup` plus direct-launch-`review_board` demo on the maintained reset-lane path.
- Phase 294 then chooses richer UI on the same fixed-surface, launcher-selected substrate as the strongest next wave.
- Phase 295 through Phase 300 spend that richer UI wave without weakening the owner split: `review_board` joins the admitted foreground-input path, gains one additive `Summary` or `Open` or `Closed` or `Urgent` detail-selection lane, shares only the tiny `Body` or `Status` `active_region` truth with `issue_rollup`, gains one bounded key-only filter-edit path with the fixed tokens `AL`, `OP`, `CL`, and `UR`, stays aligned across hosted and reset-lane proofs, and now reloads the current detail view plus one in-progress filter-edit substate through the existing journal lane.
- Phase 301 then freezes Band G as one more expressive but still bounded UI slice: fixed four-cell surface, optional two-region composition seam, key-only input, app-local interaction policy, one tiny shared value owner, no toolkit or broader layout/input framework, and the byte-literal compiler readability slice named as the strongest next blocker rather than broader packaging or another UI framework wave.

Source tree layout (Phase 219)

- `src/` root: `main.mc` (entry), `kernel_dispatch.mc` (routing), `event_codes.mc` (shared shell-event constants)
- `src/boot/`: `boot.mc`, `boot_identity.mc`, `boot_update.mc`, `service_cell_helpers.mc`, `init.mc`
- `src/identity/`: `identity_taxonomy.mc`, `program_catalog.mc`, `service_identity.mc`, `service_state.mc`, `service_topology.mc`
- `src/services/`: `completion_mailbox_service.mc`, `connection_service.mc`, `display_surface.mc`, `echo_service.mc`, `file_service.mc`, `journal_service.mc`, `kv_service.mc`, `launcher_service.mc`, `lease_service.mc`, `log_service.mc`, `object_store_service.mc`, `queue_service.mc`, `task_service.mc`, `ticket_service.mc`, `timer_service.mc`, `transfer_grant.mc`, `transfer_service.mc`, `update_store_service.mc`, `workflow/service.mc`
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

- Phase 283 (completed): pointer or alternate input first slice.
  The repository now has one explicit evidence-based non-landing for a second
  input family. `input_event.mc`, `foreground_input_route.mc`, and
  `issue_rollup_app.mc` remain unchanged as the maintained key-only path, and
  the focused reset-lane fixture is
  `tests/system/kernel_reset_lane_phase283_pointer_or_alternate_input_first_slice`.

- Phase 281 (completed): first UI slice audit.
  Band D is now explicitly closed for its admitted slice: `input_event.mc`,
  `display_surface.mc`, `foreground_input_route.mc`, `issue_rollup_app.mc`,
  the fixed readable four-cell rendering contract, the directly usable
  `issue_rollup` app path, and the launch-only `FRSH`/`RSUM`/`INVD` overlay
  are the maintained first-UI truths. No stronger still-open Band D owner was
  found than the next bounded Band E layer, so windows, widgets, composition,
  alternate input, shared controls, and retained UI-state work stay deferred.

- Phase 280 (completed): UI app restart and update follow-through.
  `issue_rollup_app.mc` now consumes the launcher-owned fresh-versus-resumed-
  versus-invalidated classification only at launch time and presents one
  bounded four-cell overlay before the first app-owned state change returns
  rendering to the existing manifest-plus-summary path. The focused reset-lane
  fixture is
  `tests/system/kernel_reset_lane_phase280_ui_app_restart_and_update_follow_through`.

- Phase 279 (completed): first human-facing app proof.
  `issue_rollup` is now directly usable on the admitted UI path through one
  app-local interactive reducer in
  `examples/real/issue_rollup/src/app/issue_rollup_interactive.mc`, while
  render policy remains on the existing parse-plus-render owners and the
  kernel-side wrapper stays thin. The focused reset-lane fixture is
  `tests/system/kernel_reset_lane_phase279_first_human_facing_app_proof`.

- Phase 278 (landed): fixed-cell rendering path.
  The admitted readable rendering contract is the existing four-byte visible
  cell payload already carried by `display_surface.mc` and `issue_rollup_app.mc`.
  `issue_rollup` render policy stays in the app-local render modules, hosted
  `issue_rollup` output now emits the same readable cell tokens, and the focused
  reset-lane fixture is
  `tests/system/kernel_reset_lane_phase278_fixed_cell_rendering`.

- Phase 277 (landed): explicit repaint/present contract.
  `issue_rollup_app.mc` now owns one bounded app-local summary plus one direct
  `display_surface.display_present(...)` request path, while `display_surface.mc`
  remains the fixed-surface owner and `launcher_service.mc` remains only the
  foreground-policy owner. The admitted `issue_rollup` render policy now stays in
  the app-local render modules and launch/input reach the surface only through
  that explicit present path. The focused reset-lane fixture is
  `tests/system/kernel_reset_lane_phase277_explicit_present_contract`.

- Phase 276 (landed): foreground app input routing.
  `foreground_input_route.mc` now owns the first explicit foreground-app input handoff by consuming parsed input frames from `input_event.mc` and foreground truth from `launcher_service.mc`, while `kernel_dispatch.mc` stays a thin caller. The admitted path still routes to exactly one foreground app, keeps no-foreground explicit, and does not widen into background delivery, sessions, or routing frameworks. The focused reset-lane fixture is
  `tests/system/kernel_reset_lane_phase276_foreground_app_input_routing`.

- Phase 275 (landed): display surface owner first slice.
  `display_surface.mc` now owns one fixed four-cell visible target with one
  explicit present contract and one direct query path, while `launcher_service.mc`
  still owns only foreground-program truth. The original landing spent that
  surface through one bounded launch-time handoff; Phase 277 later replaced the
  old `program_catalog.mc` token shortcut with an app-owned present request
  without widening launcher or display. The focused reset-lane fixture is
  `tests/system/kernel_reset_lane_phase275_display_surface_owner`.

- Phase 273 (completed): app substrate pressure audit. Band C is now
  explicitly closed for its admitted slice: `program_catalog.mc`,
  `launcher_service.mc`, and `update_store_service.mc` remain the maintained
  app-substrate owners for the current path, while second-app install policy,
  richer app-facing status, broader execution semantics, and broader asset
  families stay deferred until a later real consumer proves one of them
  stronger than Phase 274's first UI input owner.

- Phase 272 (landed): first user-facing installed workflow demo.
  `launcher_service.mc` now exposes one explicit status query reporting
  `(installed program id, installed version, foreground program id, visible
  status)` for the admitted `issue_rollup` path, while durable installed bytes
  and launch-record comparison truth remain on `update_store_service.mc`. The
  focused reset-lane fixture is
  `tests/system/kernel_reset_lane_phase272_installed_workflow_demo`.

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
