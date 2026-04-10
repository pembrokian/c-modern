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
      `kernel/phase111_scheduler_lifecycle_ownership_clarification.cpp`: one kernel proof per file.
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
- For freestanding or Canopus-facing changes, prefer `mc_tool_freestanding_unit`
  first.
- For cross-cutting driver or build changes, rerun the broader tool suite set
  before closing the change.
