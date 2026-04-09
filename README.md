# Modern C

This repository now contains the bootstrap semantic checker and typed MIR bring-up for the first Modern C implementation.

What is in place:

- a repository layout that separates compiler, runtime, standard library, tests, examples, tools, and generated output
- a host build based on CMake that produces a working `mc` frontend executable
- a support layer for diagnostics, source-file loading, deterministic dump-path conventions, and AST/MIR dumps
- a handwritten lexer and parser that handle modules, declarations, types, statements, and expression precedence for the current v0.2 bootstrap subset
- bootstrap semantic checking for imports, visibility, local control-flow rules, and first-cut layout reporting
- typed MIR lowering, validation, and deterministic MIR fixture coverage for the currently supported semantic subset
- the admitted v0.2 core language surface is now at a repository-bounded semantic plateau suitable for release hardening: current planned follow-on work is support clarity, workflow hardening, and adjacent-surface growth rather than a known missing parser/sema/MIR owner gap inside that admitted core
- bootstrap LLVM backend scaffolding for the frozen hosted Darwin arm64 target, with narrow backend preflight diagnostics for out-of-scope MIR
- a thin `mc` driver with direct-source `check` and `build`, project-driven `build.toml` target `check`, `build`, `run`, and `test`, `dump-paths`, and optional AST/MIR/backend dump emission for compiler inspection
- a bootstrap Phase 6 hosted slice with automatic repository-local stdlib discovery, imported-module executable builds, and explicit runtime support under `runtime/hosted/`
- the admitted repository-specific Phase 7 A/B/C/D/E/F/G/H slice with build.toml schema v1 parsing, target graph discovery, deterministic text `.mci` interface artifacts, interface-hash-driven incremental project rebuilds, separate per-module object emission, `mc run`, and `mc test`
- the admitted repository-specific first-pass Phase 8 slice with canonical pressure-program proof plus bounded real utility projects for text search, file walking, whole-file hashing, and arena-backed parsing
- the admitted repository-bounded follow-on networking slice with hosted `io` poller plus narrow `net`, direct-source evented server or client executable proof, the real `examples/real/evented_echo/` project fixture, and normal `mc run` or `mc test` workflow proof for that project
- the admitted repository-bounded hosted `sync` slice with typed `thread_spawn`, `thread_join`, mutex init or destroy or lock or unlock, condvar init or destroy or wait or signal, and narrow `MemoryOrder` plus `Atomic<T>` load or store support, proved by the direct-source shared-counter, producer-consumer, and atomic-publication canonical executables
- the admitted repository-bounded low-level shared-memory sync project proof with `examples/real/worker_queue/`, exercising one bounded producer or worker queue through the ordinary project `mc run` or `mc test` workflow on the admitted hosted `sync` surface without claiming that shared-memory coordination is the preferred portable communication model
- the admitted repository-bounded handle-first communication slice with minimal `io.Pipe` plus `io.pipe()`, hosted runtime support, and the real `examples/real/pipe_handoff/` project proving one bounded thread-plus-pipe communication path through the ordinary project `mc run` or `mc test` workflow without widening into channels or scheduler policy
- the admitted repository-bounded poller-coupled pipe readiness proof with `examples/real/pipe_ready/`, proving that the admitted `io.poller_*` surface composes honestly with `io.pipe()` endpoints for one bounded thread-plus-readiness workflow without widening into non-blocking pipe policy or scheduler semantics
- the admitted repository-bounded hosted `os` subprocess slice with narrow `stdlib/os.mc` `spawn_argv(...)`, additive `spawn_pipe_argv(...)`, additive `spawn_pipe_argv_merged_output(...)`, additive `spawn_pipe_argv_split_output(...)`, and `wait(...)`, the legacy fixed `spawn(...)` compatibility wrapper, explicit argv ownership, bounded pipe-backed stdin/stdout relay plus merged or split child-diagnostics capture, and the real `examples/real/line_filter_relay/` project proving one bounded child-process relay without shell semantics or broader process policy
- the admitted repository-bounded narrow `time` and `path` helper slice with `stdlib/time.mc` `Duration`, `monotonic()`, and slash-separated `path.join(...)`, `path.basename(...)`, and `path.dirname(...)`, intentionally stopping short of sleep, wall-clock, timezone, normalization, or broader platform APIs
- the admitted repository-bounded Lane B package-grouping slice with `examples/real/issue_rollup/`, proving one library-first hosted project layout with grouped internal module roots beyond one source root before the later static-library admission
- the admitted repository-bounded Phase 29 static-library slice with `examples/real/issue_rollup/`, proving one hosted `staticlib` target consumed through executable targets and ordinary `mc test` workflow through the same archive boundary
- the admitted repository-bounded Phase 30 plus Phase 74 hardening slice with the same `examples/real/issue_rollup/` proof, now covering deterministic same-build-dir selected-target reuse across the shipped executable consumers linked through the admitted static-library boundary
- the admitted repository-bounded freestanding proof slice with explicit `env = "freestanding"` executable targets, explicit target identity and startup selection, optional manifest-owned extra link inputs, narrow `hal` `mmio_ptr<T>`, `volatile_load<T>`, and `volatile_store<T>` support, one bounded endpoint-queue representation proof, one minimal task-lifecycle representation proof, one kernel static-data follow-through proof with ordinary top-level consts, globals, structs, and enums, one bounded kernel build-integration audit proving emitted objects compose with a minimal external relink step, one bounded init-to-log-service handshake proof for the first small user-space policy path, one bounded capability-handle transfer proof showing explicit user-space authority handoff and use, and one bounded ordinary helper-module proof showing repeated transfer and request scaffolding can stay in user space without a widened runtime surface, proved by the Phase 81 boot-marker, Phase 82 explicit link-input, Phase 83 polling-console, Phase 85 kernel queue, Phase 86 task lifecycle, Phase 87 static-data, Phase 88 manual-link, Phase 89 init-to-log-service, Phase 90 capability-transfer, and Phase 91 helper-boundary tool regressions without claiming broad freestanding packaging or cross-compilation
- parser, sema, MIR, and tool smoke tests wired through CTest and exposed through the same `make` command path used for local development

