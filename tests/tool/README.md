# Tool Tests

This directory contains driver, project-workflow, and support-layer regression
tests for the `mc` toolchain entrypoints.

Current structure

- `tool_suite_common.h` and `tool_suite_common.cpp`: shared project-writing,
  command-running, and assertion helpers used by the grouped tool suites.
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
    `kernel/phase107_real_user_to_user_capability_transfer.cpp`: one kernel proof per file.
  - `system/suite.cpp`: init, user-space policy, timer wake, and integrated-
    system coverage.
- `tool_suite_tests.cpp` and `phase7_tool_tests.cpp`: compatibility runners
  only, kept for older references.

Layout rule

- Keep active suite implementation split by behavior family.
- Prefer subtrees and focused suite files over growing one monolithic file.
- Keep Canopus-facing execution proofs in this directory for now rather than
  creating a separate `tests/tool/canopus/` subtree.

Validation rule

- During focused iteration, run the narrowest owning tool test target.
- For freestanding or Canopus-facing changes, prefer `mc_tool_freestanding_unit`
  first.
- For cross-cutting driver or build changes, rerun the broader tool suite set
  before closing the change.
