import io
import rollup_manifest
import rollup_model
import rollup_render_table

const ROLLUP_EMPTY: i32 = 0
const ROLLUP_STEADY: i32 = 1
const ROLLUP_BUSY: i32 = 2
const ROLLUP_ATTENTION: i32 = 3

func rollup_kind_with_manifest(summary: rollup_model.Summary, manifest: rollup_manifest.RollupManifest) i32 {
    if rollup_model.total_items(summary) == 0 {
        return ROLLUP_EMPTY
    }
    if summary.blocked_items >= manifest.blocked_attention_floor {
        return ROLLUP_ATTENTION
    }
    if summary.priority_items + manifest.priority_bonus > 0 {
        return ROLLUP_ATTENTION
    }
    if summary.open_items >= manifest.busy_open_floor {
        return ROLLUP_BUSY
    }
    return ROLLUP_STEADY
}

func rollup_kind(summary: rollup_model.Summary) i32 {
    return rollup_kind_with_manifest(summary, rollup_manifest.default_manifest())
}

func write_rollup(summary: rollup_model.Summary) i32 {
    kind: i32 = rollup_kind(summary)
    writer: func() i32 = rollup_render_table.ROLLUP_WRITERS[usize(kind)]
    return writer()
}