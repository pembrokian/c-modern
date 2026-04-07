import testing
import worker_queue

func test_expected_sum() *i32 {
    err: *i32 = testing.expect_i32_eq(worker_queue.expected_sum(), 10)
    if err != nil {
        return err
    }
    err = testing.expect(worker_queue.result_matches_expected(10, worker_queue.job_count()))
    if err != nil {
        return err
    }
    err = testing.expect_false(worker_queue.result_matches_expected(9, worker_queue.job_count()))
    if err != nil {
        return err
    }
    err = testing.expect_false(worker_queue.result_matches_expected(10, worker_queue.job_count() - 1))
    if err != nil {
        return err
    }
    return nil
}