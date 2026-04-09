# Freestanding Tool Tests

This subtree contains the repository-owned freestanding and Canopus-facing tool
proofs.

Structure

- `suite.cpp`: top-level freestanding orchestrator.
- `bootstrap/suite.cpp`: early freestanding bootstrap, explicit target, link
  input, and narrow `hal` proof coverage.
- `kernel/`: kernel-owned freestanding proofs.
  - `suite.cpp`: kernel proof orchestrator.
  - `phase85_endpoint_queue.cpp`: endpoint queue proof.
  - `phase86_task_lifecycle.cpp`: task lifecycle proof.
  - `phase87_static_data.cpp`: kernel static-data proof.
  - `phase88_build_integration.cpp`: emitted-object and relink proof.
  - `phase96_boot_entry.cpp`: real-kernel boot-entry and state-skeleton proof.
- `system/suite.cpp`: init, user-space policy, timer wake, and first-system
  integration proofs.

Rule of thumb

- Split by ownership boundary first.
- Split to one proof per file when a proof has enough setup and assertions to
  stand on its own.
- Keep shared project-writing and command helpers in `tests/tool/tool_suite_common.*`
  until a narrower freestanding-only helper layer is justified.
