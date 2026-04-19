import bundle_stage
import mem
import testing

func test_run_rounds_to_granules() *i32 {
    granule: usize = mem.run_granule_bytes()
    run: mem.Run = mem.run_init(mem.default_allocator(), granule + 17)
    defer mem.run_deinit(run)

    err: *i32 = testing.expect(mem.run_capacity(run) != 0)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(mem.run_capacity(run), granule * 2)
    if err != nil {
        return err
    }

    stage: bundle_stage.StagedBundle = bundle_stage.StagedBundle{ run: run, used: granule + 17 }
    return testing.expect_usize_eq(bundle_stage.stage_granules(stage), 2)
}