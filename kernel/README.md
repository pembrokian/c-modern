Canopus Kernel Tree
===================

This directory is the repository-owned home for real Canopus kernel sources.

Current status
--------------

- Phase 100 has moved the repository-owned kernel artifact beyond the landed
  byte-only syscall boundary into one bounded capability-carrying IPC
  transfer proof.
- The current owned slice is still intentionally small: explicit boot entry,
  deterministic kernel state tables, one bounded init address space plus
  saved user-entry frame, one bounded init-owned endpoint and handle table,
  one bounded queued-message lifetime proof, one bounded byte-only send and
  receive path with deterministic `WouldBlock` follow-through, one bounded
  attached-handle transfer with sender-side removal and receiver-local
  reinstall, and no spawn, wait, or service bring-up yet.

Current files
-------------

- `build.toml`: freestanding kernel manifest for the current proof slice
- `src/main.mc`: explicit architecture entry, first-user-entry, endpoint-
  plus-handle-core validation, bounded syscall-byte-IPC validation, and one
  bounded capability-carrying transfer validation
- `src/state.mc`: kernel-owned descriptor, slot, queue, and boot-log records
- `src/address_space.mc`: bounded address-space, mapping, and user-entry-frame
  records
- `src/capability.mc`: bounded bootstrap capability slots, per-process
  handle-table state, and explicit handle-move helpers
- `src/endpoint.mc`: bounded endpoint table, queued-message ring, and
  attached-handle message state
- `src/interrupt.mc`: bounded interrupt controller skeleton
- `src/syscall.mc`: bounded syscall gate, byte-plus-capability request, and
  receive-observation state
- `src/init.mc`: bounded boot-bundled init image descriptor skeleton

Phase boundary
--------------

- This tree now claims one bounded address-space setup, first user-entry
  handoff, one explicit endpoint-and-handle object-core follow-through, and
  one explicit syscall-owned byte-only IPC round trip plus one bounded
  attached-handle transfer.
- It does not yet claim general loading, spawn,
  wait, interrupt handling, timer-backed scheduling, or a running init-owned
  service set.
