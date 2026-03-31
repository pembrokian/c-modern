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

func atomic_fetch_add(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) i32 {
    return value
}

func wait_until_ready(status: *i32, counter: *Atomic<i32>) i32 {
    attempts: i32 = 0
    while volatile_load(status) == 0 {
        atomic_fetch_add(counter, 1, MemoryOrder.Relaxed)
        attempts = attempts + 1
    }
    volatile_store(status, attempts)
    return atomic_load(counter, MemoryOrder.Acquire)
}