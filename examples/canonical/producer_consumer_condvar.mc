export { main }

import errors
import sync

var handoff_mu_ptr: *sync.Mutex = nil
var handoff_cv_ptr: *sync.Condvar = nil
var phase: i32 = 0
var shared_value: i32 = 0
var worker_status: i32 = 0

func worker(ignored: *i32) {
    err: errors.Error = sync.mutex_lock(handoff_mu_ptr)
    if !errors.is_ok(err) {
        worker_status = 1
        return
    }

    phase = 1
    err = sync.condvar_signal(handoff_cv_ptr)
    if !errors.is_ok(err) {
        worker_status = 2
        unlock_err_on_signal: errors.Error = sync.mutex_unlock(handoff_mu_ptr)
        if !errors.is_ok(unlock_err_on_signal) {
            return
        }
        return
    }

    while phase != 2 {
        err = sync.condvar_wait(handoff_cv_ptr, handoff_mu_ptr)
        if !errors.is_ok(err) {
            worker_status = 3
            unlock_err_on_wait: errors.Error = sync.mutex_unlock(handoff_mu_ptr)
            if !errors.is_ok(unlock_err_on_wait) {
                return
            }
            return
        }
    }

    if shared_value != 7 {
        worker_status = 4
    }

    err = sync.mutex_unlock(handoff_mu_ptr)
    if !errors.is_ok(err) {
        worker_status = 5
        return
    }
}

func main() i32 {
    err: errors.Error
    mu: sync.Mutex
    cv: sync.Condvar

    mu, err = sync.mutex_init()
    if !errors.is_ok(err) {
        return 10
    }
    cv, err = sync.condvar_init()
    if !errors.is_ok(err) {
        return 11
    }

    handoff_mu_ptr = &mu
    handoff_cv_ptr = &cv
    phase = 0
    shared_value = 0
    worker_status = 0

    err = sync.mutex_lock(&mu)
    if !errors.is_ok(err) {
        return 12
    }

    thread: sync.Thread
    thread, err = sync.thread_spawn<i32>(worker, &worker_status)
    if !errors.is_ok(err) {
        unlock_err_on_spawn: errors.Error = sync.mutex_unlock(&mu)
        if !errors.is_ok(unlock_err_on_spawn) {
            return 13
        }
        return 14
    }

    while phase != 1 {
        err = sync.condvar_wait(&cv, &mu)
        if !errors.is_ok(err) {
            return 15
        }
    }

    shared_value = 7
    phase = 2
    err = sync.condvar_signal(&cv)
    if !errors.is_ok(err) {
        unlock_err_on_main_signal: errors.Error = sync.mutex_unlock(&mu)
        if !errors.is_ok(unlock_err_on_main_signal) {
            return 16
        }
        return 17
    }

    err = sync.mutex_unlock(&mu)
    if !errors.is_ok(err) {
        return 18
    }

    err = sync.thread_join(thread)
    if !errors.is_ok(err) {
        return 19
    }

    if worker_status != 0 {
        return 20
    }
    if phase != 2 {
        return 21
    }
    if shared_value != 7 {
        return 22
    }

    return 0
}