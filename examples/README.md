# Example Layout

The example tree is staged for three roles:

- `canonical/` for programs derived from the canonical pressure-test set
- `small/` for smoke-test programs used during compiler bring-up
- `real/` for small real utilities used in later semantic pressure testing

Current bootstrap canonical examples mirror the representative MIR fixtures under `tests/mir/` so Phase 4 close-out validation can point at concrete canonical sources outside the test harness itself.
