# Tool Tests

This directory contains driver, project-workflow, and support-layer regression
tests for the `mc` toolchain entrypoints.

Current structure

- `tool_suite_common.h` and `tool_suite_common.cpp`: shared project-writing,
  command-running, assertion helpers, and projected-MIR golden helpers used by
  the grouped tool suites.
- `tool_workflow_tests.cpp` and `tool_workflow_suite.cpp`: CLI, project graph,
  and workflow validation.
- `tool_build_state_tests.cpp` and `tool_build_state_suite.cpp`: interface
  artifact, incremental rebuild, and build-state validation.
- `tool_real_project_tests.cpp` and `tool_real_project_suite.cpp`: repository-
  owned real-project workflow coverage.
- `tool_freestanding_tests.cpp`: freestanding tool-suite driver.
- `freestanding/`: freestanding proof subtree.
  - `suite.cpp`: top-level freestanding orchestrator.
  - `bootstrap/suite.cpp`: early freestanding bootstrap and narrow `hal`
    proof coverage.
  - `kernel/suite.cpp`: kernel freestanding orchestrator.
  - `kernel/phase85_endpoint_queue.cpp`, `kernel/phase86_task_lifecycle.cpp`,
    `kernel/phase87_static_data.cpp`, `kernel/phase88_build_integration.cpp`,
    `kernel/phase97_user_entry.cpp`, `kernel/phase98_endpoint_handle_core.cpp`,
    `kernel/phase105_real_log_service_handshake.cpp`,
    `kernel/phase106_real_echo_service_request_reply.cpp`,
    `kernel/phase107_real_user_to_user_capability_transfer.cpp`,
    `kernel/phase108_kernel_image_program_cap_audit.cpp`,
    `kernel/phase109_first_running_kernel_slice_audit.cpp`,
      `kernel/phase110_kernel_ownership_split_audit.cpp`,
      `kernel/phase111_scheduler_lifecycle_ownership_clarification.cpp`,
      `kernel/phase112_syscall_boundary_thinness_audit.cpp`,
      `kernel/phase113_interrupt_entry_and_generic_dispatch_boundary.cpp`,
      `kernel/phase114_address_space_and_mmu_ownership_split.cpp`,
      `kernel/phase115_timer_ownership_hardening.cpp`,
      `kernel/phase116_mmu_activation_barrier_follow_through.cpp`,
      `kernel/phase117_init_orchestrated_multi_service_bring_up.cpp`,
      `kernel/phase118_request_reply_and_delegation_follow_through.cpp`,
      `kernel/phase119_namespace_pressure_audit.cpp`,
      `kernel/phase120_running_system_support_statement.cpp`,
      `kernel/phase121_kernel_image_contract_hardening.cpp`,
      `kernel/phase122_target_surface_audit.cpp`,
      `kernel/phase123_next_plateau_audit.cpp`,
      `kernel/phase124_delegation_chain_stress.cpp`, and
      `kernel/phase125_invalidation_and_rejection_audit.cpp`, and
      `kernel/phase126_authority_lifetime_classification.cpp`,
      `kernel/phase128_service_death_observation.cpp`, and
      `kernel/phase129_partial_failure_propagation.cpp`, and
      `kernel/phase130_explicit_restart_or_replacement.cpp`, and
      `kernel/phase131_fan_in_or_fan_out_composition.cpp`, and
      `kernel/phase132_backpressure_and_blocking.cpp`, and
      `kernel/phase133_message_lifetime_and_reuse.cpp`, and
      `kernel/phase134_minimal_device_service_handoff.cpp`, and
      `kernel/phase135_buffer_ownership_boundary_audit.cpp`, and
      `kernel/phase136_device_failure_containment_probe.cpp`, and
      `kernel/phase137_optional_dma_or_equivalent_follow_through.cpp`: one kernel proof per file.
  - late ownership-hardening kernel audits also keep adjacent `.mirproj.txt`
    files for projected MIR golden expectations.
  - `system/suite.cpp`: init, user-space policy, timer wake, and integrated-
    system coverage.
- `tool_suite_tests.cpp` and `phase7_tool_tests.cpp`: compatibility runners
  only, kept for older references.

Layout rule

- Keep active suite implementation split by behavior family.
- Prefer subtrees and focused suite files over growing one monolithic file.
- When a freestanding kernel audit needs structural ownership checks, prefer an
  adjacent projected MIR golden over raw source-text assertions if the merged
  MIR already preserves the relevant routed call or owner-local symbol.
- Keep Canopus-facing execution proofs in this directory for now rather than
  creating a separate `tests/tool/canopus/` subtree.

Validation rule

- During focused iteration, run the narrowest owning tool test target.
- For freestanding or Canopus-facing changes, prefer the narrowest owning freestanding slice first: `mc_tool_freestanding_bootstrap_unit`, the owning per-case kernel proof `mc_tool_freestanding_kernel_case_<name>_unit`, or `mc_tool_freestanding_system_unit`.
- For targeted kernel timing or one-proof debugging, run the freestanding test binary directly with a case selector such as `build/debug/bin/mc_tool_freestanding_tests /Users/ro/dev/c_modern /Users/ro/dev/c_modern/build/debug 'kernel-case:phase85_endpoint_queue'`.
  The equivalent CTest entrypoint is `mc_tool_freestanding_kernel_case_phase85_endpoint_queue_unit`.
  first.
- For cross-cutting driver or build changes, rerun the broader tool suite set
  before closing the change.
