export { main }

import errors
import io
import sync
import worker_queue

func cleanup_mutex_only(mu: *sync.Mutex) i32 {
    err: errors.Error = sync.mutex_destroy(mu)
    if !errors.is_ok(err) {
        return 30
    }
    return 0
}

func cleanup_sync(mu: *sync.Mutex, cv: *sync.Condvar) i32 {
    err: errors.Error = sync.condvar_destroy(cv)
    if !errors.is_ok(err) {
        return 31
    }
    err = sync.mutex_destroy(mu)
    if !errors.is_ok(err) {
        return 32
    }
    return 0
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
        condvar_cleanup_status: i32 = cleanup_mutex_only(&mu)
        if condvar_cleanup_status != 0 {
            return condvar_cleanup_status
        }
        return 11
    }

    worker_queue.bind_runtime(&mu, &cv)
    worker_queue.reset_runtime()

    worker_status: i32 = 0
    worker_thread: sync.Thread
    worker_thread, err = sync.thread_spawn<i32>(worker_queue.worker, &worker_status)
    if !errors.is_ok(err) {
        worker_spawn_cleanup_status: i32 = cleanup_sync(&mu, &cv)
        if worker_spawn_cleanup_status != 0 {
            return worker_spawn_cleanup_status
        }
        return 12
    }

    producer_status: i32 = 0
    producer_thread: sync.Thread
    producer_thread, err = sync.thread_spawn<i32>(worker_queue.producer, &producer_status)
    if !errors.is_ok(err) {
        err = worker_queue.close_queue()
        if !errors.is_ok(err) {
            return 13
        }
        err = sync.thread_join(worker_thread)
        if !errors.is_ok(err) {
            return 14
        }
        producer_spawn_cleanup_status: i32 = cleanup_sync(&mu, &cv)
        if producer_spawn_cleanup_status != 0 {
            return producer_spawn_cleanup_status
        }
        return 15
    }

    err = sync.thread_join(producer_thread)
    if !errors.is_ok(err) {
        return 16
    }
    err = sync.thread_join(worker_thread)
    if !errors.is_ok(err) {
        return 17
    }

    if producer_status != 0 {
        return 18
    }
    if worker_status != 0 {
        return 19
    }
    if worker_queue.processed_jobs() != worker_queue.job_count() {
        return 21
    }
    if worker_queue.total_sum() != worker_queue.expected_sum() {
        return 20
    }

    if io.write_line("worker-queue-ok") != 0 {
        return 22
    }

    final_cleanup_status: i32 = cleanup_sync(&mu, &cv)
    if final_cleanup_status != 0 {
        return final_cleanup_status
    }
    return 0
}