# Runtime Layout

The runtime tree is split by environment boundary:

- `hosted/` for startup shims and ABI-boundary code used by hosted targets
- `freestanding/` for future freestanding startup and boundary code

Standard-library logic should not live here.
