# Example Layout

The example tree is staged for three roles:

- `canonical/` for programs derived from the canonical pressure-test set
- `small/` for smoke-test programs used during compiler bring-up
- `real/` for small real utilities used in later semantic pressure testing

Current bootstrap canonical examples mirror the representative MIR fixtures under `tests/compiler/mir/` so Phase 4 close-out validation can point at concrete canonical sources outside the test harness itself.

Current repository-specific admitted real projects:

- `real/grep_lite/` for bounded text search over a single file
- `real/file_walker/` for deterministic recursive file traversal
- `real/hash_tool/` for bounded whole-file checksum output
- `real/arena_expr/` for arena-backed expression parsing and normalization
- `real/worker_queue/` for bounded producer-or-worker queue pressure on the admitted hosted `sync` slice as an explicit low-level shared-memory proof rather than the preferred portable communication model
- `real/pipe_handoff/` for the admitted handle-first thread-plus-pipe communication proof on the minimal `io.pipe()` surface
- `real/pipe_ready/` for the admitted poller-coupled pipe-readiness proof on `io.pipe()` plus `io.poller_*`
- `real/line_filter_relay/` for the admitted narrow hosted subprocess proof over explicit argv spawn plus pipe-backed stdin/stdout relay
- `real/evented_echo/` for the admitted hosted networking project proof
- `real/evented_partial_write/` for the admitted partial-write networking follow-on

Typical project-driven workflows:

```sh
build/debug/bin/mc run --project examples/real/grep_lite/build.toml --build-dir build/debug/probes/grep_lite -- alpha examples/real/grep_lite/tests/sample.txt
build/debug/bin/mc run --project examples/real/file_walker/build.toml --build-dir build/debug/probes/file_walker -- examples/real/file_walker/tests/sample_tree
build/debug/bin/mc run --project examples/real/hash_tool/build.toml --build-dir build/debug/probes/hash_tool -- examples/real/hash_tool/tests/sample.txt
build/debug/bin/mc run --project examples/real/arena_expr/build.toml --build-dir build/debug/probes/arena_expr -- examples/real/arena_expr/tests/sample.expr
build/debug/bin/mc run --project examples/real/worker_queue/build.toml --build-dir build/debug/probes/worker_queue
build/debug/bin/mc run --project examples/real/pipe_handoff/build.toml --build-dir build/debug/probes/pipe_handoff
build/debug/bin/mc run --project examples/real/pipe_ready/build.toml --build-dir build/debug/probes/pipe_ready
build/debug/bin/mc run --project examples/real/line_filter_relay/build.toml --build-dir build/debug/probes/line_filter_relay -- "phase forty five"
build/debug/bin/mc test --project examples/real/arena_expr/build.toml --build-dir build/debug/probes/arena_expr_tests
build/debug/bin/mc test --project examples/real/worker_queue/build.toml --build-dir build/debug/probes/worker_queue
build/debug/bin/mc test --project examples/real/pipe_handoff/build.toml --build-dir build/debug/probes/pipe_handoff
build/debug/bin/mc test --project examples/real/pipe_ready/build.toml --build-dir build/debug/probes/pipe_ready
build/debug/bin/mc test --project examples/real/line_filter_relay/build.toml --build-dir build/debug/probes/line_filter_relay
```

These projects are repository-owned proof artifacts for the admitted repository slice, not claims that the broader long-term real-program surface is complete.
