# Compiler Layout

The compiler tree is organized by phase boundary rather than by utility type.

- `driver/` holds command-line entrypoints and pipeline orchestration.
- `lex/` holds token definitions and lexer interfaces.
- `parse/` holds parser interfaces and AST construction logic.
- `ast/` is reserved for AST node definitions and source-span-aware helpers.
- `resolve/` is reserved for import loading and name resolution.
- `sema/` is reserved for type checking, constant evaluation, and layout.
- `mir/` is reserved for typed MIR definitions, builders, validators, and dumps.
- `codegen_llvm/` is reserved for MIR-to-LLVM lowering.
- `mci/` is reserved for Modern C interface artifact support.
- `support/` holds shared diagnostics, file services, and small reusable utilities.
