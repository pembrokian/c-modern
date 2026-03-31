# Modern C

This repository now contains the bootstrap semantic checker and typed MIR bring-up for the first Modern C implementation.

What is in place:

- a repository layout that separates compiler, runtime, standard library, tests, examples, tools, and generated output
- a host build based on CMake that produces a working `mc` frontend executable
- a support layer for diagnostics, source-file loading, deterministic dump-path conventions, and AST/MIR dumps
- a handwritten lexer and parser that handle modules, declarations, types, statements, and expression precedence for the current v0.2 bootstrap subset
- bootstrap semantic checking for imports, visibility, local control-flow rules, and first-cut layout reporting
- typed MIR lowering, validation, and deterministic MIR fixture coverage for the currently supported semantic subset
- a thin `mc` driver with `check`, `dump-paths`, `--dump-ast`, and `--dump-mir` for compiler inspection
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
```

Notes:

- `build/` is disposable and ignored by Git.
- `make format` expects `clang-format` to be available on the host machine.
- The current `mc check` command runs the bootstrap frontend, semantic checker, and MIR pipeline and can emit deterministic AST and MIR dumps through `--dump-ast` and `--dump-mir`.
- `make` is a thin convenience wrapper around the canonical CMake workflow above.
