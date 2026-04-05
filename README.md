# Modern C

This repository now contains the bootstrap semantic checker and typed MIR bring-up for the first Modern C implementation.

What is in place:

- a repository layout that separates compiler, runtime, standard library, tests, examples, tools, and generated output
- a host build based on CMake that produces a working `mc` frontend executable
- a support layer for diagnostics, source-file loading, deterministic dump-path conventions, and AST/MIR dumps
- a handwritten lexer and parser that handle modules, declarations, types, statements, and expression precedence for the current v0.2 bootstrap subset
- bootstrap semantic checking for imports, visibility, local control-flow rules, and first-cut layout reporting
- typed MIR lowering, validation, and deterministic MIR fixture coverage for the currently supported semantic subset
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
- the admitted repository-bounded Lane B package-grouping slice with `examples/real/issue_rollup/`, proving one library-first hosted project layout with grouped internal module roots beyond one source root before the later static-library admission
- the admitted repository-bounded Phase 29 static-library slice with `examples/real/issue_rollup/`, proving one hosted `staticlib` target consumed by a thin executable target and by ordinary `mc test` workflow through the same archive boundary
- the admitted repository-bounded Phase 30 hardening slice with the same `examples/real/issue_rollup/` proof, now covering deterministic same-build-dir selected-target reuse across more than one executable consumer linked through the admitted static-library boundary
- parser, sema, MIR, and tool smoke tests wired through CTest and exposed through the same `make` command path used for local development

Supported hosted slice:

- supported compiler host and produced executable target: Darwin arm64 only
- supported runtime environment: hosted only
- supported direct-source commands: `mc check` and `mc build`
- supported project commands: `mc check`, `mc build`, `mc run`, and `mc test`
- admitted project target boundary: hosted `exe` targets, hosted checked-test targets, and one admitted in-project hosted `staticlib` target consumed by executable targets and ordinary tests
- admitted richer real-project proof set: `examples/real/issue_rollup/`, `examples/real/worker_queue/`, `examples/real/pipe_handoff/`, `examples/real/pipe_ready/`, and `examples/real/evented_echo/`, where `worker_queue` is the low-level shared-memory sync proof, `pipe_handoff` is the direct blocking handle-first communication proof, `pipe_ready` is the pipe-readiness proof over `io.poller_*`, and `evented_echo` remains the stronger networking proof over the same handle model
- supported workflow guarantee on the admitted richer proof: deterministic same-build-dir selected-target reuse without non-selected-target churn
- unsupported today: non-hosted targets, cross-compilation, shared libraries, external system-library links in project manifests, package management, and any public portability claim beyond Darwin arm64

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

Compiler entrypoint on the supported path:

```sh
build/debug/mc
```

First-use smoke validation:

```sh
build/debug/mc --help
build/debug/mc check tests/cases/hello.mc
build/debug/mc run --project examples/real/issue_rollup/build.toml --build-dir build/debug/phase32_issue_rollup -- examples/real/issue_rollup/tests/sample.txt
build/debug/mc test --project examples/real/issue_rollup/build.toml --build-dir build/debug/phase32_issue_rollup
```

Supported command patterns:

- direct-source inspection or build: `build/debug/mc check <file.mc>` and `build/debug/mc build <file.mc> --build-dir <dir>`
- project inspection, build, run, or test: `build/debug/mc check --project <build.toml>`, `build/debug/mc build --project <build.toml> --build-dir <dir>`, `build/debug/mc run --project <build.toml> --build-dir <dir> -- <args>`, and `build/debug/mc test --project <build.toml> --build-dir <dir>`

Artifact expectations:

- `build/debug/mc` is the produced compiler entrypoint for the supported path
- `build/` is disposable generated state
- command-local `--build-dir` trees are disposable generated state and should be treated as build outputs, not source inputs
- `make build` and `make test` are convenience wrappers around the same CMake-based path, not a separate supported installation story

Additional developer commands:

