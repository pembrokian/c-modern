import io
import rollup_manifest
import rollup_model
import rollup_render_table

const ROLLUP_EMPTY: i32 = 0
const ROLLUP_STEADY: i32 = 1
const ROLLUP_BUSY: i32 = 2
const ROLLUP_ATTENTION: i32 = 3

const ROLLUP_LAUNCH_STATUS_NONE: u8 = 0
const ROLLUP_LAUNCH_STATUS_FRESH: u8 = 'F'
const ROLLUP_LAUNCH_STATUS_RESUMED: u8 = 'R'
const ROLLUP_LAUNCH_STATUS_INVALIDATED: u8 = 'I'

const ROLLUP_EMPTY_CELLS: [4]u8 = { 'E', 'M', 'T', 'Y' }
const ROLLUP_STEADY_CELLS: [4]u8 = { 'S', 'T', 'D', 'Y' }
const ROLLUP_BUSY_CELLS: [4]u8 = { 'B', 'U', 'S', 'Y' }
const ROLLUP_ATTENTION_CELLS: [4]u8 = { 'A', 'T', 'T', 'N' }
const ROLLUP_FRESH_CELLS: [4]u8 = { 'F', 'R', 'S', 'H' }
const ROLLUP_RESUMED_CELLS: [4]u8 = { 'R', 'S', 'U', 'M' }
const ROLLUP_INVALIDATED_CELLS: [4]u8 = { 'I', 'N', 'V', 'D' }

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

func rollup_display_cells_for_kind(kind: i32) [4]u8 {
    switch kind {
    case ROLLUP_STEADY:
        return ROLLUP_STEADY_CELLS
    case ROLLUP_BUSY:
        return ROLLUP_BUSY_CELLS
    case ROLLUP_ATTENTION:
        return ROLLUP_ATTENTION_CELLS
    default:
        return ROLLUP_EMPTY_CELLS
    }
}

func rollup_display_cells_for_launch_status(status: u8) [4]u8 {
    switch status {
    case ROLLUP_LAUNCH_STATUS_FRESH:
        return ROLLUP_FRESH_CELLS
    case ROLLUP_LAUNCH_STATUS_RESUMED:
        return ROLLUP_RESUMED_CELLS
    case ROLLUP_LAUNCH_STATUS_INVALIDATED:
        return ROLLUP_INVALIDATED_CELLS
    default:
        return ROLLUP_EMPTY_CELLS
    }
}

func rollup_display_cells_with_manifest(summary: rollup_model.Summary, manifest: rollup_manifest.RollupManifest) [4]u8 {
    return rollup_display_cells_for_kind(rollup_kind_with_manifest(summary, manifest))
}

func rollup_display_cells(summary: rollup_model.Summary) [4]u8 {
    return rollup_display_cells_for_kind(rollup_kind(summary))
}

func write_rollup(summary: rollup_model.Summary) i32 {
    kind: i32 = rollup_kind(summary)
    writer: func() i32 = rollup_render_table.ROLLUP_WRITERS[usize(kind)]
    return writer()
}