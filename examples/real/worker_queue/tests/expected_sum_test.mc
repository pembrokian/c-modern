export { test_expected_sum }

import testing
import worker_queue

func test_expected_sum() *i32 {
    if worker_queue.expected_sum() != 10 {
        return testing.fail()
    }
    if !worker_queue.result_matches_expected(10, worker_queue.job_count()) {
        return testing.fail()
    }
    if worker_queue.result_matches_expected(9, worker_queue.job_count()) {
        return testing.fail()
    }
    if worker_queue.result_matches_expected(10, worker_queue.job_count() - 1) {
        return testing.fail()
    }
    return nil
}