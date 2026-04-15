# Tool Tests

This directory contains driver, project-workflow, and support-layer regression
tests for the `mc` toolchain entrypoints.

Current structure

- `tool_suite_common.h` and `tool_suite_common.cpp`: shared project-writing,
  command-running, assertion helpers, and projected-MIR golden helpers used by
  the grouped tool suites.
- `tool_workflow_tests.cpp`: workflow validation driver.
- `tool_workflow_orchestrator.cpp`: thin workflow suite entry that dispatches
  to the behavior-owned workflow families.
- `tool_help_suite.cpp`: help text, mode selection, and direct-source versus
  project workflow validation.
- `tool_test_command_suite.cpp`: `mc test` ordinary-test and regression
  workflow validation.
- `tool_project_validation_suite.cpp`: target selection, import-root,
  duplicate-root, and project-graph validation.
- `tool_multifile_module_suite.cpp`: module-set and multi-file module
  validation.
- `tool_kernel_reset_lane_suite.cpp`: reset-lane kernel workflow validation,
  including the table-driven fixture family.
- `tool_build_state_tests.cpp` and `tool_build_state_suite.cpp`: interface
  artifact, incremental rebuild, and build-state validation.
- `tool_real_project_tests.cpp` and `tool_real_project_suite.cpp`: repository-
  owned real-project workflow coverage.
- `tool_suite_tests.cpp` and `phase7_tool_tests.cpp`: compatibility runners
  only, kept for older references.
- `archive/legacy_freestanding/`: retired freestanding proof harness, `kernel_old/`, and maintenance helpers preserved for history only.

Layout rule

- Keep active suite implementation split by behavior family.
- Prefer subtrees and focused suite files over growing one monolithic file.
- Keep Veya-facing execution proofs in this directory for now rather than
  creating a separate `tests/tool/veya/` subtree.

Validation rule

- During focused iteration, run the narrowest owning tool test target.
- For active reset-lane kernel changes, prefer `mc_tool_workflow_unit`; it now builds and runs the repository-owned `kernel/build.toml` directly.
- For cross-cutting driver or build changes, rerun the broader tool suite set
  before closing the change.
