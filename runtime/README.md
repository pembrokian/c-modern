# Runtime Layout

The runtime tree is split by environment boundary:

- `hosted/` for startup shims and ABI-boundary code used by hosted targets
- `freestanding/` for freestanding startup and target-boundary code

The admitted bootstrap Phase 6 hosted slice now uses:

- `runtime/hosted/mc_hosted_runtime.c` for the hosted process-entry shim
- stack-built borrowed `Slice<cstr>` argument forwarding into compiler-emitted
	`__mc_hosted_entry`
- narrow hosted support shims for the admitted `mem`, `io`, and `fs` stdlib
	surfaces

The initial freestanding Phase 81 slice now also uses:

- `runtime/freestanding/mc_bootstrap_main.c` for the first replaceable
	freestanding startup path selected through `runtime.startup = "bootstrap_main"`
- one explicit `bootstrap_main() -> i32` Modern C entry symbol rather than the
	hosted `main(args: Slice<cstr>) i32` convention

Standard-library policy should still not accumulate here. The runtime file only
owns the hosted process boundary and the narrow host ABI shims needed by the
current bootstrap slice, plus the smallest freestanding startup support needed
by the first non-hosted proof.
