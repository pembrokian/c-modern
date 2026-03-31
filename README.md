# Modern C

This repository now contains the Phase 2 frontend bring-up for the first Modern C implementation.

What is in place:

- a repository layout that separates compiler, runtime, standard library, tests, examples, tools, and generated output
- a host build based on CMake that produces a working `mc` frontend executable
- a support layer for diagnostics, source-file loading, deterministic dump-path conventions, and AST dumps
- a handwritten lexer and parser that handle modules, declarations, types, statements, and expression precedence for the current v0.2 syntax subset
- a thin `mc` driver with `check`, `dump-paths`, and AST dump wiring for frontend inspection
- parser-oriented tests wired through CTest and exposed through the same `make` command path used for local development

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
- The current `mc check` command runs the real Phase 2 frontend and can emit a deterministic AST dump through `--dump-ast`.
- `make` is a thin convenience wrapper around the canonical CMake workflow above.
