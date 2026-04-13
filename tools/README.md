# Tools Layout

`tools/` is reserved for developer helper scripts that are not part of the language surface.

Current helpers

- `select_tests.py`: conservative changed-path to owning-CTest selector for local iteration. It can inspect explicit paths or current branch changes against the merge-base with `main`, print the selected CTest targets with reasons, optionally run an incremental build plus the selected tests, and optionally skip rerunning selected tests whose owned inputs still match the last successful cached fingerprint.

Convenience entrypoint

- `make select-tests`: wrapper around `tools/select_tests.py`.
  - default behavior uses `SELECT_TESTS_ARGS=--build --run --cache`
  - override arguments when needed, for example:
    `make select-tests SELECT_TESTS_ARGS='--changed tests/tool/freestanding/kernel/runtime/phase109_first_running_kernel_slice_audit/phase.toml --changed docs/agent/prompts/repo_map.md --run --cache'`

Freestanding kernel maintenance note

- adding a new runtime kernel phase normally means updating its descriptor directory under `tests/tool/freestanding/kernel/runtime/phase.../`, adding its goldens, and touching `tests/tool/freestanding/kernel/synthetic/` only for the separate synthetic phases85-88 surface
- update `tools/select_tests.py` only when top-level kernel surface ownership changes or a new surface is introduced
