import primitives
import program_catalog
import service_effect
import syscall
import update_store_service

const LAUNCHER_OP_LAUNCH: u8 = 65
const LAUNCHER_OP_IDENTIFY: u8 = 73
const LAUNCHER_OP_LIST: u8 = 76
const LAUNCHER_OP_MANIFEST: u8 = 77
const LAUNCHER_OP_SELECT: u8 = 83

const ISSUE_ROLLUP_MANIFEST_LEN: usize = 3

const LAUNCHER_RESUME_FRESH: u8 = 70
const LAUNCHER_RESUME_RESUMED: u8 = 82
const LAUNCHER_RESUME_INVALIDATED: u8 = 73

struct LauncherServiceState {
    pid: u32
    slot: u32
    selected: u8
    foreground: u8
    launches: u8
}

struct LauncherResult {
    state: LauncherServiceState
    update_store: update_store_service.UpdateStoreServiceState
    effect: service_effect.Effect
}

struct LauncherContext {
    update_store: update_store_service.UpdateStoreServiceState
    generation: u32
}

func launcher_result(state: LauncherServiceState, update_store: update_store_service.UpdateStoreServiceState, effect: service_effect.Effect) LauncherResult {
    return LauncherResult{ state: state, update_store: update_store, effect: effect }
}

func launcher_context(update_store: update_store_service.UpdateStoreServiceState, generation: u32) LauncherContext {
    return LauncherContext{ update_store: update_store, generation: generation }
}

func launcher_init(pid: u32, slot: u32) LauncherServiceState {
    return LauncherServiceState{ pid: pid, slot: slot, selected: 0, foreground: 0, launches: 0 }
}

func launcherwith(s: LauncherServiceState, selected: u8, foreground: u8, launches: u8) LauncherServiceState {
    return LauncherServiceState{ pid: s.pid, slot: s.slot, selected: selected, foreground: foreground, launches: launches }
}

func launcher_reply(status: syscall.SyscallStatus, len: usize, b0: u8, b1: u8, b2: u8, b3: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    if len > 0 {
        payload[0] = b0
    }
    if len > 1 {
        payload[1] = b1
    }
    if len > 2 {
        payload[2] = b2
    }
    if len > 3 {
        payload[3] = b3
    }
    return service_effect.effect_reply(status, len, payload)
}

func launcher_list(s: LauncherServiceState, ctx: LauncherContext) LauncherResult {
    first := program_catalog.program_descriptor_at(0)
    second := program_catalog.program_descriptor_at(1)
    return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.Ok, 4, u8(program_catalog.program_count()), first.id, second.id, s.selected))
}

func launcher_identify(s: LauncherServiceState, ctx: LauncherContext) LauncherResult {
    return launcher_result(
        s,
        ctx.update_store,
        launcher_reply(
            syscall.SyscallStatus.Ok,
            4,
            update_store_service.update_installed_program_id(ctx.update_store),
            update_store_service.update_installed_version(ctx.update_store),
            u8(update_store_service.update_installed_len(ctx.update_store)),
            s.selected))
}

func launcher_select(s: LauncherServiceState, ctx: LauncherContext, id: u8) LauncherResult {
    desc := program_catalog.program_descriptor_for_id(id)
    if !program_catalog.program_descriptor_is_valid(desc) {
        return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
    }
    next := launcherwith(s, id, s.foreground, s.launches)
    return launcher_result(next, ctx.update_store, launcher_reply(syscall.SyscallStatus.Ok, 2, next.selected, next.foreground, 0, 0))
}

func launcher_launch_classification(update_store: update_store_service.UpdateStoreServiceState, program: u8, launcher_generation: u32) u8 {
    if update_store_service.update_launch_matches_installed_program(update_store, program) {
        if update_store_service.update_launched_generation(update_store) != launcher_generation {
            return LAUNCHER_RESUME_RESUMED
        }
        return LAUNCHER_RESUME_FRESH
    }
    if update_store_service.update_launched_present(update_store) && update_store_service.update_launched_program_id(update_store) == program {
        return LAUNCHER_RESUME_INVALIDATED
    }
    return LAUNCHER_RESUME_FRESH
}

func launcher_launch(s: LauncherServiceState, ctx: LauncherContext) LauncherResult {
    desc := program_catalog.program_descriptor_for_id(s.selected)
    if !program_catalog.program_descriptor_is_valid(desc) {
        return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
    }
    if update_store_service.update_installed_program_id(ctx.update_store) != s.selected {
        return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
    }
    next := launcherwith(s, s.selected, s.selected, s.launches + 1)
    classification := launcher_launch_classification(ctx.update_store, next.foreground, ctx.generation)
    next_update_store := update_store_service.update_record_launch(ctx.update_store, next.foreground, ctx.generation)
    return launcher_result(next, next_update_store, launcher_reply(syscall.SyscallStatus.Ok, 4, next.foreground, program_catalog.program_launch_kind_code(desc), next.launches, classification))
}

func launcher_manifest(s: LauncherServiceState, ctx: LauncherContext, idx: usize) LauncherResult {
    if s.selected != program_catalog.PROGRAM_ID_ISSUE_ROLLUP {
        return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
    }
    if update_store_service.update_installed_program_id(ctx.update_store) != program_catalog.PROGRAM_ID_ISSUE_ROLLUP {
        return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
    }
    if update_store_service.update_installed_len(ctx.update_store) != ISSUE_ROLLUP_MANIFEST_LEN {
        return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
    }
    if idx >= ISSUE_ROLLUP_MANIFEST_LEN {
        return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
    }
    return launcher_result(
        s,
        ctx.update_store,
        launcher_reply(
            syscall.SyscallStatus.Ok,
            4,
            program_catalog.PROGRAM_ID_ISSUE_ROLLUP,
            update_store_service.update_installed_version(ctx.update_store),
            u8(ISSUE_ROLLUP_MANIFEST_LEN),
            update_store_service.update_installed_byte(ctx.update_store, idx)))
}

func handle(s: LauncherServiceState, ctx: LauncherContext, m: service_effect.Message) LauncherResult {
    if m.payload_len == 0 {
        return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
    }

    switch m.payload[0] {
    case LAUNCHER_OP_IDENTIFY:
        if m.payload_len != 1 {
            return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
        }
        return launcher_identify(s, ctx)
    case LAUNCHER_OP_LIST:
        if m.payload_len != 1 {
            return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
        }
        return launcher_list(s, ctx)
    case LAUNCHER_OP_MANIFEST:
        if m.payload_len != 2 {
            return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
        }
        return launcher_manifest(s, ctx, usize(m.payload[1]))
    case LAUNCHER_OP_SELECT:
        if m.payload_len != 2 {
            return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
        }
        return launcher_select(s, ctx, m.payload[1])
    case LAUNCHER_OP_LAUNCH:
        if m.payload_len != 1 {
            return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
        }
        return launcher_launch(s, ctx)
    default:
        return launcher_result(s, ctx.update_store, launcher_reply(syscall.SyscallStatus.InvalidArgument, 0, 0, 0, 0, 0))
    }
}

func launcher_foreground_id(s: LauncherServiceState) u8 {
    return s.foreground
}
