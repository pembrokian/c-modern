# Standard Library Layout

The `stdlib/` tree still contains the long-term module buckets from the bring-up
plan, but the repository now also has a real bootstrap Phase 6 hosted slice.

Implemented bootstrap modules:

- `mem.mc`
- `io.mc`
- `fmt.mc`
- `path.mc`
- `time.mc`
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
  byte-slice validation and `utf8.leading_codepoint_width(...)` for non-allocating
  leading-code-point sizing
- the admitted file-loading path is `fs.read_all(path,
  mem.default_allocator())`, keeping allocation policy explicit at the callsite
- `path` is a narrow slash-separated helper companion above `mem` and
  `strings`; the admitted slice currently includes `path.join(...)`,
  `path.basename(...)`, and `path.dirname(...)`; `path.join(...)` preserves an
  absolute right-hand operand unchanged, `basename(...)` and `dirname(...)`
  ignore trailing `/`, and the empty-input or no-parent cases follow a small
  POSIX-like convention by returning `"."` while root-only inputs return `"/"`
- the hosted `fs` surface now also includes `fs.is_dir(...)` and
  `fs.list_dir(...)`, where directory listings are returned as newline-delimited
  entry names with directories marked by a trailing `/`; this remains a narrow
  bootstrap surface for deterministic utilities rather than a final richer API
- `fs.file_size(...)` and `fs.is_dir(...)` now return `errors.Error` alongside
  their primary result so callers can distinguish filesystem failure from an
  ordinary negative or zero result; `fs.is_dir(...)` currently returns an
  `i32` status flag rather than `bool` so the MC ABI matches the hosted C
  runtime honestly on the current compiler boundary; `fs.read_all(...)` and
  `fs.list_dir(...)` still use nil-only failure for the admitted slice
- `fs.read_all(...)` and `fs.list_dir(...)` intentionally remain coarse on the
  admitted slice: they return either an owned buffer or `nil`, and they do not
  currently surface per-call host error codes through an `errors.Error` channel;
  widening that boundary stays deferred until a real admitted consumer needs
  actionable filesystem failure detail rather than simple success-or-failure
- `io` is the primary portable I/O and communication boundary: it owns
  `File`, `read`, `write`, `close`, readiness polling, and the admitted
  `Pipe` plus `pipe()` primitive used to connect independent execution
  contexts without introducing scheduler policy
- `fmt` is the narrow formatting companion above `io`: it currently provides
  explicit sink helpers like `print` and `fprint`, plus owned-buffer text
  helpers like `sprint` and small numeric conversion helpers for bootstrap
  use
- `net` remains a narrow portable constructor layer above `io`; TCP listen,
  accept, and connect operations return `io.File` so ordinary networking
  composes directly with `io.read`, `io.write`, `io.close`, and `io.poller_*`;
  the admitted slice now also includes `net.parse_port(...)` as one borrowed-
  text helper for ordinary CLI-style port parsing used across the admitted
  networking proofs, while richer endpoint text parsing remains deferred
- the admitted networking address record is currently explicit IPv4-only
  `net.Ipv4Addr`; broader multi-family address modeling remains deferred
- `time` is a narrow monotonic elapsed-time companion; the admitted slice
  currently includes `time.Duration`, `time.monotonic()`, and small explicit
  conversion or comparison helpers; `time.millis(...)`, `time.seconds(...)`,
  and `time.add(...)` now saturate at the maximum representable duration rather
  than silently wrapping, while wall-clock and sleep semantics remain deferred
- `testing` is admitted only as a narrow toolchain companion module for the
  ordinary project-test path; it now includes modest expectation helpers for
  boolean, integer, and string checks with minimal mismatch output, but it is
  still not a broader fixture, capture, or compiler-magic API
- `sync` is the hosted execution and low-level shared-memory boundary: thread
  spawn or join remain part of the primary execution model, while mutexes,
  condition variables, and narrow `Atomic<T>` load or store publication remain
  explicit low-level hosted tools rather than the preferred portable
  communication model; `condvar_wait(...)` follows the ordinary spurious-wakeup
  rule and callers must re-check their predicate in a loop, while compare-
  exchange, fetch-add, and broader scheduler claims remain deferred
- the repository does not admit async syntax, futures, channels, actors, or
  hidden scheduler semantics; the preferred portable model remains explicit
  execution plus handle-based communication plus readiness waiting

Still deferred in this repository:

- `os`
- `hal`
