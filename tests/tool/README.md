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
- `workflow/<case-name>/case.toml`: checked-in workflow case descriptors used
  by CMake registration, grouped-runner dispatch, and local test selection.
- `tool_build_state_tests.cpp` and `tool_build_state_suite.cpp`: interface
  artifact, incremental rebuild, and build-state validation.
- `tool_real_project_tests.cpp` and `tool_real_project_suite.cpp`: repository-
  owned real-project workflow coverage.
- `real_projects/<case-name>/case.toml`: checked-in real-project descriptors
  used by CMake registration, grouped-runner dispatch, and local test
  selection.
- `tool_suite_tests.cpp` and `phase7_tool_tests.cpp`: compatibility runners
  only, kept for older references.
- the retired freestanding proof harness has been deleted; do not recreate it as a live suite.

Layout rule

- Keep active suite implementation split by behavior family.
- Prefer subtrees and focused suite files over growing one monolithic file.
- Keep grouped workflow and real-project case identity descriptor-driven.
- Add a new grouped workflow or real-project case by creating one adjacent
  `case.toml`, not by editing parallel case-name registries.
- Keep Veya-facing execution proofs in this directory for now rather than
  creating a separate `tests/tool/veya/` subtree.

Validation rule

- During focused iteration, run the narrowest owning tool test target.
- For active reset-lane kernel changes, prefer `mc_tool_workflow_kernel_reset_lane_unit`; it now builds and runs the repository-owned `kernel/build.toml` directly.
- For cross-cutting driver or build changes, rerun the broader tool suite set
  before closing the change.
