# Freestanding Tool Tests

This subtree contains the repository-owned freestanding and Canopus-facing tool
proofs.

Structure

- `suite.cpp`: top-level freestanding orchestrator.
- `bootstrap/suite.cpp`: early freestanding bootstrap, explicit target, link
  input, and narrow `hal` proof coverage.
- `kernel/`: kernel-owned freestanding proofs.
  - `suite.cpp`: kernel proof orchestrator.
    The `kernel-runtime`, `kernel-docs`, and `kernel-artifacts` top-level
    surfaces now load their owned checks from descriptor directories under
    `kernel/runtime/...`, `kernel/docs/...`, and
    `kernel/artifact_specs/...`.
  - `phase97_user_entry.cpp`: real-kernel address-space and first-user-entry proof.
  - `phase98_endpoint_handle_core.cpp`: real-kernel endpoint-and-handle-core proof.
  - `phase99_syscall_byte_ipc.cpp` through `phase104_kernel_critique_hardening.cpp`: earlier focused real-kernel proof files that still stand alone, with owned MIR goldens under `runtime/legacy_goldens/`.
  - `synthetic/suite.cpp`: standalone synthetic proof owner for phases85-88.
- `system/suite.cpp`: init, user-space policy, timer wake, and first-system
  integration proofs.

Late kernel audit pattern

- For ownership-hardening kernel audits, keep checked-in per-phase descriptors under `kernel/runtime/phase.../` with adjacent run and projected MIR expectations, keep the earlier standalone runtime MIR goldens under `kernel/runtime/legacy_goldens/`, and keep artifact expectations adjacent to `kernel/artifact_specs/.../artifact.toml`.
- Treat shard1 phases85-88 as a separate synthetic-project proof surface rather
  than part of the shared runtime descriptor contract. They write standalone
  projects and, in phase88, own an explicit relink proof.
- Keep phase-note, roadmap, and repo-map assertions in the separate kernel docs surface, with one descriptor directory per audit under `kernel/docs/...`, rather than in the runtime shard itself.
- Keep behavior assertions over built artifacts and emitted objects in the separate kernel artifacts surface, with one descriptor directory per audit under `kernel/artifact_specs/...`.
- Keep publication assertions descriptor-owned over the phase note, README,
  repo map, and other intentionally published status files.
- Keep MIR structure assertions as projected goldens adjacent to their owning
  runtime phase descriptor or under `kernel/runtime/legacy_goldens/` for the
  earlier standalone proofs, rather than embedding long expected MIR snippets
  in the `.cpp` file.
- When a phase's descriptor data becomes repetitive, prefer another checked-in
  `kernel/runtime/phase.../phase.toml` directory over another large
  static C++ table.
- Runtime-folder kernel phases now own execution metadata in `phase.toml`
  (`output_stem`, `build_context`, and `run_context`) in addition to run and
  MIR golden references.
- Runtime-folder kernel phases now also declare the explicit MIR contract
  `mir_contract = "first-match-projection-v1"` in `phase.toml`.
- Use `ExpectMirFirstMatchProjectionFile` from `tests/tool/tool_suite_common.*`
  as the current implementation of that contract when comparing projected MIR
  goldens against the merged dump.

Rule of thumb

- Split by ownership boundary first.
- Split to one proof per file when a proof has enough setup and assertions to
  stand on its own.
- For late kernel audits, prefer projected MIR golden files over raw source
  text scanning when the merged MIR already carries the ownership and routing
  facts you need.
- Keep shared project-writing and command helpers in `tests/tool/tool_suite_common.*`
  until a narrower freestanding-only helper layer is justified.
