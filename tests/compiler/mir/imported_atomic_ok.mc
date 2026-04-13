import sync

func read(atom: *sync.Atomic<i32>, order: sync.MemoryOrder) i32 {
    return sync.atomic_load(atom, order)
}