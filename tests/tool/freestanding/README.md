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
  - `phase97_user_entry.cpp`: real-kernel address-space and first-user-entry proof.
  - `phase98_endpoint_handle_core.cpp`: real-kernel endpoint-and-handle-core proof.
  - `phase105_real_log_service_handshake.cpp`: real-kernel log-service handshake proof.
  - `phase106_real_echo_service_request_reply.cpp`: real-kernel echo-service request-reply proof.
  - `phase107_real_user_to_user_capability_transfer.cpp`: real-kernel user-to-user capability transfer proof.
  - `phase108_kernel_image_program_cap_audit.cpp`: real-kernel image-input and program-cap audit.
  - `phase109_first_running_kernel_slice_audit.cpp`: real-kernel first-running-kernel-slice support audit.
  - `phase110_kernel_ownership_split_audit.cpp`: real-kernel ownership split audit.
  - `phase111_scheduler_lifecycle_ownership_clarification.cpp`: real-kernel scheduler and lifecycle ownership clarification audit.
  - `phase112_syscall_boundary_thinness_audit.cpp`: real-kernel syscall boundary thinness audit.
  - `phase113_interrupt_entry_and_generic_dispatch_boundary.cpp`: real-kernel interrupt-entry and generic-dispatch boundary audit.
- `system/suite.cpp`: init, user-space policy, timer wake, and first-system
  integration proofs.

Rule of thumb

- Split by ownership boundary first.
- Split to one proof per file when a proof has enough setup and assertions to
  stand on its own.
- Keep shared project-writing and command helpers in `tests/tool/tool_suite_common.*`
  until a narrower freestanding-only helper layer is justified.
