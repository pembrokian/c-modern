Canopus Kernel Tree
===================

This directory is the repository-owned home for real Canopus kernel sources.

Current status
--------------

- Phase 97 has moved the repository-owned kernel artifact beyond boot-only
  state seeding into one bounded address-space and first-user-entry proof.
- The current owned slice is still intentionally small: explicit boot entry,
  deterministic kernel state tables, one bounded init address space plus
  saved user-entry frame, bounded capability or endpoint or interrupt or
  syscall skeleton records, and no syscall or service bring-up yet.

Current files
-------------

- `build.toml`: freestanding kernel manifest for the current proof slice
- `src/main.mc`: explicit architecture entry and first-user-entry validation
- `src/state.mc`: kernel-owned descriptor, slot, queue, and boot-log records
- `src/address_space.mc`: bounded address-space, mapping, and user-entry-frame
  records
- `src/capability.mc`: bounded capability slot skeleton
- `src/endpoint.mc`: bounded endpoint table skeleton
- `src/interrupt.mc`: bounded interrupt controller skeleton
- `src/syscall.mc`: bounded syscall gate skeleton
- `src/init.mc`: bounded boot-bundled init image descriptor skeleton

Phase boundary
--------------

- This tree now claims one bounded address-space setup and first user-entry
  handoff only.
- It does not yet claim general loading, spawn, syscall execution, endpoint
  delivery, interrupt handling, timer-backed scheduling, or a running init-
  owned service set.
