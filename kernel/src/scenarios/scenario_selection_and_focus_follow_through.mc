import boot
import input_event
import issue_rollup_app
import kernel_dispatch
import program_catalog
import scenario_transport
import service_effect
import service_topology
import syscall
import update_store_service
import workflow/core

const FAIL_FOCUS_STAGE0: i32 = 28401
const FAIL_FOCUS_STAGE1: i32 = 28402
const FAIL_FOCUS_STAGE2: i32 = 28403
const FAIL_FOCUS_MANIFEST: i32 = 28404
const FAIL_FOCUS_APPLY_SCHEDULE: i32 = 28405
const FAIL_FOCUS_APPLY_WAITING: i32 = 28406
const FAIL_FOCUS_APPLY_DONE: i32 = 28407
const FAIL_FOCUS_SELECT: i32 = 28408
const FAIL_FOCUS_LAUNCH: i32 = 28409
const FAIL_FOCUS_INPUT_OPEN_BODY: i32 = 28410
const FAIL_FOCUS_DISPLAY_STEADY_BODY: i32 = 28411
const FAIL_FOCUS_INPUT_TOGGLE_STATUS: i32 = 28412
const FAIL_FOCUS_DISPLAY_STEADY_STATUS: i32 = 28413
const FAIL_FOCUS_INPUT_BLOCKED_OPEN: i32 = 28414
const FAIL_FOCUS_DISPLAY_BLOCKED_OPEN: i32 = 28415
const FAIL_FOCUS_INPUT_TOGGLE_BODY: i32 = 28416
const FAIL_FOCUS_DISPLAY_BODY_RETURN: i32 = 28417
const FAIL_FOCUS_INPUT_OPEN_BUSY: i32 = 28418
const FAIL_FOCUS_DISPLAY_BUSY_BODY: i32 = 28419

const DISPLAY_STATE_STEADY_BODY: [4]u8 = [4]u8{ 83, 84, 67, 70 }
const DISPLAY_STATE_STEADY_STATUS: [4]u8 = [4]u8{ 83, 84, 67, 83 }
const DISPLAY_STATE_BUSY_BODY: [4]u8 = [4]u8{ 66, 85, 67, 70 }

func expect_reply(effect: service_effect.Effect, status: syscall.SyscallStatus, len: usize, b0: u8, b1: u8, b2: u8, b3: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != len {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == b0 && payload[1] == b1 && payload[2] == b2 && payload[3] == b3
}

func expect_workflow(effect: service_effect.Effect, status: syscall.SyscallStatus, state: u8, restart: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == state && payload[1] == restart
}

func display_query_obs() syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.DISPLAY_ENDPOINT_ID, source_pid: 1, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: { 0, 0, 0, 0 } }
}

func expect_delivered(effect: service_effect.Effect, value: u8) bool {
    return expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, value)
}

func expect_display(effect: service_effect.Effect, cells: [4]u8) bool {
    return expect_reply(effect, syscall.SyscallStatus.Ok, 4, cells[0], cells[1], cells[2], cells[3])
}

func run_selection_and_focus_follow_through_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_FOCUS_STAGE0
    }

    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_FOCUS_STAGE0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(4))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_FOCUS_STAGE1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(0))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_FOCUS_STAGE2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(1, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_FOCUS_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_FOCUS_APPLY_SCHEDULE
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_FOCUS_APPLY_WAITING
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_FOCUS_APPLY_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_FOCUS_SELECT
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_FOCUS_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(79))
    if !expect_delivered(effect, 79) {
        return FAIL_FOCUS_INPUT_OPEN_BODY
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_STEADY_BODY) {
        return FAIL_FOCUS_DISPLAY_STEADY_BODY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(issue_rollup_app.ISSUE_ROLLUP_KEY_FOCUS))
    if !expect_delivered(effect, issue_rollup_app.ISSUE_ROLLUP_KEY_FOCUS) {
        return FAIL_FOCUS_INPUT_TOGGLE_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_STEADY_STATUS) {
        return FAIL_FOCUS_DISPLAY_STEADY_STATUS
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(79))
    if !expect_delivered(effect, 79) {
        return FAIL_FOCUS_INPUT_BLOCKED_OPEN
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_STEADY_STATUS) {
        return FAIL_FOCUS_DISPLAY_BLOCKED_OPEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(issue_rollup_app.ISSUE_ROLLUP_KEY_FOCUS))
    if !expect_delivered(effect, issue_rollup_app.ISSUE_ROLLUP_KEY_FOCUS) {
        return FAIL_FOCUS_INPUT_TOGGLE_BODY
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_STEADY_BODY) {
        return FAIL_FOCUS_DISPLAY_BODY_RETURN
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(79))
    if !expect_delivered(effect, 79) {
        return FAIL_FOCUS_INPUT_OPEN_BUSY
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_BUSY_BODY) {
        return FAIL_FOCUS_DISPLAY_BUSY_BODY
    }

    return 0
}