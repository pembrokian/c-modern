Canopus Kernel Tree
===================

This directory is the repository-owned home for real Canopus kernel sources.

Current status
--------------

- Phase 130 has moved the repository-owned kernel artifact beyond the landed
  Phase 129 partial failure propagation step into one bounded explicit
  restart or replacement probe.
- Phase 129 has moved the repository-owned kernel artifact beyond the landed
  Phase 128 service death observation step into one bounded partial failure
  propagation step.
- Phase 128 has moved the repository-owned kernel artifact beyond the landed
  Phase 126 authority lifetime classification step into one bounded service
  death observation step.
- Phase 125 has moved the repository-owned kernel artifact beyond the landed
  Phase 124 delegation-chain stress step into one bounded invalidation and
  rejection audit step.
- Phase 124 has moved the repository-owned kernel artifact beyond the landed
  Phase 123 next-plateau audit into one bounded delegation-chain stress step.
- Phase 123 has moved the repository-owned kernel artifact beyond the landed
  Phase 122 target-surface audit into one bounded next-plateau audit.
- Phase 122 has moved the repository-owned kernel artifact beyond the landed
  Phase 121 kernel image-contract hardening step into one bounded target-surface audit.
- Phase 121 has moved the repository-owned kernel artifact beyond the landed
  Phase 120 running-system support statement into one bounded kernel image-contract hardening step.
- Phase 120 has moved the repository-owned kernel artifact beyond the landed
  Phase 119 namespace-pressure audit into one bounded running-system support statement.
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
  MMU ownership split, one bounded timer ownership hardening step, one bounded
  MMU activation barrier follow-through step, one bounded init-orchestrated multi-service
  bring-up step, one bounded delegated request-reply follow-through step, one
  bounded init-owned fixed service-directory step, one bounded target-surface
  audit, one bounded next-plateau audit, one bounded delegation-chain stress
  step, one bounded invalidation and rejection audit step, and one bounded
  authority lifetime classification step, one bounded service death
  observation step, one bounded partial failure propagation step, and one
  bounded explicit restart or replacement probe.

Current files
-------------

- `build.toml`: freestanding kernel manifest for the current proof slice and
  the explicit kernel image-input contract carried through the Phase 121
  image-contract hardening step
- `src/main.mc`: explicit architecture entry, first-user-entry, endpoint-
  plus-handle-core setup, bounded syscall-byte-IPC setup, bounded capability-
  carrying transfer setup, bounded init-owned multi-service orchestration,
  one bounded delegated request-reply follow-through, one bounded fixed
  service-directory publication step, one bounded next-plateau publication
  step, one bounded delegation-chain stress step, one bounded invalidation
  and rejection audit step, one bounded authority lifetime classification
  step, one bounded service death observation step, one bounded partial
  failure propagation step, one bounded explicit restart or replacement
  probe, and thin root orchestration across the owned scheduler, lifecycle,
  bootstrap helper, and debug audit modules
- `src/bootstrap_audit/`: one logical `bootstrap_audit` module split through
  `module_sets.bootstrap_audit`, owning the extracted Phase 104 contract
  hardening helpers, bounded service validation helpers, Phase 108-129 audit
  builders, and the Phase 130 explicit restart-or-replacement audit builder
  used by the root proof module
- `src/bootstrap_services/`: one logical `bootstrap_services` module split
  through `module_sets.bootstrap_services`, owning extracted bounded log,
  echo, and transfer service execution flows plus explicit service config and
  state packaging used by the root proof module and the aggregate late-phase
  running-system audits
- `src/sched.mc`: scheduler-owned lifecycle validation for bounded spawn,
  wait, sleep, and wake follow-through
- `src/lifecycle.mc`: lifecycle-owned task and process slot mutation for
  spawn, block, ready, exit, and waited-child release follow-through