Supported hosted slice:

- supported compiler host and produced executable target: Darwin arm64 only
- supported runtime environment: hosted only
- supported direct-source commands: `mc check` and `mc build`
- supported project commands: `mc check`, `mc build`, `mc run`, and `mc test`
- admitted project target boundary: hosted `exe` targets, hosted checked-test targets, and one admitted in-project hosted `staticlib` target consumed by executable targets and ordinary tests
- admitted adjacent helper boundary: narrow hosted `time` `Duration` plus `monotonic()` and pure slash-separated `path` `join(...)`, `basename(...)`, and `dirname(...)`
- admitted richer real-project proof set: `examples/real/issue_rollup/`, `examples/real/worker_queue/`, `examples/real/pipe_handoff/`, `examples/real/pipe_ready/`, `examples/real/line_filter_relay/`, and `examples/real/evented_echo/`, where `worker_queue` is the low-level shared-memory sync proof, `pipe_handoff` is the direct blocking handle-first communication proof, `pipe_ready` is the pipe-readiness proof over `io.poller_*`, `line_filter_relay` is the narrow subprocess plus stdio proof over `os.spawn_pipe_argv(...)`, `os.spawn_pipe_argv_merged_output(...)`, `os.spawn_pipe_argv_split_output(...)`, and `os.wait(...)`, and `evented_echo` remains the stronger networking proof over the same handle model
- supported workflow guarantee on the admitted richer proof: deterministic same-build-dir selected-target reuse without non-selected-target churn, now exposed directly on the shipped `issue_rollup` project through its default and report executable consumers over the admitted static-library boundary
- unsupported today beyond this hosted support claim: the separate freestanding proof slice recorded below, general cross-compilation, shared libraries, external system-library links in project manifests, package management, and any public portability claim beyond Darwin arm64

Supported freestanding proof slice:

