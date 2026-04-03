enum MemoryOrder {
    Relaxed,
    Acquire,
    Release,
}

struct Atomic<T> {}

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

func atomic_fetch_add(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) i32 {
    return value
}

func volatile_roundtrip(ptr: *i32, value: i32) i32 {
    volatile_store(ptr, value)
    return volatile_load(ptr)
}

func atomic_demo(ptr: *Atomic<i32>, value: i32) i32 {
    atomic_store(ptr, value, MemoryOrder.Release)
    loaded: i32 = atomic_load(ptr, MemoryOrder.Acquire)
    swapped: i32 = atomic_exchange(ptr, loaded, MemoryOrder.Acquire)
    return atomic_fetch_add(ptr, swapped, MemoryOrder.Release)
}