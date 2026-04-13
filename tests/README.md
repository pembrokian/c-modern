# Test Layout

The test tree now has two different roles and they should stay separate:

- behavior-first tests for whether the system works
- compiler-validation tests for parser, sema, MIR, codegen, and driver internals

Behavior-first owners:

- `smoke/` for tiny pass-fail checks such as build, hello, IPC round trip, serial basic flow, and shell boot
- `system/` for direct service and multi-service behavior such as serial ingress, IPC composition, echo, log, and key-value flows

Compiler-validation owners:

- `compiler/parser/` for parser success dumps and parse-fail diagnostics
- `compiler/sema/` for semantic module dumps, semantic-fail diagnostics, and import or package fixtures
- `compiler/mir/` for MIR dumps, lowering-fail diagnostics, and canonical-example sync checks
- `compiler/codegen/` for deterministic backend dumps and executable behavior tests
- `stdlib/` for checked-in stdlib proof programs
- `tool/` for `mc` driver, manifest, rebuild, workflow tests, and the temporary reset-lane runners used while `smoke/` and `system/` coverage is being established
- `cases/` for small shared direct-source smoke fixtures
- `cmake/` for repository-owned CTest audit helpers
- `support/` for local reusable test harness utilities

Legacy rule during Phase 152c:

- do not expand phase-driven runtime-proof suites such as `tests/tool/freestanding/kernel/runtime/phase*` unless a narrow legacy maintenance fix forces it
- treat those proof-heavy runtime suites as read-only legacy surfaces while new cleanup checks move toward `smoke/` and `system/`

The shared support layer currently covers fixture helpers plus the common
process, socket, timeout, and temporary-file helpers used by grouped
integration suites.

## Fixture Conventions

Adjacent expectation files use stable suffixes:

- `<name>.mc` is the source fixture
- `<name>.ast.txt` is the expected AST dump
- `<name>.sema.txt` is the expected semantic module dump
- `<name>.mir.txt` is the expected MIR dump
- `<name>.backend.txt` is the expected backend dump
- `<name>.errors.txt` stores expected diagnostic substrings, one per line

Diagnostic expectation rules:

- each non-empty line in `.errors.txt` must appear somewhere in rendered diagnostics
- substring matching is intentional so fixtures stay stable across path and formatting drift
- malformed-literal lexer failures can use the same parser fixture path because lex errors propagate through the parser runner

## Discovery Rules

Routine fixture discovery is intentionally small and explicit:

- `tests/compiler/parser/` is autodiscovered by adjacent `.ast.txt` or `.errors.txt` files
- `tests/compiler/codegen/` backend dump fixtures are autodiscovered by adjacent `.backend.txt` files
- `tests/compiler/sema/` and `tests/compiler/mir/` use a hybrid model: ordinary adjacent `.sema.txt`/`.mir.txt` and `.errors.txt` fixtures are autodiscovered, while explicit runner metadata remains only for import roots, imported modules, canonical-example sync, or package-identity overrides

Contributor rule of thumb:

- a routine parser or backend dump fixture usually only needs new adjacent files
- a routine sema or MIR fixture with adjacent expectations usually only needs new files
- if a fixture needs import roots, imported helper modules, or package metadata, keep that metadata explicit rather than hiding it behind discovery magic

## Build Output Rules

Checked-in files under `tests/` are source inputs and expectations only.

Generated outputs belong under the active build tree:

- repository-owned smoke and audit runs should prefer `build/debug/audit/...`
- manual test probes should prefer `build/debug/probes/...`
- short-lived scratch runs should prefer `build/debug/tmp/...`

Do not treat `tests/` as a home for generated runners, executables, object files, or command-local build trees.

## Integration Targets

The large integration areas are now split by behavior family instead of one executable per domain:

- tool workflow and CLI/project validation
- tool build-state, imported-artifact, and incremental rebuild validation
- tool real-project workflow validation
- tool freestanding proof validation
- codegen core executable behavior
- codegen stdlib and canonical executable behavior
- codegen project and imported-module executable behavior

The active grouped tool CTest targets use semantic names:

- `mc_tool_workflow_unit`
- `mc_tool_build_state_unit`
- `mc_tool_real_project_unit`
- `mc_tool_freestanding_bootstrap_unit`
- `mc_tool_freestanding_kernel_runtime_unit`
- `mc_tool_freestanding_kernel_docs_unit`
- `mc_tool_freestanding_kernel_artifacts_unit`
- `mc_tool_freestanding_system_unit`

The freestanding kernel workflow now routes through the top-level runtime,
docs, and artifacts surfaces. Any retained shard CTests are legacy
implementation details rather than normal ownership targets.

During the Phase 152c reset, those retained runtime proof targets are not the
destination for new behavior-first cleanup work. Use the simplest possible
workflow runner for new smokes and system checks until those surfaces can own
their own direct runners.

When those grouped tool suites generate disposable outputs, they now root them
under semantic suite directories such as `build/debug/tool/workflow/...`,
`build/debug/tool/build_state/...`, `build/debug/tool/real_projects/...`, and
`build/debug/tool/freestanding/...` instead of creating fresh top-level
`phase*` trees. The leaf project, build, and output names under those suite
roots are also semantic now, so routine runs no longer create new disposable
`phase*` artifacts anywhere under the active build root.

The grouped codegen executable suites were audited as part of the same cleanup.
They now root their disposable outputs under semantic suite directories such as
`build/debug/codegen/executable/core/...`,
`build/debug/codegen/executable/stdlib/...`, and
`build/debug/codegen/executable/project/...`, and they do not create fresh
top-level `build/debug/phase*` trees.

## Hosted Assumptions

The grouped tool and executable codegen suites currently assume a hosted
POSIX-like development environment.

- they use fork/exec child-process control for background runs and timeout checks
- they use loopback sockets for networked echo and partial-write fixtures
- they use temporary project trees under the active build directory

No extra CMake host gating is currently applied because the admitted bootstrap
host remains POSIX-like. If that host matrix broadens, these suites should be
the first place to add explicit gating rather than implying wider portability.

## Local Selection Helper

For local iteration, `tools/select_tests.py` can map changed paths to the
owning CTest targets and optionally run an incremental build plus only that
selected test subset. By default it compares the working tree against the
current branch merge-base with `main` or `master`, and it also supports an
optional per-target fingerprint cache for repeated local reruns.

The repository also exposes `make select-tests` as the low-friction wrapper
for this workflow.
