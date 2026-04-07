import mem
import testing
import walk

func test_path_join() *i32 {
    joined: *Buffer<u8> = walk.path_join("root", "child.txt")
    err: *i32 = testing.expect_buffer_non_nil<u8>(joined)
    if err != nil {
        return err
    }
    defer mem.buffer_free<u8>(joined)

    joined_bytes: Slice<u8> = mem.slice_from_buffer<u8>(joined)
    joined_text: str = str{ ptr: joined_bytes.ptr, len: joined_bytes.len }
    err = testing.expect_str_eq(joined_text, "root/child.txt")
    if err != nil {
        return err
    }
    return nil
}