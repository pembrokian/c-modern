# Test Layout

The test tree mirrors the planned validation layers:

- `parser/`
- `sema/`
- `mir/`
- `codegen/`
- `stdlib/`
- `tool/`
- `cases/`

Current codegen coverage is still bootstrap-narrow:

- `codegen/` currently snapshots deterministic backend text dumps for inspectability before executable bring-up exists
- executable tests under `codegen/` are a later Phase 5 step, after object emission and linking are implemented

Phase 2 adds parser fixtures under `parser/` for both AST snapshots and parse-fail diagnostic substrings.

Parse-fail convention:

- the `.errors.txt` file stores one expected diagnostic substring per line
- fixture checks require every non-empty line to appear somewhere in the rendered diagnostics
- use substrings rather than full-path or full-line matches so fixtures stay stable across temporary paths and formatting tweaks
- malformed-literal lexer diagnostics can be covered through the same parser fixture runner because lex failures propagate as parse-fail results
- numeric-literal fixtures should cover both malformed structure such as missing exponent digits and lexical-shape rules such as underscore placement
- exponent digit sequences are intentionally stricter than the significand: `_` is allowed in `1_000.25` but rejected in `1e1_0`