```sh
make build
make test
make dump-paths FILE=tests/cases/hello.mc
make check FILE=tests/cases/hello.mc

# direct cmake path
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
build/debug/mc check tests/cases/hello.mc --dump-ast --emit-dump-paths
build/debug/mc build tests/codegen/smoke_return_zero.mc --build-dir build/debug/stage5_smoke --dump-backend
build/debug/mc check tests/stdlib/hello_stdout.mc
build/debug/mc build tests/stdlib/hello_stdout.mc --build-dir build/debug/phase6_stdout
build/debug/mc check --project tests/tool/phase7_project/build.toml
build/debug/mc build --project tests/tool/phase7_project/build.toml --build-dir build/debug/phase7_project_smoke
build/debug/mc run --project tests/tool/phase7_project/build.toml --build-dir build/debug/phase7_project_smoke
build/debug/mc run --project examples/real/grep_lite/build.toml --build-dir build/debug/phase8_grep -- alpha examples/real/grep_lite/tests/sample.txt
build/debug/mc test --project examples/real/hash_tool/build.toml --build-dir build/debug/phase8_hash
build/debug/mc run --project examples/real/arena_expr/build.toml --build-dir build/debug/phase8_expr -- examples/real/arena_expr/tests/sample.expr
build/debug/mc run --project examples/real/worker_queue/build.toml --build-dir build/debug/phase20_worker_queue
build/debug/mc test --project examples/real/worker_queue/build.toml --build-dir build/debug/phase20_worker_queue
build/debug/mc run --project examples/real/pipe_handoff/build.toml --build-dir build/debug/phase43_pipe_handoff
build/debug/mc test --project examples/real/pipe_handoff/build.toml --build-dir build/debug/phase43_pipe_handoff
build/debug/mc run --project examples/real/pipe_ready/build.toml --build-dir build/debug/phase43_pipe_ready
build/debug/mc test --project examples/real/pipe_ready/build.toml --build-dir build/debug/phase43_pipe_ready
build/debug/mc run --project examples/real/evented_echo/build.toml --build-dir build/debug/phase13_evented_echo -- 4040
build/debug/mc test --project examples/real/evented_echo/build.toml --build-dir build/debug/phase13_evented_echo
build/debug/mc build --project examples/real/issue_rollup/build.toml --target issue-rollup-core --build-dir build/debug/phase29_issue_rollup
build/debug/mc run --project examples/real/issue_rollup/build.toml --build-dir build/debug/phase29_issue_rollup -- examples/real/issue_rollup/tests/sample.txt
build/debug/mc test --project examples/real/issue_rollup/build.toml --build-dir build/debug/phase29_issue_rollup
```

Checked versus debug posture:

- admitted real-project executable targets currently default to `mode = "debug"`
- admitted project ordinary tests currently run in `mode = "checked"`
- debug-mode proof is narrow: it proves the admitted executable-run workflow, not a broad release-mode certification across the whole language surface
- checked-mode proof is the main safety-validation surface for the current repository: `mc test` ordinary-test coverage is the strongest admitted workflow-trust proof on the supported slice

Public-cut smoke audit:

```sh
build/debug/mc check tests/cases/hello.mc
build/debug/mc build tests/codegen/smoke_return_zero.mc --build-dir build/debug/phase33_smoke_return_zero
build/debug/mc run --project examples/real/issue_rollup/build.toml --build-dir build/debug/phase33_issue_rollup -- examples/real/issue_rollup/tests/sample.txt
build/debug/mc test --project examples/real/issue_rollup/build.toml --build-dir build/debug/phase33_issue_rollup
build/debug/mc test --project examples/real/worker_queue/build.toml --build-dir build/debug/phase33_worker_queue
build/debug/mc test --project examples/real/pipe_handoff/build.toml --build-dir build/debug/phase43_pipe_handoff
build/debug/mc test --project examples/real/pipe_ready/build.toml --build-dir build/debug/phase43_pipe_ready
build/debug/mc test --project examples/real/evented_echo/build.toml --build-dir build/debug/phase33_evented_echo
```

First public-cut hosted statement:

- the current repository can honestly claim a hosted Darwin arm64 bootstrap compiler and toolchain slice with direct-source semantic check and executable build, project-mode debug executable runs, checked ordinary tests, grouped internal package layouts, one admitted in-project `staticlib`, deterministic selected-target same-build-dir reuse, and the admitted `issue_rollup`, `worker_queue`, `pipe_handoff`, `pipe_ready`, and `evented_echo` examples, with `worker_queue` scoped as the low-level shared-memory sync proof, `pipe_handoff` as the direct handle-first communication proof, and `pipe_ready` as the poller-coupled pipe-readiness proof
- it does not honestly claim broad release-mode certification across the full canonical-program surface, portability beyond Darwin arm64, package distribution, non-hosted targets, shared libraries, or broader linker-policy coverage

