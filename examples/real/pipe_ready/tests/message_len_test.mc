import pipe_ready
import testing

func test_message_len() *i32 {
    err: *i32 = testing.expect_str_eq(pipe_ready.expected_message(), "phase43-ready")
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(pipe_ready.message_len(), 13)
    if err != nil {
        return err
    }
    return nil
}