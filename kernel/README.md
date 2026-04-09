Canopus Kernel Tree
===================

This directory is the repository-owned home for real Canopus kernel sources.

Current status
--------------

- Phase 101 has moved the repository-owned kernel artifact beyond the landed
  capability-carrying IPC boundary into one bounded program-cap spawn and
  wait proof.
- The current owned slice is still intentionally small: explicit boot entry,
  deterministic kernel state tables, one bounded init address space plus
  saved user-entry frame, one bounded init-owned endpoint and handle table,
  one bounded queued-message lifetime proof, one bounded byte-only send and
  receive path with deterministic `WouldBlock` follow-through, one bounded
  attached-handle transfer with sender-side removal and receiver-local
  reinstall, one bounded program-capability spawn and explicit wait-handle
  reap path, and no timer-backed blocking or service bring-up yet.

Current files
-------------

- `build.toml`: freestanding kernel manifest for the current proof slice
- `src/main.mc`: explicit architecture entry, first-user-entry, endpoint-
  plus-handle-core validation, bounded syscall-byte-IPC validation, bounded
  capability-carrying transfer validation, and one bounded spawn-and-wait
  validation
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
- `src/init.mc`: bounded boot-bundled init image descriptor skeleton

Phase boundary
--------------

- This tree now claims one bounded address-space setup, first user-entry
  handoff, one explicit endpoint-and-handle object-core follow-through, and
  one explicit syscall-owned byte-only IPC round trip plus one bounded
  attached-handle transfer plus one bounded spawn-and-wait lifecycle path.
- It does not yet claim general loading, interrupt handling, timer-backed
  scheduling, kill semantics, or a running init-owned service set.
