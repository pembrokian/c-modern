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
    The top-level `kernel-docs` surface now discovers documentation audits from
    `kernel/docs/*/audit.toml`.
    The top-level `kernel-artifacts` surface now discovers artifact audits from
    `kernel/artifact_specs/*/artifact.toml`.
    The top-level `kernel-runtime` surface now discovers runtime proofs from
    `kernel/runtime/phase.../phase.toml` without shard dispatch.
    The top-level `kernel-synthetic` surface owns the standalone phases85-88
    proofs.
  - `kernel/phase97_user_entry.cpp` through
    `kernel/phase104_kernel_critique_hardening.cpp`: earlier focused
    freestanding kernel proof files that still stand alone, with owned MIR
    goldens under `kernel/runtime/legacy_goldens/`.
  - `kernel/synthetic/suite.cpp`: early synthetic standalone-project proof owner for
    phases85-88.
  - late ownership-hardening kernel audits keep checked-in kernel goldens under
    `kernel/runtime/phase.../` for per-phase descriptors plus adjacent run/MIR
    expectations, with earlier standalone runtime MIR goldens under
    `kernel/runtime/legacy_goldens/` and artifact expectations under
    `kernel/artifact_specs/.../` when needed.
  - kernel documentation audits now live beside their descriptor files under
    `kernel/docs/...` instead of as a handwritten assertion list.
  - kernel artifact audits now live beside their descriptor files and support
    files under `kernel/artifact_specs/...` instead of as a handwritten
    assertion list.
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
- For freestanding or Canopus-facing changes, prefer the narrowest owning freestanding surface first: `mc_tool_freestanding_bootstrap_unit`, `mc_tool_freestanding_kernel_runtime_unit`, `mc_tool_freestanding_kernel_synthetic_unit`, `mc_tool_freestanding_kernel_docs_unit`, `mc_tool_freestanding_kernel_artifacts_unit`, or `mc_tool_freestanding_system_unit`.
- The top-level runtime/synthetic/docs/artifacts targets are the freestanding kernel workflow surfaces.
- For targeted kernel debugging, prefer the top-level direct runtime selector such as `build/debug/bin/mc_tool_freestanding_tests /Users/ro/dev/c_modern /Users/ro/dev/c_modern/build/debug kernel-runtime:phase106_real_echo_service_request_reply`.
  For standalone synthetic proofs, use `kernel-synthetic:<label>`.
- For cross-cutting driver or build changes, rerun the broader tool suite set
  before closing the change.
