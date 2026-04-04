# Standard Library Layout

The `stdlib/` tree still contains the long-term module buckets from the bring-up
plan, but the repository now also has a real bootstrap Phase 6 hosted slice.

Implemented bootstrap modules:

- `mem.mc`
- `io.mc`
- `fs.mc`
- `sync.mc`
- `strings.mc`
- `utf8.mc`
- `testing.mc`

Current repository-specific notes:

- imports are repository-local and explicit; `mc check` and `mc build` discover
  `stdlib/` automatically during local development and test runs
- imported values now use module-qualified access, so user code writes
  `import io` followed by `io.write_line(...)`
- imported user-defined types now support dotted type expressions such as
  `mem.Allocator`
- the admitted `mem` surface now uses canonical buffer operation names such as
  `buffer_new`, `buffer_free`, `buffer_len`, and `slice_from_buffer`; the
  admitted bootstrap surface now supports explicit generic helper calls such as
  `mem.buffer_new<u8>(...)` and `mem.slice_from_buffer<u8>(...)`
- `strings` stays non-allocating in the admitted slice and currently exposes
  borrowed-text helpers only, including `strings.bytes(...)` for explicit
  borrowed byte views over `str`
- `utf8` currently exposes a narrow borrowed-text slice built on top of
  `strings`; fuller canonical validation helpers remain deferred, but
  executable byte-view support is now present
- the admitted `utf8` surface also includes `utf8.valid_bytes(...)` for direct
  byte-slice validation and `utf8.codepoint_len(...)` for non-allocating
  leading-code-point sizing
- the admitted file-loading path is `fs.read_all(path,
  mem.default_allocator())`, keeping allocation policy explicit at the callsite
- the hosted `fs` surface now also includes `fs.is_dir(...)` and
  `fs.list_dir(...)`, where directory listings are returned as newline-delimited
  entry names with directories marked by a trailing `/`; this remains a narrow
  bootstrap surface for deterministic utilities rather than a final richer API
- `testing` is admitted only as a narrow toolchain companion module for the
  ordinary project-test path; it is not yet a broader assertion or fixture API
- `sync` is admitted only as the first hosted concurrency slice: thread spawn
  or join plus mutex init, lock, and unlock; condition variables, atomics, and
  mutex destruction remain deferred

Still deferred in this repository:

- `time`
- `os`
- `hal`
