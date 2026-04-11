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
  - `phase114_address_space_and_mmu_ownership_split.cpp`: real-kernel address-space and MMU ownership split audit.
  - `phase115_timer_ownership_hardening.cpp`: real-kernel timer ownership hardening audit.
  - `phase116_mmu_activation_barrier_follow_through.cpp`: real-kernel MMU activation barrier follow-through audit.
  - `phase117_init_orchestrated_multi_service_bring_up.cpp`: real-kernel init-orchestrated multi-service bring-up audit.
  - `phase118_request_reply_and_delegation_follow_through.cpp`: real-kernel delegated request-reply and source-invalidation audit.
  - `phase119_namespace_pressure_audit.cpp`: real-kernel fixed service-directory and namespace-pressure audit.
  - `phase120_running_system_support_statement.cpp`: real-kernel running-system support-statement audit.
  - `phase121_kernel_image_contract_hardening.cpp`: real-kernel image-contract hardening audit.
- `system/suite.cpp`: init, user-space policy, timer wake, and first-system
  integration proofs.

Late kernel audit pattern

- For ownership-hardening kernel audits, keep one proof owner `.cpp` per phase.
- Split those phase owners into three local slices when practical:
  behavior, publication, and MIR structure.
- Keep behavior assertions in C++ over the built artifact and emitted objects.
- Keep publication assertions in C++ over the phase note, README, repo map,
  and other intentionally published status files.
- Keep MIR structure assertions as projected goldens in adjacent
  `.mirproj.txt` files rather than embedding long expected MIR snippets in the
  `.cpp` file.
- Use `ExpectMirFirstMatchProjectionFile` from `tests/tool/tool_suite_common.*`
  to compare those projected MIR goldens against the merged dump.

Rule of thumb

- Split by ownership boundary first.
- Split to one proof per file when a proof has enough setup and assertions to
  stand on its own.
- For late kernel audits, prefer projected MIR golden files over raw source
  text scanning when the merged MIR already carries the ownership and routing
  facts you need.
- Keep shared project-writing and command helpers in `tests/tool/tool_suite_common.*`
  until a narrower freestanding-only helper layer is justified.
