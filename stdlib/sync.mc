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
extern(c) func __mc_sync_condvar_broadcast(cv: *Condvar) errors.Error

extern(c) func atomic_load<T>(ptr: *Atomic<T>, order: MemoryOrder) T
extern(c) func atomic_store<T>(ptr: *Atomic<T>, value: T, order: MemoryOrder)

func thread_spawn<T>(entry: func(*T), ctx: *T) (Thread, errors.Error) {
    err: errors.Error = errors.ok()
    thread: Thread = __mc_sync_thread_spawn((uintptr)(entry), (uintptr)(ctx), &err)
    return thread, err
}

func thread_join(thread: Thread) errors.Error {
    // Thread handles are single-use; joining the same handle twice is an error.
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
    // Condvars may wake spuriously; callers must re-check their predicate in a loop.
    return __mc_sync_condvar_wait(cv, mu)
}

func condvar_signal(cv: *Condvar) errors.Error {
    return __mc_sync_condvar_signal(cv)
}

func condvar_broadcast(cv: *Condvar) errors.Error {
    return __mc_sync_condvar_broadcast(cv)
}