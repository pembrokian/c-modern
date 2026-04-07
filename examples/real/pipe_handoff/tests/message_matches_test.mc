import pipe_handoff
import strings
import testing

func test_message_matches() *i32 {
    expected: Slice<u8> = strings.bytes(pipe_handoff.expected_message())
    storage: [32]u8
    bytes: Slice<u8> = (Slice<u8>)(storage)

    index: usize = 0
    while index < expected.len {
        bytes[index] = expected[index]
        index = index + 1
    }

    err: *i32 = testing.expect(pipe_handoff.message_matches(bytes[0:expected.len]))
    if err != nil {
        return err
    }

    bytes[0] = 0
    err = testing.expect_false(pipe_handoff.message_matches(bytes[0:expected.len]))
    if err != nil {
        return err
    }
    return nil
}