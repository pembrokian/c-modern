export { Thread, Mutex, Condvar, MemoryOrder, Atomic, thread_spawn_raw, thread_spawn, thread_join, mutex_init, mutex_destroy, mutex_lock, mutex_unlock, condvar_init, condvar_destroy, condvar_wait, condvar_signal, atomic_load, atomic_store }

import errors

struct Thread {
    raw: uintptr
}

struct Mutex {
    raw: uintptr
}

struct Condvar {
    raw: uintptr
}

enum MemoryOrder {
    Relaxed,
    Acquire,
    Release,
    AcqRel,
    SeqCst,
}

struct Atomic<T> {}

extern(c) func __mc_sync_thread_spawn(entry: uintptr, ctx: uintptr, err: *errors.Error) Thread
extern(c) func __mc_sync_thread_join(thread: Thread) errors.Error
extern(c) func __mc_sync_mutex_init(err: *errors.Error) Mutex
extern(c) func __mc_sync_mutex_destroy(mu: *Mutex) errors.Error
extern(c) func __mc_sync_mutex_lock(mu: *Mutex) errors.Error
extern(c) func __mc_sync_mutex_unlock(mu: *Mutex) errors.Error
extern(c) func __mc_sync_condvar_init(err: *errors.Error) Condvar
extern(c) func __mc_sync_condvar_destroy(cv: *Condvar) errors.Error
extern(c) func __mc_sync_condvar_wait(cv: *Condvar, mu: *Mutex) errors.Error
extern(c) func __mc_sync_condvar_signal(cv: *Condvar) errors.Error

extern(c) func atomic_load<T>(ptr: *Atomic<T>, order: MemoryOrder) T
extern(c) func atomic_store<T>(ptr: *Atomic<T>, value: T, order: MemoryOrder)

func thread_spawn_raw(entry: uintptr, ctx: uintptr) (Thread, errors.Error) {
    err: errors.Error = errors.ok()
    thread: Thread = __mc_sync_thread_spawn(entry, ctx, &err)
    return thread, err
}

func thread_spawn<T>(entry: func(*T), ctx: *T) (Thread, errors.Error) {
    return thread_spawn_raw((uintptr)(entry), (uintptr)(ctx))
}

func thread_join(thread: Thread) errors.Error {
    return __mc_sync_thread_join(thread)
}

func mutex_init() (Mutex, errors.Error) {
    err: errors.Error = errors.ok()
    mu: Mutex = __mc_sync_mutex_init(&err)
    return mu, err
}

func mutex_destroy(mu: *Mutex) errors.Error {
    return __mc_sync_mutex_destroy(mu)
}

func mutex_lock(mu: *Mutex) errors.Error {
    return __mc_sync_mutex_lock(mu)
}

func mutex_unlock(mu: *Mutex) errors.Error {
    return __mc_sync_mutex_unlock(mu)
}

func condvar_init() (Condvar, errors.Error) {
    err: errors.Error = errors.ok()
    cv: Condvar = __mc_sync_condvar_init(&err)
    return cv, err
}

func condvar_destroy(cv: *Condvar) errors.Error {
    return __mc_sync_condvar_destroy(cv)
}

func condvar_wait(cv: *Condvar, mu: *Mutex) errors.Error {
    return __mc_sync_condvar_wait(cv, mu)
}

func condvar_signal(cv: *Condvar) errors.Error {
    return __mc_sync_condvar_signal(cv)
}