- supported compiler host and produced proof-artifact target family: Darwin arm64 bootstrap target family only
- admitted runtime environment: freestanding project `exe` targets with explicit `env = "freestanding"`
- admitted project command path: `mc build --project <build.toml> --target <name> --build-dir <dir>`
- admitted manifest boundary: freestanding `exe` targets with explicit `target`, explicit `[targets.<name>.runtime] startup`, and optional explicit `[targets.<name>.link] inputs = [...]`
- admitted public stdlib boundary: narrow `hal` `mmio_ptr<T>`, `volatile_load<T>`, and `volatile_store<T>`
- admitted proof set: Phase 81 boot-marker executable proof, Phase 82 explicit manifest-owned link-input proof, Phase 83 deterministic polling-console proof, Phase 85 kernel endpoint queue smoke proof, Phase 86 task lifecycle kernel proof, Phase 87 kernel static-data and artifact follow-through proof, Phase 88 kernel build integration audit, Phase 89 init-to-log-service handshake proof, Phase 90 capability-handle transfer proof, and Phase 91 early user-space helper-boundary audit
- unsupported today: `mc run` or `mc test` freestanding workflow admission, general cross-compilation, non-bootstrap target families, linker scripts, arbitrary linker flags, shared libraries, kernel-image packaging, and broader `hal` taxonomy

Supported host assumptions:

- host platform: Darwin arm64 only
- runtime boundary: hosted only
- installation story: build from source only
- toolchain assumption: CMake plus the ordinary host Clang toolchain

Build from source:

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j4
```

Repeat the narrow supported first-use path with:

```sh
make first-use-smoke
```

Repeat the broader current public-cut smoke audit with:

```sh
make public-cut-smoke
```

Repeat the current release-readiness audit with:

```sh
make release-readiness-audit
```

Repeat the current `v0.2` gate with:

```sh
make v0_2_gate
```

Repeat the freestanding support audit with:

```sh
make freestanding-support-audit
```

Prepare a reviewed `v0.2` release snapshot from clean committed state with:

```sh
make release-snapshot-prep
```

Compiler entrypoint on the supported path:

```sh
build/debug/mc
```

First-use smoke validation:

```sh
build/debug/mc --help
build/debug/mc check tests/cases/hello.mc
build/debug/mc run --project examples/real/issue_rollup/build.toml --build-dir build/debug/audit/first_use_smoke/issue_rollup -- examples/real/issue_rollup/tests/sample.txt
build/debug/mc test --project examples/real/issue_rollup/build.toml --build-dir build/debug/audit/first_use_smoke/issue_rollup
```

Supported command patterns:

- direct-source inspection or build: `build/debug/mc check <file.mc>` and `build/debug/mc build <file.mc> --build-dir <dir>`
- project inspection, build, run, or test: `build/debug/mc check --project <build.toml>`, `build/debug/mc build --project <build.toml> --build-dir <dir>`, `build/debug/mc run --project <build.toml> --build-dir <dir> -- <args>`, and `build/debug/mc test --project <build.toml> --build-dir <dir>`

Current module-model contract:

- ordinary modules are public by default
- `@private` is the only declaration-level hiding mechanism
- a file named `internal.mc` is package-internal and may be imported only by modules in the same explicit target `package`
- project manifests should set `package = "..."` on each target instead of relying on implicit package identity

Artifact expectations:

- `build/debug/mc` is the produced compiler entrypoint for the supported path
- `build/` is disposable generated state
- command-local `--build-dir` trees are disposable generated state and should be treated as build outputs, not source inputs
- repository-owned smoke and audit runs should prefer `build/debug/audit/...`
- manual probes should prefer `build/debug/probes/...`
- short-lived scratch runs should prefer `build/debug/tmp/...`
- the grouped tool regression suites now root disposable outputs under semantic suite directories such as `build/debug/tool/workflow/...`, `build/debug/tool/build_state/...`, `build/debug/tool/real_projects/...`, and `build/debug/tool/freestanding/...`
- the grouped codegen executable suites now keep disposable executable proof trees under nested semantic work roots such as `build/debug/stage5_exec_stdlib_tests/...` and `build/debug/stage5_exec_project_tests/...` rather than creating fresh top-level `phase*` trees
- remaining top-level `build/debug/phase*` entries are reserved for intentionally preserved manual or probe work, not active repository-owned regression outputs
- `make build` and `make test` are convenience wrappers around the same CMake-based path, not a separate supported installation story
- `make first-use-smoke` is the repo-owned convenience wrapper for the narrow documented first-use validation path, not a broader public-cut certification
- `make public-cut-smoke` is the repo-owned convenience wrapper for the broader current public-cut smoke audit on the admitted hosted slice
- `make release-readiness-audit` is the repo-owned convenience wrapper for the current aggregate release-readiness re-audit on the admitted hosted slice
- `make v0_2_gate` is the repo-owned convenience wrapper for the final documented `v0.2` gate outcome on the admitted hosted slice
- `make freestanding-support-audit` is the repo-owned convenience wrapper for the focused audit that keeps the published freestanding proof statement separate from the hosted `v0.2` note
- `make release-snapshot-prep` is the repo-owned convenience wrapper for the real maintainer release-snapshot preparation path after the final gate passes
- `build/debug/release/v0.2-snapshot.txt` is disposable generated state capturing the exact reviewed commit hash plus the suggested `git tag -a v0.2 ...` command for the current release snapshot candidate

Additional developer commands:

```sh
make build
make test
make first-use-smoke
make public-cut-smoke
make release-readiness-audit
make v0_2_gate
make freestanding-support-audit
make release-snapshot-prep
make dump-paths FILE=tests/cases/hello.mc
make check FILE=tests/cases/hello.mc

