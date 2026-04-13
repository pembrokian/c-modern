enum MemoryOrder {
    Relaxed,
    Acquire,
    Release,
}

struct Atomic<T> {}

func memory_barrier() {
}

func volatile_load(ptr: *i32) i32 {
    return 0
}

func volatile_store(ptr: *i32, value: i32) {
}

func atomic_load(ptr: *Atomic<i32>, order: MemoryOrder) i32 {
    return 0
}

func atomic_store(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) {
}

func atomic_exchange(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) i32 {
    return value
}

func atomic_compare_exchange(ptr: *Atomic<i32>, expected: *i32, desired: i32, success: MemoryOrder, failure: MemoryOrder) bool {
    return true
}

func atomic_fetch_add(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) i32 {
    return value
}

func demo(ptr: *i32, atom: *Atomic<i32>, expected: *i32) bool {
    value: i32 = volatile_load(ptr)
    volatile_store(ptr, value)
    memory_barrier()
    loaded: i32 = atomic_load(atom, MemoryOrder.Acquire)
    atomic_store(atom, loaded, MemoryOrder.Release)
    swapped: i32 = atomic_exchange(atom, loaded, MemoryOrder.Acquire)
    added: i32 = atomic_fetch_add(atom, swapped, MemoryOrder.Release)
    return atomic_compare_exchange(atom, expected, added, MemoryOrder.Acquire, MemoryOrder.Relaxed)
}