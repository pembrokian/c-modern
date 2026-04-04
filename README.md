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
- the admitted repository-bounded first hosted `sync` slice with `thread_spawn`, `thread_join`, and mutex init or lock or unlock, proved by the direct-source shared-counter canonical executable
- parser, sema, MIR, and tool smoke tests wired through CTest and exposed through the same `make` command path used for local development

Supported hosted slice:

- supported compiler host and executable target: Darwin arm64
- supported runtime environment: hosted only
- supported project workflow: direct-source `check` or `build`, hosted project `build`, `run`, and `test` for executable and checked-test targets
- unsupported today: non-hosted targets, cross-compilation, static or shared libraries, package management, and any public portability claim beyond Darwin arm64

Build from source:

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j4
ctest --test-dir build/debug --output-on-failure
```

Compiler entrypoint:

```sh
build/debug/mc
```

Bootstrap commands:

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
build/debug/mc run --project examples/real/evented_echo/build.toml --build-dir build/debug/phase13_evented_echo -- 4040
build/debug/mc test --project examples/real/evented_echo/build.toml --build-dir build/debug/phase13_evented_echo
```

Checked versus release note:

- admitted real-project executable targets currently default to `mode = "debug"`
- admitted project ordinary tests currently run in `mode = "checked"`
- the repository currently proves checked-mode behavior most strongly through `mc test` ordinary-test coverage and focused executable regressions; it does not yet claim a broad release-mode audit across the entire canonical-program surface

Notes:

- `build/` is disposable and ignored by Git.
- `make format` expects `clang-format` to be available on the host machine.
- The current `mc check` command runs the bootstrap frontend, semantic checker, and MIR pipeline and can emit deterministic AST, MIR, and backend dumps.
- The bootstrap `mc build` command now lowers one checked module through the LLVM backend, emits deterministic LLVM IR and object artifacts under `build/`, and links one hosted executable on Darwin arm64 through the ordinary host Clang toolchain.
- direct-source `mc check` and `mc build` discover `stdlib/` automatically for the admitted hosted Phase 6 slice.
- target-driven `mc check --project ...` and `mc build --project ...` now read a narrow `build.toml` schema v1 slice, resolve imports through ordered `search_paths.modules`, and emit deterministic `.mci` files under the active build directory.
- the current project-driven Phase 7 slice admits hosted executable targets only; `mc run`, `mc test`, and interface-hash-driven project rebuild reuse are implemented, while non-default runtime startup, non-hosted targets, and non-executable target kinds remain out of the admitted bootstrap slice.
- the admitted repository-specific first-pass Phase 8 slice now includes deterministic canonical program proof and four bounded real utility projects under `examples/real/`; the follow-on hosted networking slice now additionally admits the bounded `examples/real/evented_echo/` project and its normal project workflow proof, and the first hosted `sync` slice now admits the shared-counter mutex executable proof, while condvar, atomic, and hardware-boundary pressure remain explicitly deferred.
- bootstrap `mc test` discovers `_test.mc` files under `tests.roots`, builds one deterministic runner per enabled target, executes tests serially, and also supports a narrow compiler-regression manifest path for `check-pass`, `check-fail`, `run-output`, and `mir` cases.
- the repository-owned bootstrap ordinary test contract currently accepts `func() *T` or `func() Error`; `stdlib/testing.mc` provides `testing.fail()` for the current failure path until the broader spec-level testing surface is complete.
- imported stdlib values now use module-qualified access such as `io.write_line(...)`.
- imported user-defined types now support dotted imported type expressions such as `mem.Allocator`.
- the current standard-library boundary is still a bootstrap slice, not the full long-term Phase 6 surface from `docs/plan/plan.txt`.
- the current repository Phase 7 work now covers workstreams A through H for the admitted bootstrap slice, but the broader long-term architecture-specified Phase 7 surface remains open.
- the current repository Phase 8 work now covers the admitted first-pass closure described in `docs/plan/phase8_implementation_summary.txt`, but the broader long-term architecture-specified Phase 8 surface remains open.
- the current hosted release-hardening statement is recorded in `docs/plan/release_hardening_hosted_slice.txt`; keep that note and this README aligned whenever the supported slice changes.
- known limitations remain explicit: the supported host target is still Darwin arm64 only, the project-mode evented echo example still uses local hosted ABI declarations as a bootstrap workaround, the admitted networking surface remains narrow IPv4 TCP plus poller support rather than a broader async or portability claim, and the admitted hosted `sync` slice currently stops at thread spawn or join plus mutex without condvar, atomic, or mutex-destroy support.
- `make` is a thin convenience wrapper around the canonical CMake workflow above.
