import rollup_manifest
import rollup_model
import rollup_render
import testing

func test_manifest_profile() *i32 {
    manifest := rollup_manifest.manifest_decode(1, 3, 4, 1, 0)

    err: *i32 = testing.expect_usize_eq(manifest.busy_open_floor, 4)
    if err != nil {
        return err
    }

    err = testing.expect_usize_eq(manifest.blocked_attention_floor, 1)
    if err != nil {
        return err
    }

    summary := rollup_model.Summary{ open_items: 1, closed_items: 1, blocked_items: 1, priority_items: 0 }
    err = testing.expect_i32_eq(rollup_render.rollup_kind_with_manifest(summary, manifest), 3)
    if err != nil {
        return err
    }

    fallback := rollup_manifest.manifest_decode(7, 2, 9, 9, 9)
    err = testing.expect_i32_eq(
        rollup_render.rollup_kind_with_manifest(
            rollup_model.Summary{ open_items: 2, closed_items: 0, blocked_items: 0, priority_items: 0 },
            fallback),
        2)
    if err != nil {
        return err
    }

    return nil
}