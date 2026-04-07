import testing
import worker_queue

func test_next_slot() *i32 {
    if worker_queue.next_slot(0) != 1 {
        return testing.fail()
    }
    if worker_queue.next_slot(1) != 0 {
        return testing.fail()
    }
    if worker_queue.next_slot(5) != 0 {
        return testing.fail()
    }
    return nil
}