Canopus Kernel Tree
===================

This directory is the repository-owned home for real Canopus kernel sources.

Current status
--------------

- Phase 96 has moved from an inline freestanding test-only proof into this
  tree.
- The current owned slice is still intentionally small: explicit boot entry,
  deterministic kernel state tables, bounded capability or endpoint or
  interrupt or syscall skeleton records, and no user-space handoff yet.

Current files
-------------

- `build.toml`: freestanding kernel manifest for the current proof slice
- `src/main.mc`: explicit architecture entry and boot-state validation
- `src/state.mc`: kernel-owned descriptor, slot, queue, and boot-log records
- `src/capability.mc`: bounded capability slot skeleton
- `src/endpoint.mc`: bounded endpoint table skeleton
- `src/interrupt.mc`: bounded interrupt controller skeleton
- `src/syscall.mc`: bounded syscall gate skeleton
- `src/init.mc`: bounded init image descriptor skeleton

Phase boundary
--------------

- This tree does not yet claim address-space setup, first user entry, syscall
  execution, endpoint delivery, interrupt handling, or a running init task.
- Those remain later kernel bring-up work.
