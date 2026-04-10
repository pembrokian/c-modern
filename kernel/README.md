Canopus Kernel Tree
===================

This directory is the repository-owned home for real Canopus kernel sources.

Current status
--------------

- Phase 103 has moved the repository-owned kernel artifact beyond the landed
  timer-backed child-lifecycle proof into one explicit init bootstrap-
  capability handoff proof.
- Phase 104 is a narrow critique-response hardening pass over that same owned
  kernel artifact: timer wake consumption, bootstrap layout validation,
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
  hardening pass over that bounded proof kernel, and no real service bring-up
  yet.

Current files
-------------

- `build.toml`: freestanding kernel manifest for the current proof slice
- `src/main.mc`: explicit architecture entry, first-user-entry, endpoint-
  plus-handle-core validation, bounded syscall-byte-IPC validation, bounded
  capability-carrying transfer validation, bounded spawn-and-wait
  validation, bounded timer sleep and wake validation, bounded init
  bootstrap-capability handoff validation, and bounded Phase 104 contract-
  hardening regressions
- `src/state.mc`: kernel-owned descriptor, slot, queue, and boot-log records
- `src/address_space.mc`: bounded address-space, mapping, and user-entry-frame
  records
- `src/capability.mc`: bounded bootstrap capability slots, per-process
  handle-table state, explicit wait-handle state, and explicit handle-move
  helpers
- `src/endpoint.mc`: bounded endpoint table, queued-message ring, and
  attached-handle message state
- `src/interrupt.mc`: bounded interrupt controller skeleton
- `src/syscall.mc`: bounded syscall gate, byte-plus-capability request,
  spawn-and-wait request, and observation state
- `src/init.mc`: bounded boot-bundled init image descriptor plus explicit init
  bootstrap-capability handoff records

Phase boundary
--------------

- This tree now claims one bounded address-space setup, first user-entry
  handoff, one explicit endpoint-and-handle object-core follow-through, and
  one explicit syscall-owned byte-only IPC round trip plus one bounded
  attached-handle transfer plus one bounded spawn-and-wait lifecycle path plus
  one explicit critique-response hardening pass over helper and proof
  contracts.
- It does not yet claim general loading, real service launch, namespace
  policy, kill semantics, or a running init-owned service set.
