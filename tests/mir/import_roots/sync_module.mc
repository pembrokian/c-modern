export { MemoryOrder, Atomic, atomic_load }

enum MemoryOrder {
    Relaxed,
    Acquire,
    Release,
}

struct Atomic<T> {}

func atomic_load(ptr: *Atomic<i32>, order: MemoryOrder) i32 {
    return 0
}