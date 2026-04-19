struct Allocator {
    raw: uintptr
}

struct Pool {
    raw: uintptr
}

struct Run {
    raw: uintptr
}

distinct Arena = *u8

@private
extern(c) func __mc_mem_default_allocator() *Allocator
@private
extern(c) func __mc_mem_buffer_len_u8(buf: *Buffer<u8>) usize
@private
extern(c) func __mc_mem_run_granule_bytes() usize
@private
extern(c) func __mc_mem_run_init(alloc: *Allocator, min_bytes: usize) uintptr
@private
extern(c) func __mc_mem_run_deinit(raw: uintptr)
@private
extern(c) func __mc_mem_run_capacity(raw: uintptr) usize
@private
extern(c) func __mc_mem_run_data(raw: uintptr) *u8
@private
extern(c) func __mc_mem_arena_init(alloc: *Allocator, cap: usize) Arena
@private
extern(c) func __mc_mem_arena_reset(arena: Arena)
@private
extern(c) func __mc_mem_arena_deinit(arena: Arena)
@private
extern(c) func __mc_mem_pool_init(alloc: *Allocator, slot_size: usize, cap: usize) uintptr
@private
extern(c) func __mc_mem_pool_deinit(raw: uintptr)
@private
extern(c) func __mc_mem_pool_take(raw: uintptr) *u8
@private
extern(c) func __mc_mem_pool_return(raw: uintptr, slot: *u8) bool
@private
extern(c) func __mc_mem_pool_available(raw: uintptr) usize

extern(c) func buffer_new<T>(alloc: *Allocator, cap: usize) *Buffer<T>
extern(c) func buffer_free<T>(buf: *Buffer<T>)
extern(c) func slice_from_buffer<T>(buf: *Buffer<T>) Slice<T>
extern(c) func arena_new<T>(arena: Arena) *T

func default_allocator() *Allocator {
    return __mc_mem_default_allocator()
}

func buffer_len(buf: *Buffer<u8>) usize {
    return __mc_mem_buffer_len_u8(buf)
}

func run_granule_bytes() usize {
    return __mc_mem_run_granule_bytes()
}

func run_init(alloc: *Allocator, min_bytes: usize) Run {
    return Run{ raw: __mc_mem_run_init(alloc, min_bytes) }
}

func run_deinit(run: Run) {
    __mc_mem_run_deinit(run.raw)
}

func run_capacity(run: Run) usize {
    return __mc_mem_run_capacity(run.raw)
}

func run_slice(run: Run) Slice<u8> {
    return Slice<u8>{ ptr: __mc_mem_run_data(run.raw), len: run_capacity(run) }
}

func arena_init(alloc: *Allocator, cap: usize) Arena {
    return __mc_mem_arena_init(alloc, cap)
}

func arena_reset(arena: Arena) {
    __mc_mem_arena_reset(arena)
}

func arena_deinit(arena: Arena) {
    __mc_mem_arena_deinit(arena)
}

func pool_init(alloc: *Allocator, slot_size: usize, cap: usize) Pool {
    return Pool{ raw: __mc_mem_pool_init(alloc, slot_size, cap) }
}

func pool_deinit(pool: Pool) {
    __mc_mem_pool_deinit(pool.raw)
}

func pool_take(pool: Pool) *u8 {
    return __mc_mem_pool_take(pool.raw)
}

func pool_return(pool: Pool, slot: *u8) bool {
    return __mc_mem_pool_return(pool.raw, slot)
}

func pool_available(pool: Pool) usize {
    return __mc_mem_pool_available(pool.raw)
}
