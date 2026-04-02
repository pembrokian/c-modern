export { Allocator, Arena, default_allocator, buffer_new, buffer_free, buffer_len, slice_from_buffer, arena_init, arena_deinit, arena_new }

struct Allocator {
    raw: uintptr
}

type Arena = *u8

extern(c) func __mc_mem_default_allocator() *Allocator
extern(c) func __mc_mem_buffer_len_u8(buf: *Buffer<u8>) usize
extern(c) func __mc_mem_arena_init(alloc: *Allocator, cap: usize) Arena
extern(c) func __mc_mem_arena_deinit(arena: Arena)

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

func arena_init(alloc: *Allocator, cap: usize) Arena {
    return __mc_mem_arena_init(alloc, cap)
}

func arena_deinit(arena: Arena) {
    __mc_mem_arena_deinit(arena)
}