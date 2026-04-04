export { test_write_chunk_len }

import partial_write_core
import testing

func test_write_chunk_len() *i32 {
    if partial_write_core.write_chunk_len(64) != 64 {
        return testing.fail()
    }
    if partial_write_core.write_chunk_len(128) != 128 {
        return testing.fail()
    }
    if partial_write_core.write_chunk_len(129) != 128 {
        return testing.fail()
    }
    if partial_write_core.write_chunk_len(1536) != 128 {
        return testing.fail()
    }
    return nil
}