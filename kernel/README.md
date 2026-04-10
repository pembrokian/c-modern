Canopus Kernel Tree
===================

This directory is the repository-owned home for real Canopus kernel sources.

Current status
--------------

- Phase 115 has moved the repository-owned kernel artifact beyond the landed
  Phase 114 address-space and MMU ownership split into one bounded timer
  ownership hardening step.
- Phase 104 remains the landed critique-response hardening pass over that same
  owned kernel artifact: timer wake consumption, bootstrap layout validation,
  endpoint and capability helper contracts, boot-log overflow visibility, and
  smaller proof-harness sequencing fixes are now explicit without widening the
  repository into a larger kernel architecture claim.
- The current owned slice is still intentionally small: explicit boot entry,
  deterministic kernel state tables, one bounded init address space plus
  saved user-entry frame, one bounded init-owned endpoint and handle table,
  one bounded queued-message lifetime proof, one bounded byte-only send and
  receive path with deterministic `WouldBlock` follow-through, one bounded
  attached-handle transfer with sender-side removal and receiver-local
  reinstall, one bounded program-capability spawn and explicit wait-handle
  reap path, one bounded timer sleep path with explicit wake delivery, one
  explicit init bootstrap-capability handoff, one landed helper-contract
  hardening pass over that bounded proof kernel, one bounded real log-service
  request-and-acknowledgment flow with explicit service reap, one bounded real
  echo-service request-reply flow with explicit service reap, one bounded real
  user-to-user endpoint transfer with explicit sender-side removal and
  receiver-side installation, one bounded kernel image-input and program-cap
  audit, one bounded first running Canopus kernel slice audit, one bounded
  kernel ownership split audit, one bounded scheduler and lifecycle ownership
  clarification, one bounded syscall boundary thinness audit, one bounded
  interrupt-entry and generic-dispatch boundary, one bounded address-space and
  MMU ownership split, one bounded timer ownership hardening step, and no
  broader multi-service bring-up yet.

Current files
-------------

- `build.toml`: freestanding kernel manifest for the current proof slice and
  the explicit kernel image-input contract carried through the Phase 115 timer
  ownership hardening step
- `src/main.mc`: explicit architecture entry, first-user-entry, endpoint-
  plus-handle-core setup, bounded syscall-byte-IPC setup, bounded capability-
  carrying transfer setup, thin service-proof orchestration, and thin root
  orchestration across the owned scheduler, lifecycle, bootstrap helper, and
  debug audit modules
- `src/bootstrap_audit.mc`: extracted Phase 104 contract hardening helpers,
  bounded service validation helpers, and Phase 108-109 audit builders used by
  the root proof module
- `src/bootstrap_services.mc`: extracted bounded log, echo, and transfer
  service execution flows plus explicit service config/state packaging used by
  the root proof module
- `src/sched.mc`: scheduler-owned lifecycle validation for bounded spawn,
  wait, sleep, and wake follow-through
- `src/lifecycle.mc`: lifecycle-owned task and process slot mutation for
  spawn, block, ready, exit, and waited-child release follow-through
- `src/debug.mc`: debug-owned Phase 108 image/program-cap audit, Phase 109
  running-slice audit, Phase 110 ownership-split audit, Phase 111
  lifecycle-ownership audit, Phase 112 syscall-boundary audit, Phase 113
  interrupt-boundary audit, Phase 114 address-space/MMU audit, and Phase 115
  timer-ownership audit
- `src/log_service.mc`: bounded log-service protocol state, acknowledgment
  payload, and final handshake observation records
- `src/echo_service.mc`: bounded echo-service protocol state, request-derived
  reply payload, and final exchange observation records
- `src/transfer_service.mc`: bounded transfer-service grant state, emitted
  payload construction, and final transfer observation records
- `src/state.mc`: kernel-owned descriptor, slot, queue, and boot-log records
- `src/address_space.mc`: bounded address-space, mapping, and user-entry-frame
  records
- `src/mmu.mc`: bounded translation-root construction and activation helpers
  for the landed first-user and spawn path
- `src/timer.mc`: bounded timer state, sleep records, wake observations, and
  interrupt-tick delivery helpers for the landed timer-backed wake path
- `src/capability.mc`: bounded bootstrap capability slots, per-process
  handle-table state, explicit wait-handle state, and explicit handle-move
  helpers
- `src/endpoint.mc`: bounded endpoint table, queued-message ring, and
  attached-handle message state plus endpoint-owned runtime queue helpers for
  the landed syscall slice
- `src/interrupt.mc`: bounded interrupt controller, architecture-entry
  records, and generic dispatch classification for the landed timer-backed
  wake path
- `src/syscall.mc`: bounded syscall gate, byte-plus-capability request,
  spawn-and-wait request, and thin observation state over capability,
  endpoint, address-space, and lifecycle owners
- `src/init.mc`: bounded boot-bundled init image descriptor plus explicit init
  bootstrap-capability handoff records

Phase boundary
--------------

- This tree now claims one bounded address-space setup, first user-entry
  handoff, one explicit endpoint-and-handle object-core follow-through, one
  explicit syscall-owned byte-only IPC round trip, one bounded attached-
  handle transfer, one bounded spawn-and-wait lifecycle path, one explicit
  critique-response hardening pass over helper and proof contracts, one
  bounded real log-service request-and-acknowledgment flow with explicit
  service reap, one bounded real echo-service request-reply flow with explicit
  service reap, one bounded real user-to-user endpoint transfer with explicit
  service-side follow-through, one bounded kernel image-and-program-cap audit,
  one bounded first running Canopus kernel slice audit, one bounded kernel
  ownership split audit, one bounded scheduler and lifecycle ownership
  clarification, one bounded syscall boundary thinness audit, one bounded
  interrupt-entry and generic-dispatch boundary, one bounded address-space and
  MMU ownership split, and one bounded timer ownership hardening step.
- The repository can now honestly claim one first running Canopus kernel
  slice with an explicit Phase 115 timer ownership hardening over the landed
  Phase 114 address-space and MMU ownership split: explicit
  boot
  entry,
  first user entry, endpoint-and-handle object core, syscall-owned byte-only
  IPC, attached-handle transfer, program-cap spawn and wait, timer sleep and
  wake, init bootstrap-capability handoff, real log-service handshake, real
  echo-service request-reply, real user-to-user capability transfer, and
  kernel image/program-cap audit, with syscall decode and observation shaping
  separated from capability, endpoint, address-space, and lifecycle
  semantics, with one bounded interrupt path expressed as explicit
  architecture entry plus generic dispatch classification, with
  translation-root mechanics separated from generic address-space layout, and
  with timer-owned tick delivery separated from interrupt classification.
- It does not yet claim general loading, multi-service launch, namespace
  policy, kill semantics, or a running init-owned service set.
