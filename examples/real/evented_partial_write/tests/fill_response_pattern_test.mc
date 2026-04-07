import partial_write_core
import testing

func test_fill_response_pattern() *i32 {
    bytes: [32]u8
    partial_write_core.fill_response((Slice<u8>)(bytes))
    if bytes[0] != 112 {
        return testing.fail()
    }
    if bytes[5] != 49 {
        return testing.fail()
    }
    if bytes[6] != 54 {
        return testing.fail()
    }
    if bytes[27] != 124 {
        return testing.fail()
    }
    if bytes[28] != 112 {
        return testing.fail()
    }
    return nil
}