Notes:

- `build/` is disposable and ignored by Git.
- `make format` expects `clang-format` to be available on the host machine.
- The current `mc check` command runs the bootstrap frontend, semantic checker, and MIR pipeline and can emit deterministic AST, MIR, and backend dumps.
- The bootstrap `mc build` command now lowers one checked module through the LLVM backend, emits deterministic LLVM IR and object artifacts under `build/`, and links one hosted executable on Darwin arm64 through the ordinary host Clang toolchain.
- direct-source `mc check` and `mc build` discover `stdlib/` automatically for the admitted hosted Phase 6 slice.
- target-driven `mc check --project ...` and `mc build --project ...` now read a narrow `build.toml` schema v1 slice, resolve imports through ordered `search_paths.modules`, and emit deterministic `.mci` files under the active build directory.
- the current project-driven bootstrap slice now admits hosted executable targets plus one bounded hosted `staticlib` target kind; `mc run`, `mc test`, archive reuse, and interface-hash-driven project rebuild reuse are implemented, while non-default runtime startup, `sharedlib`, non-hosted targets, and broader linker surface remain out of the admitted bootstrap slice.
- the admitted repository-specific first-pass Phase 8 slice now includes deterministic canonical program proof and four bounded real utility projects under `examples/real/`; the follow-on hosted networking slice now additionally admits the bounded `examples/real/evented_echo/` project and its normal project workflow proof, the hosted `sync` slice now admits the shared-counter, producer-consumer, and atomic-publication executables plus the real `examples/real/worker_queue/` project as an explicit low-level shared-memory proof without widening into broader scheduler or atomic read-modify-write claims, and Phase 43 now adds the bounded `examples/real/pipe_handoff/` project as the direct handle-first communication proof over the admitted `io.pipe()` surface.
- the admitted project-workflow slice now also includes one grouped multi-root library-first proof through `examples/real/issue_rollup/`; Phase 29 narrows that proof further by admitting one hosted `staticlib` target consumed through ordinary `mc build`, `mc run`, and `mc test` workflow, and Phase 30 then hardens the same proof with deterministic selected-target non-churn across multiple executable consumers while keeping `sharedlib`, package management, and external library linking deferred.
- bootstrap `mc test` discovers `_test.mc` files under `tests.roots`, builds one deterministic runner per enabled target, executes tests serially, prints explicit target-scoped ordinary-test and compiler-regression scopes or verdicts, rejects direct-source invocation, and also supports a narrow compiler-regression manifest path for `check-pass`, `check-fail`, `run-output`, and `mir` cases.
- the repository-owned bootstrap ordinary test contract currently accepts `func() *T` or `func() Error`; `stdlib/testing.mc` now provides a still-bounded companion helper surface with `testing.fail()`, boolean expectations, typed integer equality checks, and string equality checks for ordinary project tests while the broader spec-level testing surface remains incomplete.
- imported stdlib values now use module-qualified access such as `io.write_line(...)`.
- the admitted hosted `io` surface now also includes `Pipe` plus `pipe()`, and the runtime `io.write_file(...)` path now uses ordinary descriptor writes so the same API composes honestly across sockets and pipe endpoints.
- imported user-defined types now support dotted imported type expressions such as `mem.Allocator`.
- the current standard-library boundary is still a bootstrap slice, not the full long-term Phase 6 surface from `docs/plan/plan.txt`.
- the current repository Phase 7 work now covers workstreams A through H for the admitted bootstrap slice, but the broader long-term architecture-specified Phase 7 surface remains open.
- the current repository Phase 8 work now covers the admitted first-pass closure described in `docs/plan/phase8_implementation_summary.txt`, but the broader long-term architecture-specified Phase 8 surface remains open.
- the current hosted release-hardening statement is recorded in `docs/plan/release_hardening_hosted_slice.txt`; keep that note and this README aligned whenever the supported slice changes.
- known limitations remain explicit: the supported host target is still Darwin arm64 only, the admitted networking surface remains narrow IPv4 TCP plus poller support rather than a broader async or portability claim, and the admitted hosted `sync` slice still excludes compare-exchange, fetch-add, condvar broadcast, schedulers, and broader portability claims.
- `make` is a thin convenience wrapper around the canonical CMake workflow above.
