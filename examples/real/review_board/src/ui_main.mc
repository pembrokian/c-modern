import io
import review_board_ui
import strings

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        return 97
    }

    state := review_board_ui.review_board_ui_init()
    keys: Slice<u8> = strings.bytes(args[1])
    for i in 0..keys.len {
        next := review_board_ui.review_board_ui_handle_key(state, keys[i])
        state = next.state
    }

    cells := review_board_ui.review_board_ui_display_cells(state)
    if io.write_line(str{ ptr: &cells[0], len: 4 }) != 0 {
        return 98
    }
    return 0
}