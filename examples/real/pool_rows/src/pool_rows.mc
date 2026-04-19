import io
import mem

const ROW_SLOT_BYTES: usize = 8
const ACTIVE_ROWS: usize = 2
const ROW_COUNT: usize = 5

type RowSlot = *u8

struct ActiveRow {
    slot: RowSlot
    used: usize
}

func take_row(pool: mem.Pool) RowSlot {
    return mem.pool_take(pool)
}

func return_row(pool: mem.Pool, slot: RowSlot) bool {
    return mem.pool_return(pool, slot)
}

func format_row(slot: RowSlot, index: usize) usize {
    bytes: Slice<u8> = Slice<u8>{ ptr: slot, len: ROW_SLOT_BYTES }
    bytes[0] = 114
    bytes[1] = 111
    bytes[2] = 119
    bytes[3] = 45
    bytes[4] = (u8)((usize)(48) + index)
    return 5
}

func row_text(slot: RowSlot, used: usize) str {
    return str{ ptr: slot, len: used }
}

func emit_row(slot: RowSlot, used: usize) i32 {
    if io.write_line(row_text(slot, used)) != 0 {
        return 1
    }
    return 0
}

func flush_rows(pool: mem.Pool, rows: [ACTIVE_ROWS]ActiveRow, count: usize) i32 {
    index: usize = 0
    while index < count {
        status: i32 = emit_row(rows[index].slot, rows[index].used)
        if status != 0 {
            return status
        }
        if !return_row(pool, rows[index].slot) {
            return 20
        }
        index = index + 1
    }
    return 0
}

func run() i32 {
    pool: mem.Pool = mem.pool_init(mem.default_allocator(), ROW_SLOT_BYTES, ACTIVE_ROWS)
    if mem.pool_available(pool) != ACTIVE_ROWS {
        return 90
    }
    defer mem.pool_deinit(pool)

    active: [ACTIVE_ROWS]ActiveRow
    count: usize = 0
    index: usize = 0
    while index < ROW_COUNT {
        slot: RowSlot = take_row(pool)
        if slot == nil {
            status: i32 = flush_rows(pool, active, count)
            if status != 0 {
                return status
            }
            count = 0
            slot = take_row(pool)
            if slot == nil {
                return 91
            }
        }

        active[count] = ActiveRow{ slot: slot, used: format_row(slot, index) }
        count = count + 1
        index = index + 1
    }

    final_status: i32 = flush_rows(pool, active, count)
    if final_status != 0 {
        return final_status
    }
    if mem.pool_available(pool) != ACTIVE_ROWS {
        return 92
    }
    return 0
}