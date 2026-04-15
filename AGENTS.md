# AGENTS

This repository is a bootstrap Modern C compiler. Treat it as a staged compiler bring-up, not a finished implementation.

## Current Status

- Parser, AST, bootstrap semantic checking, and typed MIR all exist.
- The repository has completed a bootstrap Phase 3 milestone, not the full Phase 3 defined in `docs/plan/plan.txt`.
- Phase 4 MIR work is already active.
- Do not claim “Phase 3 is complete” unless the full `plan.txt` exit criteria are satisfied.

See these files first:

- `README.md`
- `docs/plan/admin/bootstrap_plan.txt`
- `docs/plan/archive/phase3_bootstrap_finish.txt`
- `docs/agent/prompts/repo_map.md`
- `docs/arch/veya/kernel_style_guide.txt` for kernel/src/ service shape, naming, and main.mc limits

Documentation routing:

- `docs/arch/open_decisions.txt`: unresolved language, architecture, or packaging questions only
- `docs/arch/key_decisions.txt`: settled design choices with short rationale
- `docs/plan/backlog.txt`: implementation themes, deferred tasks, and recurring follow-up work
- `docs/plan/decision_log.txt`: intentional limited solutions, blockers, and deferred fuller fixes
- `docs/plan/*phase*.txt`: phase-scoped milestones, summaries, and closeouts

## Working Rules

- Preserve the phase boundaries: parser builds syntax, sema owns language meaning, MIR owns typed lowered control flow.
- Prefer minimal, targeted changes. Do not refactor unrelated code while doing feature work.
- Keep semantic and MIR dumps deterministic. If behavior changes intentionally, update the relevant fixture goldens.
- Do not paper over missing semantics by pushing logic into MIR or the driver when it belongs in sema.
- Do not mark incomplete bootstrap behavior as spec-complete.
- Respect the existing repository style: straightforward C++20, small helper functions, deterministic tests.

## Validation

Canonical local workflow:

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j4
ctest --test-dir build/debug --output-on-failure
```

Explicit parallel full-suite path:

```sh
ctest --test-dir build/debug -j4 --output-on-failure
```

Useful focused commands:

```sh
build/debug/bin/mc --help
build/debug/bin/mc check tests/cases/hello.mc
build/debug/bin/mc check tests/cases/hello.mc --dump-mir --build-dir build/debug
ctest --test-dir build/debug -R mc_sema_fixture_unit --output-on-failure
ctest --test-dir build/debug -R mc_mir_fixture_unit --output-on-failure
ctest --test-dir build/debug -R mc_tool_workflow_unit --output-on-failure
ctest --test-dir build/debug -R mc_tool_build_state_unit --output-on-failure
ctest --test-dir build/debug -R mc_tool_real_project_unit --output-on-failure
make select-tests
```

## Testing Conventions

- `tests/compiler/parser`: parser fixtures and parse-fail diagnostics
- `tests/compiler/sema`: semantic module dumps and semantic-fail diagnostics
- `tests/compiler/mir`: MIR dumps and lowering failures
- `tests/tool`: driver and support-layer smoke/unit tests

Active grouped regression layout:

- `tests/tool/tool_suite_common.cpp`: shared grouped tool helpers
- `tests/tool/tool_workflow_tests.cpp`: workflow validation driver
- `tests/tool/tool_workflow_orchestrator.cpp`: thin workflow suite entry that dispatches the behavior-owned workflow families
- `tests/tool/tool_help_suite.cpp`: help text, mode selection, and direct-source versus project workflow validation
- `tests/tool/tool_test_command_suite.cpp`: ordinary test and `mc test` workflow validation
- `tests/tool/tool_project_validation_suite.cpp`: target selection, import-root, duplicate-root, and project-graph validation
- `tests/tool/tool_multifile_module_suite.cpp`: module-set and multi-file module validation
- `tests/tool/tool_kernel_reset_lane_suite.cpp`: reset-lane kernel workflow validation and table-driven fixture runs
- `tests/tool/tool_build_state_tests.cpp`: build-state, imported-artifact, and incremental rebuild driver
- `tests/tool/tool_build_state_suite.cpp`: build-state grouped implementation
- `tests/tool/tool_real_project_tests.cpp`: real-project workflow driver
- `tests/tool/tool_real_project_suite.cpp`: real-project grouped implementation
- local changed-path selection now lives in `tools/select_tests.py` and the `make select-tests` wrapper; use that for narrow local iteration before wider gates
- `kernel/build.toml`: repository-owned hosted reset-lane project manifest used by the active workflow suite
- `archive/legacy_freestanding/`: retired freestanding proof harness, `kernel_old/`, and related maintenance helpers preserved for historical reference only
- `tests/tool/tool_suite_tests.cpp` and `tests/tool/phase7_tool_tests.cpp`: compatibility runners only, not the active implementation owners
- `tests/compiler/codegen/codegen_executable_tests.cpp`: shared grouped codegen executable implementation
- primary build products now belong under `build/debug/bin/...` and `build/debug/lib/...`
- grouped tool outputs now belong under semantic suite roots such as `build/debug/tool/workflow/...`
- grouped codegen executable outputs now belong under semantic suite roots such as `build/debug/codegen/executable/core/...`, `build/debug/codegen/executable/stdlib/...`, and `build/debug/codegen/executable/project/...`
- treat remaining top-level `build/debug/phase*` paths as preserved manual or probe areas unless you have explicit reason to clean them

Diagnostic fixture convention:

- `.errors.txt` stores one expected diagnostic substring per line
- tests match substrings, not full formatted lines
- freestanding late-kernel audit convention: `.mirproj.txt` stores the expected projected MIR lines consumed by `ExpectMirFirstMatchProjectionFile`

## Editing Guidance By Area

- `compiler/driver`: CLI parsing and pipeline orchestration only
- `compiler/lex`, `compiler/parse`, `compiler/ast`: syntax layer only
- `compiler/sema`: name resolution, type checking, layout, semantic module
- `compiler/mir`: typed IR, lowering from sema, MIR validation and dumps
- `compiler/support`: diagnostics, source loading, dump-path helpers

If you add a feature that changes compiler-visible behavior, update the narrowest relevant test layer first.

## Common Pitfalls

- `docs/plan/phase3_bootstrap_finish.txt` is a repository-specific milestone note, not the full language plan.
- Do not leave resolved questions in `docs/arch/open_decisions.txt`; move them to the appropriate spec, key decisions, backlog, or decision log file.
- Do not use `docs/plan/backlog.txt` as a replacement for `docs/arch/open_decisions.txt`; backlog is for work items, not unresolved semantics.
- Do not use `docs/plan/decision_log.txt` for general status updates; it is only for deliberate limited solutions and the reason they were chosen.
- Import handling currently lives in bootstrap sema and is intentionally simpler than a future interface-artifact system.
- Layout support exists, but it is still first-cut bootstrap behavior and not full ABI completion.
- The build tree under `build/` is disposable.
- Do not reactivate or extend the archived freestanding proof harness under `archive/legacy_freestanding/`; new validation belongs on the active hosted kernel or grouped workflow surfaces.
- Do not reintroduce fresh top-level `build/debug/phase*` regression output trees for repository-owned tests; prefer the semantic `audit`, `probes`, `tmp`, `tool`, or grouped suite work roots already in use.
- If you need to tidy an existing build tree in place, prefer `make clean-legacy-build-debris` over ad hoc manual deletion.
