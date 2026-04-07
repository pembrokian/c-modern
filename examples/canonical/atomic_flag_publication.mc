import errors
import sync

const SPIN_LIMIT: usize = 10000000

type ReadyAtomic = sync.Atomic<u32>
type ValueAtomic = sync.Atomic<i32>

var ready_raw: u32 = 0
var shared_value_raw: i32 = 0
var producer_status: i32 = 0
var consumer_status: i32 = 0
var observed_value: i32 = 0

func ready_ptr() *ReadyAtomic {
    return (*ReadyAtomic)((uintptr)(&ready_raw))
}

func shared_value_ptr() *ValueAtomic {
    return (*ValueAtomic)((uintptr)(&shared_value_raw))
}

func producer(status: *i32) {
    sync.atomic_store<i32>(shared_value_ptr(), 42, sync.MemoryOrder.Relaxed)
    sync.atomic_store<u32>(ready_ptr(), 1, sync.MemoryOrder.Release)
    *status = 0
}

func consumer(status: *i32) {
    spins: usize = 0
    while spins < SPIN_LIMIT {
        if sync.atomic_load<u32>(ready_ptr(), sync.MemoryOrder.Acquire) != 0 {
            observed_value = sync.atomic_load<i32>(shared_value_ptr(), sync.MemoryOrder.Relaxed)
            *status = 0
            return
        }
        spins = spins + 1
    }
    *status = 1
}

func main() i32 {
    ready_raw = 0
    shared_value_raw = 0
    producer_status = 0
    consumer_status = 0
    observed_value = 0

    consumer_thread: sync.Thread
    err: errors.Error
    consumer_thread, err = sync.thread_spawn<i32>(consumer, &consumer_status)
    if !errors.is_ok(err) {
        return 10
    }

    producer_thread: sync.Thread
    producer_thread, err = sync.thread_spawn<i32>(producer, &producer_status)
    if !errors.is_ok(err) {
        err = sync.thread_join(consumer_thread)
        if !errors.is_ok(err) {
            return 11
        }
        return 12
    }

    err = sync.thread_join(producer_thread)
    if !errors.is_ok(err) {
        return 13
    }
    err = sync.thread_join(consumer_thread)
    if !errors.is_ok(err) {
        return 14
    }

    if producer_status != 0 {
        return 15
    }
    if consumer_status != 0 {
        return 16
    }
    if observed_value != 42 {
        return 17
    }

    return 0
}