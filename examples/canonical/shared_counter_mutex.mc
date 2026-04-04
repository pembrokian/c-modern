export { main }

import errors
import sync

const ITERATIONS: usize = 10000

var counter_mu_ptr: *sync.Mutex = nil
var counter_value: i64 = 0
var worker1_status: i32 = 0
var worker2_status: i32 = 0

func worker1(ignored: *i32) {
    index: usize = 0
    while index < ITERATIONS {
        err: errors.Error = sync.mutex_lock(counter_mu_ptr)
        if !errors.is_ok(err) {
            worker1_status = 1
            return
        }
        counter_value = counter_value + 1
        err = sync.mutex_unlock(counter_mu_ptr)
        if !errors.is_ok(err) {
            worker1_status = 2
            return
        }
        index = index + 1
    }
}

func worker2(ignored: *i32) {
    index: usize = 0
    while index < ITERATIONS {
        err: errors.Error = sync.mutex_lock(counter_mu_ptr)
        if !errors.is_ok(err) {
            worker2_status = 1
            return
        }
        counter_value = counter_value + 1
        err = sync.mutex_unlock(counter_mu_ptr)
        if !errors.is_ok(err) {
            worker2_status = 2
            return
        }
        index = index + 1
    }
}

func main() i32 {
    err: errors.Error
    mu: sync.Mutex
    mu, err = sync.mutex_init()
    if !errors.is_ok(err) {
        return 10
    }

    counter_mu_ptr = &mu
    counter_value = 0
    worker1_status = 0
    worker2_status = 0

    t1: sync.Thread
    t1, err = sync.thread_spawn((uintptr)(worker1), (uintptr)(&worker1_status))
    if !errors.is_ok(err) {
        return 11
    }

    t2: sync.Thread
    t2, err = sync.thread_spawn((uintptr)(worker2), (uintptr)(&worker2_status))
    if !errors.is_ok(err) {
        err = sync.thread_join(t1)
        if !errors.is_ok(err) {
            return 12
        }
        return 13
    }

    err = sync.thread_join(t1)
    if !errors.is_ok(err) {
        return 14
    }
    err = sync.thread_join(t2)
    if !errors.is_ok(err) {
        return 15
    }

    if worker1_status != 0 {
        return 16
    }
    if worker2_status != 0 {
        return 17
    }
    if counter_value != (i64)(ITERATIONS + ITERATIONS) {
        return 18
    }

    return 0
}