# direct cmake path
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
ctest --test-dir build/debug -R 'mc_tool_|mc_codegen_executable_(stdlib|project)_unit' --output-on-failure
build/debug/mc check tests/cases/hello.mc --dump-ast --emit-dump-paths
build/debug/mc build tests/codegen/smoke_return_zero.mc --build-dir build/debug/probes/smoke_return_zero --dump-backend
build/debug/mc check tests/stdlib/hello_stdout.mc
build/debug/mc build tests/stdlib/hello_stdout.mc --build-dir build/debug/probes/hello_stdout
build/debug/mc check --project tests/tool/phase7_project/build.toml
build/debug/mc build --project tests/tool/phase7_project/build.toml --build-dir build/debug/probes/phase7_project
build/debug/mc run --project tests/tool/phase7_project/build.toml --build-dir build/debug/probes/phase7_project
build/debug/mc run --project examples/real/grep_lite/build.toml --build-dir build/debug/probes/grep_lite -- alpha examples/real/grep_lite/tests/sample.txt
build/debug/mc test --project examples/real/hash_tool/build.toml --build-dir build/debug/probes/hash_tool
build/debug/mc run --project examples/real/arena_expr/build.toml --build-dir build/debug/probes/arena_expr -- examples/real/arena_expr/tests/sample.expr
build/debug/mc run --project examples/real/worker_queue/build.toml --build-dir build/debug/probes/worker_queue
build/debug/mc test --project examples/real/worker_queue/build.toml --build-dir build/debug/probes/worker_queue
build/debug/mc run --project examples/real/pipe_handoff/build.toml --build-dir build/debug/probes/pipe_handoff
build/debug/mc test --project examples/real/pipe_handoff/build.toml --build-dir build/debug/probes/pipe_handoff
build/debug/mc run --project examples/real/pipe_ready/build.toml --build-dir build/debug/probes/pipe_ready
build/debug/mc test --project examples/real/pipe_ready/build.toml --build-dir build/debug/probes/pipe_ready
build/debug/mc run --project examples/real/line_filter_relay/build.toml --build-dir build/debug/probes/line_filter_relay -- "phase forty five"
build/debug/mc test --project examples/real/line_filter_relay/build.toml --build-dir build/debug/probes/line_filter_relay
build/debug/mc run --project examples/real/evented_echo/build.toml --build-dir build/debug/probes/evented_echo -- 4040
build/debug/mc test --project examples/real/evented_echo/build.toml --build-dir build/debug/probes/evented_echo
build/debug/mc build --project examples/real/issue_rollup/build.toml --target issue-rollup-core --build-dir build/debug/probes/issue_rollup
build/debug/mc build --project examples/real/issue_rollup/build.toml --target issue-rollup-report --build-dir build/debug/probes/issue_rollup
build/debug/mc run --project examples/real/issue_rollup/build.toml --build-dir build/debug/probes/issue_rollup -- examples/real/issue_rollup/tests/sample.txt
build/debug/mc run --project examples/real/issue_rollup/build.toml --target issue-rollup-report --build-dir build/debug/probes/issue_rollup -- examples/real/issue_rollup/tests/sample.txt
build/debug/mc test --project examples/real/issue_rollup/build.toml --build-dir build/debug/probes/issue_rollup
```

Grouped regression targets:

- tool workflow validation: `mc_tool_workflow_unit`
- tool build-state and incremental rebuild validation: `mc_tool_build_state_unit`
- tool real-project workflow validation: `mc_tool_real_project_unit`
- tool freestanding proof validation: `mc_tool_freestanding_unit`
- codegen stdlib executable validation: `mc_codegen_executable_stdlib_unit`
- codegen project executable validation: `mc_codegen_executable_project_unit`

Checked versus debug posture:

- admitted real-project executable targets currently default to `mode = "debug"`
- admitted project ordinary tests currently run in `mode = "checked"`
- debug-mode proof is narrow: it proves the admitted executable-run workflow, not a broad release-mode certification across the whole language surface
- checked-mode proof is the main safety-validation surface for the current repository: `mc test` ordinary-test coverage is the strongest admitted workflow-trust proof on the supported slice

Public-cut smoke audit:

Repeat the repo-owned public-cut smoke path with:

```sh
make public-cut-smoke
```

```sh
build/debug/mc check tests/cases/hello.mc
build/debug/mc build tests/codegen/smoke_return_zero.mc --build-dir build/debug/audit/public_cut_smoke/smoke_return_zero
build/debug/mc run --project examples/real/issue_rollup/build.toml --build-dir build/debug/audit/public_cut_smoke/issue_rollup -- examples/real/issue_rollup/tests/sample.txt
build/debug/mc run --project examples/real/issue_rollup/build.toml --target issue-rollup-report --build-dir build/debug/audit/public_cut_smoke/issue_rollup -- examples/real/issue_rollup/tests/sample.txt
build/debug/mc test --project examples/real/issue_rollup/build.toml --build-dir build/debug/audit/public_cut_smoke/issue_rollup
build/debug/mc test --project examples/real/worker_queue/build.toml --build-dir build/debug/audit/public_cut_smoke/worker_queue
build/debug/mc test --project examples/real/pipe_handoff/build.toml --build-dir build/debug/audit/public_cut_smoke/pipe_handoff
build/debug/mc test --project examples/real/pipe_ready/build.toml --build-dir build/debug/audit/public_cut_smoke/pipe_ready
build/debug/mc test --project examples/real/line_filter_relay/build.toml --build-dir build/debug/audit/public_cut_smoke/line_filter_relay
build/debug/mc test --project examples/real/evented_echo/build.toml --build-dir build/debug/audit/public_cut_smoke/evented_echo
```

First public-cut hosted statement:

- the current repository can honestly claim a hosted Darwin arm64 bootstrap compiler and toolchain slice with direct-source semantic check and executable build, project-mode debug executable runs, checked ordinary tests, grouped internal package layouts, one admitted in-project `staticlib`, deterministic selected-target same-build-dir reuse without non-selected-target churn on the admitted real `issue_rollup` project proof, the narrow hosted `time` and pure slash-separated `path` helper slice, and the admitted `issue_rollup`, `worker_queue`, `pipe_handoff`, `pipe_ready`, `line_filter_relay`, and `evented_echo` examples, with `worker_queue` scoped as the low-level shared-memory sync proof, `pipe_handoff` as the direct handle-first communication proof, `pipe_ready` as the poller-coupled pipe-readiness proof, and `line_filter_relay` as the narrow hosted subprocess plus stdio proof
- it does not honestly claim broad release-mode certification across the full canonical-program surface, portability beyond Darwin arm64, package distribution, non-hosted targets, shared libraries, or broader linker-policy coverage

Release-readiness re-audit:

- the remaining gaps after the current support freeze, first-use smoke, supported-slice doc audit, and public-cut smoke refresh are now explicitly outside the admitted source-only Darwin arm64 hosted claim rather than hidden breaks inside it
- those remaining gaps are narrow enough to tolerate in a source-only `v0.2` release cut if the repository chooses to tag at that boundary, but Phase 79 still owns the final tag-or-defer decision

`v0.2` gate outcome:

- the repository does not create a `v0.2` tag in-tree at this point
- the one remaining plateau that still blocks that tag is maintainer-owned release execution: cutting the final reviewed commit snapshot and applying the tag from that committed state rather than from a live working tree
- within the repository itself, the technical gate is otherwise satisfied by the current first-use smoke, supported-slice doc audit, public-cut smoke, and aggregate release-readiness audit

Notes:

- `build/` is disposable and ignored by Git.
- `make format` expects `clang-format` to be available on the host machine.
- The current `mc check` command runs the bootstrap frontend, semantic checker, and MIR pipeline and can emit deterministic AST, MIR, and backend dumps.
- The bootstrap `mc build` command now lowers one checked module through the LLVM backend, emits deterministic LLVM IR and object artifacts under `build/`, links one hosted executable on Darwin arm64 through the ordinary host Clang toolchain, and also carries one separate repo-bounded freestanding executable proof path on the same bootstrap target family when the manifest uses explicit freestanding target and startup settings.
- direct-source `mc check` and `mc build` discover `stdlib/` automatically for the admitted hosted Phase 6 slice.
- target-driven `mc check --project ...` and `mc build --project ...` now read a narrow `build.toml` schema v1 slice, resolve imports through ordered `search_paths.modules`, and emit deterministic `.mci` files under the active build directory.
- the current project-driven bootstrap slice now admits hosted executable targets plus one bounded hosted `staticlib` target kind on the hosted claim, and also admits one separate repo-bounded freestanding executable proof slice with explicit `target`, explicit `runtime.startup`, optional `link.inputs`, and narrow `hal` coverage; `mc run` and `mc test` remain hosted-only, while `sharedlib` and broader linker surface remain out of the admitted bootstrap slice.
- the admitted repository-specific first-pass Phase 8 slice now includes deterministic canonical program proof and four bounded real utility projects under `examples/real/`; the follow-on hosted networking slice now additionally admits the bounded `examples/real/evented_echo/` project and its normal project workflow proof, the hosted `sync` slice now admits the shared-counter, producer-consumer, and atomic-publication executables plus the real `examples/real/worker_queue/` project as an explicit low-level shared-memory proof without widening into broader scheduler or atomic read-modify-write claims, Phase 43 adds the bounded `examples/real/pipe_handoff/` project as the direct handle-first communication proof over the admitted `io.pipe()` surface, and Phase 45/46 now add the bounded `examples/real/line_filter_relay/` project as the real consumer proof for the admitted narrow hosted subprocess slice.
- the repository also admits one narrow hosted `time` and `path` helper boundary: `time.Duration`, `time.monotonic()`, and pure slash-separated `path.join(...)`, `path.basename(...)`, and `path.dirname(...)`; sleep, wall-clock, timezone, normalization, and broader platform APIs remain deferred.
- the admitted project-workflow slice now also includes one grouped multi-root library-first proof through `examples/real/issue_rollup/`; Phase 29 narrows that proof further by admitting one hosted `staticlib` target consumed through ordinary `mc build`, `mc run`, and `mc test` workflow, Phase 30 hardens the target-switching contract, and Phase 74 now exposes that same multi-target proof directly on the shipped project rather than only through a cloned regression harness while keeping `sharedlib`, package management, and external library linking deferred.
- bootstrap `mc test` discovers `_test.mc` files under `tests.roots`, builds one deterministic runner per enabled target, executes tests serially, prints explicit target-scoped ordinary-test and compiler-regression scopes or verdicts, rejects direct-source invocation, and also supports a narrow compiler-regression manifest path for `check-pass`, `check-fail`, `run-output`, and `mir` cases.
- the repository-owned bootstrap ordinary test contract currently accepts `func() *T` or `func() Error`; `stdlib/testing.mc` now provides a still-bounded companion helper surface with `testing.fail()`, boolean expectations, typed integer equality checks, and string equality checks for ordinary project tests while the broader spec-level testing surface remains incomplete.
- imported stdlib values now use module-qualified access such as `io.write_line(...)`.
- the admitted hosted `io` surface now also includes `Pipe` plus `pipe()`, and the runtime `io.write_file(...)` path now uses ordinary descriptor writes so the same API composes honestly across sockets and pipe endpoints.
- the admitted hosted `os` surface now includes explicit argv-slice `spawn_argv(...)`, additive `spawn_pipe_argv(...)` for the admitted one-input-pipe plus one-output-pipe relay shape, additive `spawn_pipe_argv_merged_output(...)` for bounded merged child diagnostics, additive `spawn_pipe_argv_split_output(...)` for bounded separate stdout and stderr capture on that same relay family, the legacy fixed `spawn(...)` compatibility wrapper, and blocking `wait(...)`; shell command strings, broader stderr routing policy beyond the admitted split helper, environment APIs, cwd mutation, signals, and broader process management remain deferred.
- imported user-defined types now support dotted imported type expressions such as `mem.Allocator`.
- the current standard-library boundary is still a bootstrap slice, not the full long-term Phase 6 surface from `docs/plan/plan.txt`.
- the current repository Phase 7 work now covers workstreams A through H for the admitted bootstrap slice, but the broader long-term architecture-specified Phase 7 surface remains open.
- the current repository Phase 8 work now covers the admitted first-pass closure described in `docs/plan/phase8_implementation_summary.txt`, but the broader long-term architecture-specified Phase 8 surface remains open.
- `docs/plan/phase65_language_surface_plateau_decision.txt` now records that the admitted v0.2 core is semantically complete enough for repository-bounded release hardening; remaining work should be read as support hardening, portability, or adjacent-surface follow-through unless a new admitted core-language owner gap is found.
- the current hosted release-hardening statement is recorded in `docs/plan/release_hardening_hosted_slice.txt`; keep that note and this README aligned whenever the supported slice changes.
- the separate freestanding proof statement is recorded in `docs/plan/freestanding_support_statement.txt`; keep that note, the hosted note, and this README aligned whenever the admitted freestanding proof slice changes.
- the documented supported first-use path is now also automated through `make first-use-smoke` and the `mc_first_use_smoke` CTest entry; keep those aligned with the written install-and-usage guidance.
- the documented current public-cut smoke path is now also automated through `make public-cut-smoke` and the `mc_public_cut_smoke` CTest entry; keep those aligned with the written public-cut audit commands.
- the documented current release-readiness re-audit is now also automated through `make release-readiness-audit` and the `mc_release_readiness_audit` CTest entry; keep those aligned with the written release-readiness statement.
- the documented current `v0.2` gate is now also automated through `make v0_2_gate` and the `mc_v0_2_gate` CTest entry; keep those aligned with the written tag-or-defer outcome.
- the documented freestanding support split is now also automated through `make freestanding-support-audit` and the `mc_freestanding_support_audit` CTest entry; keep those aligned with the published freestanding proof statement.
- the documented release-snapshot preparation path is now also automated through `make release-snapshot-prep`, validated through the `mc_release_snapshot_prep` audit entry, and writes `build/debug/release/v0.2-snapshot.txt`; keep those aligned with the written maintainer release-execution path.
- the shared grouped tool implementation source now lives at `tests/tool/tool_suite_tests.cpp`; `tests/tool/phase7_tool_tests.cpp` remains only as a compatibility shim for older references in archived notes.
- known limitations remain explicit: the supported host target is still Darwin arm64 only, the admitted freestanding slice is still only a repository-bounded proof path rather than a general non-hosted workflow claim, the admitted networking surface remains narrow IPv4 TCP plus poller support rather than a broader async or portability claim, the admitted hosted `sync` slice still excludes public compare-exchange, exchange, fetch-add, schedulers, and broader portability claims even though `condvar_broadcast(...)` is implemented, and the admitted `time` plus `path` helper slice still stops short of sleep, wall-clock, timezone, normalization, or broader platform APIs.
- `make` is a thin convenience wrapper around the canonical CMake workflow above.
