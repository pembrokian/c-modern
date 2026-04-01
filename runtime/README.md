# Runtime Layout

The runtime tree is split by environment boundary:

- `hosted/` for startup shims and ABI-boundary code used by hosted targets
- `freestanding/` for future freestanding startup and boundary code

The admitted bootstrap Phase 6 hosted slice now uses:

- `runtime/hosted/mc_hosted_runtime.c` for the hosted process-entry shim
- stack-built borrowed `Slice<cstr>` argument forwarding into compiler-emitted
	`__mc_hosted_entry`
- narrow hosted support shims for the admitted `mem`, `io`, and `fs` stdlib
	surfaces

Standard-library policy should still not accumulate here. The runtime file only
owns the hosted process boundary and the narrow host ABI shims needed by the
current bootstrap slice.
