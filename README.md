# Modern C

This repository now contains the Phase 1 bring-up skeleton for the first Modern C implementation.

What is in place:

- a repository layout that separates compiler, runtime, standard library, tests, examples, tools, and generated output
- a host build based on CMake that produces a single `mc` executable stub
- a small support layer for diagnostics, source-file loading, and deterministic dump-path conventions
- a thin `mc` driver with `check` and `dump-paths` bootstrap commands
- basic bootstrap tests wired through CTest and exposed through the same `make` command path used for local development

Bootstrap commands:

```sh
make build
make test
make dump-paths FILE=tests/cases/hello.mc

# direct cmake path
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

Notes:

- `build/` is disposable and ignored by Git.
- `make format` expects `clang-format` to be available on the host machine.
- The current `mc check` command validates file loading and exercises the bootstrap lexer/parser interfaces only. Full frontend work belongs to Phase 2.
- `make` is a thin convenience wrapper around the canonical CMake workflow above.
