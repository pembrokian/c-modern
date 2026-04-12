# Tools Layout

`tools/` is reserved for developer helper scripts that are not part of the language surface.

Current helpers

- `select_tests.py`: conservative changed-path to owning-CTest selector for local iteration. It can inspect explicit paths or current branch changes against the merge-base with `main`, print the selected CTest targets with reasons, optionally run an incremental build plus the selected tests, and optionally skip rerunning selected tests whose owned inputs still match the last successful cached fingerprint.

Convenience entrypoint

- `make select-tests`: wrapper around `tools/select_tests.py`.
	- default behavior uses `SELECT_TESTS_ARGS=--build --run --cache`
	- override arguments when needed, for example:
		`make select-tests SELECT_TESTS_ARGS='--changed tests/tool/freestanding/kernel/shard2.cpp --changed docs/agent/prompts/repo_map.md --run --cache'`

Freestanding kernel maintenance note

- adding a new kernel phase normally means updating the owning shard or standalone proof file, adding its goldens, and updating `tests/tool/freestanding/kernel/suite.cpp` only if a new direct `kernel-case:` selector is needed
- update `tools/select_tests.py` only when shard ownership changes, a new shard is added, or a new non-shard suite surface is introduced
