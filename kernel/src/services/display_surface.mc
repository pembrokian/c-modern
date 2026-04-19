import primitives
import service_effect
import syscall

const DISPLAY_CELL_COUNT: usize = 4

struct DisplaySurfaceState {
    pid: u32
    slot: u32
    cells: [DISPLAY_CELL_COUNT]u8
}

struct DisplayResult {
    state: DisplaySurfaceState
    effect: service_effect.Effect
}

func display_result(state: DisplaySurfaceState, effect: service_effect.Effect) DisplayResult {
    return DisplayResult{ state: state, effect: effect }
}

func display_init(pid: u32, slot: u32) DisplaySurfaceState {
    return DisplaySurfaceState{ pid: pid, slot: slot, cells: primitives.zero_payload() }
}

func displaywith(s: DisplaySurfaceState, cells: [DISPLAY_CELL_COUNT]u8) DisplaySurfaceState {
    return DisplaySurfaceState{ pid: s.pid, slot: s.slot, cells: cells }
}

func display_query(s: DisplaySurfaceState) DisplayResult {
    return display_result(s, service_effect.effect_reply(syscall.SyscallStatus.Ok, DISPLAY_CELL_COUNT, s.cells))
}

func display_present(s: DisplaySurfaceState, cells: [DISPLAY_CELL_COUNT]u8) DisplayResult {
    return display_result(displaywith(s, cells), service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()))
}

func handle(s: DisplaySurfaceState, m: service_effect.Message) DisplayResult {
    if m.payload_len == 0 {
        return display_query(s)
    }
    if m.payload_len != DISPLAY_CELL_COUNT {
        return display_result(s, service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()))
    }
    return display_present(s, m.payload)
}

func display_nonzero_count(s: DisplaySurfaceState) u8 {
    count: u8 = 0
    for i in 0..DISPLAY_CELL_COUNT {
        if s.cells[i] != 0 {
            count = count + 1
        }
    }
    return count
}