- `src/debug/`: one logical `debug` module split through `module_sets.debug`,
  owning Phase 108 image/program-cap audit, Phase 109 running-slice audit,
  Phase 110 ownership-split audit, Phase 111 lifecycle-ownership audit,
  Phase 112 syscall-boundary audit, Phase 113 interrupt-boundary audit,
  Phase 114 address-space/MMU audit, Phase 115 timer-ownership audit,
  Phase 116 MMU activation-barrier audit, Phase 117 init-orchestrated
  multi-service audit, Phase 118 delegated request-reply audit, Phase 119
  namespace-pressure audit, Phase 120 running-system support audit, Phase 121
  kernel image-contract hardening audit, Phase 122
  target-surface audit, Phase 123 next-plateau audit, Phase 124
  delegation-chain stress audit, Phase 125 invalidation and rejection audit,
  Phase 126 authority lifetime classification audit, Phase 128 service death
  observation audit, Phase 129 partial failure propagation audit, and Phase
  130 explicit restart or replacement audit
- `src/log_service.mc`: bounded log-service protocol state, acknowledgment
  payload, and final handshake observation records
- `src/echo_service.mc`: bounded echo-service protocol state, request-derived
  reply payload, and final exchange observation records
- `src/transfer_service.mc`: bounded transfer-service grant state, emitted
  payload construction, and final transfer observation records
- `src/state.mc`: kernel-owned descriptor, slot, queue, and boot-log records
- `src/address_space.mc`: bounded address-space, mapping, and user-entry-frame
  records
- `src/mmu.mc`: bounded translation-root construction, activation, and
  barrier-backed publish helpers for the landed first-user and spawn path
- `src/timer.mc`: bounded timer state, sleep records, wake observations, and
  interrupt-tick delivery helpers for the landed timer-backed wake path
- `src/capability/`: one logical `capability` module split through
  `module_sets.capability`, owning bounded bootstrap capability slots,
  per-process handle-table state, explicit wait-handle state, and explicit
  handle-move helpers
- `src/endpoint/`: one logical `endpoint` module split through
  `module_sets.endpoint`, owning bounded endpoint table, queued-message ring,
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
  MMU ownership split, one bounded timer ownership hardening step, one
  bounded MMU activation barrier follow-through step, one bounded
  init-orchestrated multi-service bring-up step, one bounded delegated
  request-reply follow-through step, one bounded init-owned fixed service-
  directory step, one bounded target-surface audit, one bounded next-plateau
  audit, one bounded delegation-chain stress step, one bounded invalidation
  and rejection audit step, and one bounded authority lifetime classification
  step, one bounded service death observation step, and one bounded partial
  failure propagation step.
- The repository can now honestly claim one first running Canopus kernel
  slice with an explicit Phase 120 running-system support statement over the
  landed Phase 118 delegated request-reply follow-through, landed Phase 121
  kernel image-contract hardening step, landed Phase 122 target-surface
  audit, a landed Phase 123 next-plateau audit, a landed Phase 124
  delegation-chain stress step, a landed Phase 125 invalidation and
  rejection audit step, a landed Phase 126 authority lifetime
  classification step, a landed Phase 128 service death observation step,
  and a landed Phase 129 partial failure propagation step:
  explicit
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
  with timer-owned tick delivery separated from interrupt classification, and
  with translation-root activation published through one explicit `hal`
  barrier hook, and with init explicitly orchestrating one bounded multi-
  service set over the landed service owners, and with one delegated reply
  route plus one invalidated-source refusal published explicitly over the
  landed transfer-service owner, and with one fixed init-owned three-entry
  service directory published explicitly as the current namespace-pressure
  answer, and with one bounded running-system support statement plus one
  bounded image-contract hardening step plus one bounded target-surface audit
  plus one bounded next-plateau audit plus one bounded delegation-chain
  stress step plus one bounded invalidation and rejection audit step plus one
  bounded authority lifetime classification step plus one bounded service
  death observation step plus one bounded partial failure propagation step
  publishing that same admitted slice without widening into a broader service
  framework or supervision policy.
- It does not yet claim general loading, dynamic service discovery,
  namespace policy, kill semantics, or a general running init-owned service
  framework.
