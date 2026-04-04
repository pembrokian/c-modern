# Example Layout

The example tree is staged for three roles:

- `canonical/` for programs derived from the canonical pressure-test set
- `small/` for smoke-test programs used during compiler bring-up
- `real/` for small real utilities used in later semantic pressure testing

Current bootstrap canonical examples mirror the representative MIR fixtures under `tests/mir/` so Phase 4 close-out validation can point at concrete canonical sources outside the test harness itself.

Current repository-specific admitted real projects:

- `real/grep_lite/` for bounded text search over a single file
- `real/file_walker/` for deterministic recursive file traversal
- `real/hash_tool/` for bounded whole-file checksum output
- `real/arena_expr/` for arena-backed expression parsing and normalization
- `real/worker_queue/` for bounded producer-or-worker queue pressure on the admitted hosted `sync` slice
- `real/evented_echo/` for the admitted hosted networking project proof
- `real/evented_partial_write/` for the admitted partial-write networking follow-on

Typical project-driven workflows:

```sh
build/debug/mc run --project examples/real/grep_lite/build.toml --build-dir build/debug/phase8_grep -- alpha examples/real/grep_lite/tests/sample.txt
build/debug/mc run --project examples/real/file_walker/build.toml --build-dir build/debug/phase8_walk -- examples/real/file_walker/tests/sample_tree
build/debug/mc run --project examples/real/hash_tool/build.toml --build-dir build/debug/phase8_hash -- examples/real/hash_tool/tests/sample.txt
build/debug/mc run --project examples/real/arena_expr/build.toml --build-dir build/debug/phase8_expr -- examples/real/arena_expr/tests/sample.expr
build/debug/mc run --project examples/real/worker_queue/build.toml --build-dir build/debug/phase20_worker_queue
build/debug/mc test --project examples/real/arena_expr/build.toml --build-dir build/debug/phase8_expr_tests
build/debug/mc test --project examples/real/worker_queue/build.toml --build-dir build/debug/phase20_worker_queue
```

These projects are repository-owned proof artifacts for the admitted repository slice, not claims that the broader long-term real-program surface is complete.
