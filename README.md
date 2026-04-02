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
- parser, sema, MIR, and tool smoke tests wired through CTest and exposed through the same `make` command path used for local development

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
```

Notes:

- `build/` is disposable and ignored by Git.
- `make format` expects `clang-format` to be available on the host machine.
- The current `mc check` command runs the bootstrap frontend, semantic checker, and MIR pipeline and can emit deterministic AST, MIR, and backend dumps.
- The bootstrap `mc build` command now lowers one checked module through the LLVM backend, emits deterministic LLVM IR and object artifacts under `build/`, and links one hosted executable on Darwin arm64 through the ordinary host Clang toolchain.
- direct-source `mc check` and `mc build` discover `stdlib/` automatically for the admitted hosted Phase 6 slice.
- target-driven `mc check --project ...` and `mc build --project ...` now read a narrow `build.toml` schema v1 slice, resolve imports through ordered `search_paths.modules`, and emit deterministic `.mci` files under the active build directory.
- the current project-driven Phase 7 slice admits hosted executable targets only; `mc run`, `mc test`, and interface-hash-driven project rebuild reuse are implemented, while non-default runtime startup, non-hosted targets, and non-executable target kinds remain out of the admitted bootstrap slice.
- bootstrap `mc test` discovers `_test.mc` files under `tests.roots`, builds one deterministic runner per enabled target, executes tests serially, and also supports a narrow compiler-regression manifest path for `check-pass`, `check-fail`, `run-output`, and `mir` cases.
- the repository-owned bootstrap ordinary test contract currently accepts `func() *T` or `func() Error`; `stdlib/testing.mc` provides `testing.fail()` for the current failure path until the broader spec-level testing surface is complete.
- imported stdlib values now use module-qualified access such as `io.write_line(...)`.
- imported user-defined types now support dotted imported type expressions such as `mem.Allocator`.
- the current standard-library boundary is still a bootstrap slice, not the full long-term Phase 6 surface from `docs/plan/plan.txt`.
- the current repository Phase 7 work now covers workstreams A through H for the admitted bootstrap slice, but the broader long-term architecture-specified Phase 7 surface remains open.
- `make` is a thin convenience wrapper around the canonical CMake workflow above.
