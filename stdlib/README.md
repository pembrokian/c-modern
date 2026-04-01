# Standard Library Layout

The `stdlib/` tree still contains the long-term module buckets from the bring-up
plan, but the repository now also has a real bootstrap Phase 6 hosted slice.

Implemented bootstrap modules:

- `mem.mc`
- `io.mc`
- `fs.mc`
- `strings.mc`
- `utf8.mc`

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
	borrowed-text helpers only
- `utf8` currently exposes a narrow borrowed-text slice built on top of
	`strings`; fuller canonical validation helpers remain deferred, but
	executable byte-view support is now present
- the preferred admitted file-loading path is now `fs.read_all(path,
	mem.default_allocator())`; `fs.read_all_default` remains as a narrow hosted
	convenience wrapper

Still deferred in this repository:

- `sync`
- `time`
- `os`
- `hal`
- `testing`
