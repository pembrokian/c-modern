# AGENTS

This repository is a bootstrap Modern C compiler. Treat it as a staged compiler bring-up, not a finished implementation.

## Current Status

- Parser, AST, bootstrap semantic checking, and typed MIR all exist.
- The repository has completed a bootstrap Phase 3 milestone, not the full Phase 3 defined in `docs/plan/plan.txt`.
- Phase 4 MIR work is already active.
- Do not claim “Phase 3 is complete” unless the full `plan.txt` exit criteria are satisfied.

See these files first:

- `README.md`
- `docs/plan/plan.txt`
- `docs/plan/phase3_bootstrap_finish.txt`
- `docs/agent/repo_map.md`

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

Useful focused commands:

```sh
build/debug/mc --help
build/debug/mc check tests/cases/hello.mc
build/debug/mc check tests/cases/hello.mc --dump-mir --build-dir build/debug
ctest --test-dir build/debug -R mc_sema_fixture_unit --output-on-failure
ctest --test-dir build/debug -R mc_mir_fixture_unit --output-on-failure
```

## Testing Conventions

- `tests/parser`: parser fixtures and parse-fail diagnostics
- `tests/sema`: semantic module dumps and semantic-fail diagnostics
- `tests/mir`: MIR dumps and lowering failures
- `tests/tool`: driver and support-layer smoke/unit tests

Diagnostic fixture convention:

- `.errors.txt` stores one expected diagnostic substring per line
- tests match substrings, not full formatted lines

## Editing Guidance By Area

- `compiler/driver`: CLI parsing and pipeline orchestration only
- `compiler/lex`, `compiler/parse`, `compiler/ast`: syntax layer only
- `compiler/sema`: name resolution, type checking, layout, semantic module
- `compiler/mir`: typed IR, lowering from sema, MIR validation and dumps
- `compiler/support`: diagnostics, source loading, dump-path helpers

If you add a feature that changes compiler-visible behavior, update the narrowest relevant test layer first.

## Common Pitfalls

- `docs/plan/phase3_bootstrap_finish.txt` is a repository-specific milestone note, not the full language plan.
- Import handling currently lives in bootstrap sema and is intentionally simpler than a future interface-artifact system.
- Layout support exists, but it is still first-cut bootstrap behavior and not full ABI completion.
- The build tree under `build/` is disposable.