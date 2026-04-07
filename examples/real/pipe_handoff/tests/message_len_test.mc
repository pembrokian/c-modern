import pipe_handoff
import testing

func test_message_len() *i32 {
    err: *i32 = testing.expect_str_eq(pipe_handoff.expected_message(), "phase43-pipe")
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(pipe_handoff.message_len(), 12)
    if err != nil {
        return err
    }
    return nil
}