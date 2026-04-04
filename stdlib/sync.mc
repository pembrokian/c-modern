export { Thread, Mutex, thread_spawn, thread_join, mutex_init, mutex_lock, mutex_unlock }

import errors

struct Thread {
    raw: uintptr
}

struct Mutex {
    raw: uintptr
}

extern(c) func __mc_sync_thread_spawn(entry: uintptr, ctx: uintptr, err: *errors.Error) Thread
extern(c) func __mc_sync_thread_join(thread: Thread) errors.Error
extern(c) func __mc_sync_mutex_init(err: *errors.Error) Mutex
extern(c) func __mc_sync_mutex_lock(mu: *Mutex) errors.Error
extern(c) func __mc_sync_mutex_unlock(mu: *Mutex) errors.Error

func thread_spawn(entry: uintptr, ctx: uintptr) (Thread, errors.Error) {
    err: errors.Error = errors.ok()
    thread: Thread = __mc_sync_thread_spawn(entry, ctx, &err)
    return thread, err
}

func thread_join(thread: Thread) errors.Error {
    return __mc_sync_thread_join(thread)
}

func mutex_init() (Mutex, errors.Error) {
    err: errors.Error = errors.ok()
    mu: Mutex = __mc_sync_mutex_init(&err)
    return mu, err
}

func mutex_lock(mu: *Mutex) errors.Error {
    return __mc_sync_mutex_lock(mu)
}

func mutex_unlock(mu: *Mutex) errors.Error {
    return __mc_sync_mutex_unlock(mu)
}