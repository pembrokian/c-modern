# Tools Layout

`tools/` is reserved for developer helper scripts that are not part of the language surface.

Current helpers

- `select_tests.py`: conservative changed-path to owning-CTest selector for local iteration. It can inspect explicit paths or current branch changes against the merge-base with `main`, print the selected CTest targets with reasons, optionally run an incremental build plus the selected tests, and optionally skip rerunning selected tests whose owned inputs still match the last successful cached fingerprint.

Convenience entrypoint

- `make select-tests`: wrapper around `tools/select_tests.py`.
  - default behavior uses `SELECT_TESTS_ARGS=--build --run --cache`
  - override arguments when needed, for example:
    `make select-tests SELECT_TESTS_ARGS='--changed kernel/src/kernel_dispatch.mc --changed compiler/driver/builder.cpp --run --cache'`

Legacy retirement note

- the retired freestanding proof suite and related maintenance scripts have been removed from the live repository surface
- active `kernel/` work should route through `kernel/build.toml`, `tests/tool/tool_workflow_orchestrator.cpp`, and the ordinary grouped tool suites
