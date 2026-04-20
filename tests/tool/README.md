# Tool Tests

This directory contains driver, project-workflow, and support-layer regression
tests for the `mc` toolchain entrypoints.

Current structure

- `tool_suite_common.h` and `tool_suite_common.cpp`: shared project-writing,
  command-running, assertion helpers, and projected-MIR golden helpers used by
  the grouped tool suites.
- `tool_kernel_common.h` and `tool_kernel_common.cpp`: freestanding-kernel and
  reset-lane-specific helpers used by the workflow-owned kernel suites.
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
- `tool_real_project_tests.cpp`: real-project workflow driver.
- `tool_real_project_suite.cpp`: thin grouped real-project dispatch owner.
- `tool_real_project_suite_internal.h`: exported declarations for grouped
  real-project case functions.
- `real_projects/<case-name>/case.toml`: checked-in real-project descriptors
  used by CMake registration, grouped-runner dispatch, and local test
  selection.
- `real_projects/<case-name>/test.cpp`: case-local real-project
  implementation owner for that descriptor directory when the case has
  bespoke behavior.
- `tools/tool_case_manifest.py`: shared descriptor loader that now also
  checks descriptor family, runner, owned roots, and real-project co-located
  `test.cpp` files before registration output is generated.
- `../../docs/arch/c-lang/tools/grouped_tool_test_operator_reference.txt`:
  short operator-facing reference for grouped tool and real-project test
  maintenance.
- `tool_suite_tests.cpp` and `phase7_tool_tests.cpp`: compatibility runners
  only, kept for older references.
- the retired freestanding proof harness has been deleted; do not recreate it as a live suite.

Layout rule

- Keep real-project implementation ownership directory-local by default.
- Keep `tool_real_project_suite.cpp` limited to grouped dispatch and case
  lookup.
- Keep grouped workflow and real-project case identity descriptor-driven.
- Keep descriptor semantics unified through `tools/tool_case_manifest.py`.
- Keep the registration integrity check in the shared descriptor loader so
  CMake and the local selector see the same ownership truth.
- Add a new grouped workflow or real-project case by creating one adjacent
  `case.toml`, not by editing parallel case-name registries.
- Keep Veya-facing execution proofs in this directory for now rather than
  creating a separate `tests/tool/veya/` subtree.

Validation rule

- During focused iteration, run the narrowest owning tool test target.
- For active reset-lane kernel changes, prefer `mc_tool_workflow_kernel_reset_lane_unit`; it now builds and runs the repository-owned `kernel/build.toml` directly.
- For reset-lane changes that may affect suite cost or build reuse, also run `mc_tool_workflow_kernel_reset_lane_full_unit` and inspect the per-scenario `cache=hit` or `cache=miss` summary from `tool_kernel_reset_lane_suite.cpp`.
- If the first full run is correct, rerun it once immediately when practical. A large warm-run slowdown usually means build reuse regressed.
- Keep the full reset-lane surface as one maintained grouped case. Prefer repairing cached-build reuse inside the runner over adding manual shard-count maintenance.
- For cross-cutting driver or build changes, rerun the broader tool suite set
  before closing the change.
