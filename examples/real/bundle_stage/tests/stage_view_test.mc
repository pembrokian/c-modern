import bundle_stage
import mem
import testing

func test_stage_view_tracks_used_bytes() *i32 {
    run: mem.Run = mem.run_init(mem.default_allocator(), 4)
    defer mem.run_deinit(run)

    full: Slice<u8> = mem.run_slice(run)
    full[0] = 82
    full[1] = 85
    full[2] = 78
    full[3] = 33

    stage: bundle_stage.StagedBundle = bundle_stage.StagedBundle{ run: run, used: 4 }
    payload: Slice<u8> = bundle_stage.stage_payload(stage)
    err: *i32 = testing.expect(payload.ptr != nil)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(payload.len, 4)
    if err != nil {
        return err
    }

    text: str = str{ ptr: payload.ptr, len: payload.len }
    err = testing.expect_str_eq(text, "RUN!")
    if err != nil {
        return err
    }
    return testing.expect_usize_eq((usize)(bundle_stage.stage_byte_sum(stage)), 278)
}