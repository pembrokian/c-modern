import errors
import sync

const QUEUE_CAPACITY: usize = 2
const JOB_COUNT: usize = 4

var queue_mu_ptr: *sync.Mutex = nil
var queue_cv_ptr: *sync.Condvar = nil
var queue_slot0: i32 = 0
var queue_slot1: i32 = 0
var queue_head: usize = 0
var queue_len: usize = 0
var queue_closed: bool = false
var queue_total: i32 = 0
var queue_processed: usize = 0

func bind_runtime(mu: *sync.Mutex, cv: *sync.Condvar) {
    queue_mu_ptr = mu
    queue_cv_ptr = cv
}

func reset_runtime() {
    queue_slot0 = 0
    queue_slot1 = 0
    queue_head = 0
    queue_len = 0
    queue_closed = false
    queue_total = 0
    queue_processed = 0
}

func job_count() usize {
    return JOB_COUNT
}

func next_slot(index: usize) usize {
    return (index + 1) % QUEUE_CAPACITY
}

func job_value(index: usize) i32 {
    if index == 0 {
        return 3
    }
    if index == 1 {
        return 1
    }
    if index == 2 {
        return 4
    }
    return 2
}

func expected_sum() i32 {
    sum: i32 = 0
    index: usize = 0
    while index < JOB_COUNT {
        sum = sum + job_value(index)
        index = index + 1
    }
    return sum
}

func result_matches_expected(total: i32, processed: usize) bool {
    if total != expected_sum() {
        return false
    }
    if processed != JOB_COUNT {
        return false
    }
    return true
}

func total_sum() i32 {
    return queue_total
}

func processed_jobs() usize {
    return queue_processed
}

func store_slot(index: usize, value: i32) {
    if index == 0 {
        queue_slot0 = value
        return
    }
    queue_slot1 = value
}

func load_slot(index: usize) i32 {
    if index == 0 {
        return queue_slot0
    }
    return queue_slot1
}

func close_queue() errors.Error {
    err: errors.Error = sync.mutex_lock(queue_mu_ptr)
    if !errors.is_ok(err) {
        return err
    }

    queue_closed = true
    err = sync.condvar_signal(queue_cv_ptr)
    if !errors.is_ok(err) {
        unlock_err: errors.Error = sync.mutex_unlock(queue_mu_ptr)
        if !errors.is_ok(unlock_err) {
            return unlock_err
        }
        return err
    }

    return sync.mutex_unlock(queue_mu_ptr)
}

func producer(status: *i32) {
    *status = 0

    index: usize = 0
    while index < JOB_COUNT {
        err: errors.Error = sync.mutex_lock(queue_mu_ptr)
        if !errors.is_ok(err) {
            *status = 1
            return
        }

        while queue_len == QUEUE_CAPACITY {
            err = sync.condvar_wait(queue_cv_ptr, queue_mu_ptr)
            if !errors.is_ok(err) {
                *status = 2
                unlock_err_on_wait: errors.Error = sync.mutex_unlock(queue_mu_ptr)
                if !errors.is_ok(unlock_err_on_wait) {
                    *status = 3
                }
                return
            }
        }

        tail: usize = (queue_head + queue_len) % QUEUE_CAPACITY
        store_slot(tail, job_value(index))
        queue_len = queue_len + 1

        err = sync.condvar_signal(queue_cv_ptr)
        if !errors.is_ok(err) {
            *status = 4
            unlock_err_on_signal: errors.Error = sync.mutex_unlock(queue_mu_ptr)
            if !errors.is_ok(unlock_err_on_signal) {
                *status = 5
            }
            return
        }

        err = sync.mutex_unlock(queue_mu_ptr)
        if !errors.is_ok(err) {
            *status = 6
            return
        }

        index = index + 1
    }

    close_err: errors.Error = sync.mutex_lock(queue_mu_ptr)
    if !errors.is_ok(close_err) {
        *status = 7
        return
    }

    queue_closed = true
    close_err = sync.condvar_signal(queue_cv_ptr)
    if !errors.is_ok(close_err) {
        *status = 8
        unlock_err_on_close_signal: errors.Error = sync.mutex_unlock(queue_mu_ptr)
        if !errors.is_ok(unlock_err_on_close_signal) {
            *status = 9
        }
        return
    }

    close_err = sync.mutex_unlock(queue_mu_ptr)
    if !errors.is_ok(close_err) {
        *status = 10
        return
    }
}

func worker(status: *i32) {
    *status = 0

    while true {
        err: errors.Error = sync.mutex_lock(queue_mu_ptr)
        if !errors.is_ok(err) {
            *status = 1
            return
        }

        while queue_len == 0 {
            if queue_closed {
                err = sync.mutex_unlock(queue_mu_ptr)
                if !errors.is_ok(err) {
                    *status = 2
                }
                return
            }

            err = sync.condvar_wait(queue_cv_ptr, queue_mu_ptr)
            if !errors.is_ok(err) {
                *status = 3
                unlock_err_on_wait: errors.Error = sync.mutex_unlock(queue_mu_ptr)
                if !errors.is_ok(unlock_err_on_wait) {
                    *status = 4
                }
                return
            }
        }

        value: i32 = load_slot(queue_head)
        queue_head = next_slot(queue_head)
        queue_len = queue_len - 1
        queue_total = queue_total + value
        queue_processed = queue_processed + 1

        err = sync.condvar_signal(queue_cv_ptr)
        if !errors.is_ok(err) {
            *status = 5
            unlock_err_on_signal: errors.Error = sync.mutex_unlock(queue_mu_ptr)
            if !errors.is_ok(unlock_err_on_signal) {
                *status = 6
            }
            return
        }

        err = sync.mutex_unlock(queue_mu_ptr)
        if !errors.is_ok(err) {
            *status = 7
            return
        }
    }
}