# Compiler Layout

The compiler tree is organized by phase boundary rather than by utility type.

- `driver/` holds command-line entrypoints and pipeline orchestration.
- `lex/` holds token definitions and lexer interfaces.
- `parse/` holds parser interfaces and AST construction logic.
- `ast/` holds AST node definitions and source-span-aware helpers.
- `resolve/` is reserved for future import loading and name resolution infrastructure.
- `sema/` holds the bootstrap semantic checker, type utilities, and first-cut layout logic.
- `mir/` holds typed MIR definitions, lowering, validation, and dumps.
- `codegen_llvm/` holds the bootstrap LLVM backend scaffolding and the MIR-to-LLVM lowering entrypoints.
- `mci/` is reserved for Modern C interface artifact support.
- `support/` holds shared diagnostics, file services, and small reusable utilities.
