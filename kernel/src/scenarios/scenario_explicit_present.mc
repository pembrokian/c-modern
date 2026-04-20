import boot
import input_event
import kernel_dispatch
import program_catalog
import scenario_assert
import scenario_transport
import service_effect
import service_topology
import syscall
import update_store_service
import workflow/core

const FAIL_PRESENT_STAGE0: i32 = 27701
const FAIL_PRESENT_STAGE1: i32 = 27702
const FAIL_PRESENT_STAGE2: i32 = 27703
const FAIL_PRESENT_MANIFEST: i32 = 27704
const FAIL_PRESENT_APPLY_SCHEDULE: i32 = 27705
const FAIL_PRESENT_APPLY_WAITING: i32 = 27706
const FAIL_PRESENT_APPLY_DONE: i32 = 27707
const FAIL_PRESENT_BLANK: i32 = 27708
const FAIL_PRESENT_SELECT: i32 = 27709
const FAIL_PRESENT_SELECT_DISPLAY: i32 = 27710
const FAIL_PRESENT_LAUNCH: i32 = 27711
const FAIL_PRESENT_LAUNCH_DISPLAY: i32 = 27712
const FAIL_PRESENT_NOOP_DELIVERED: i32 = 27713
const FAIL_PRESENT_NOOP_DISPLAY: i32 = 27714
const FAIL_PRESENT_STEADY_DELIVERED: i32 = 27715
const FAIL_PRESENT_STEADY_DISPLAY: i32 = 27716

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

func run_explicit_present_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_PRESENT_STAGE0
    }

    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_BLANK) {
        return FAIL_PRESENT_BLANK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_PRESENT_STAGE0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(22))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_PRESENT_STAGE1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_PRESENT_STAGE2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(7, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_PRESENT_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_PRESENT_APPLY_SCHEDULE
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_PRESENT_APPLY_WAITING
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_PRESENT_APPLY_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_PRESENT_SELECT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_BLANK) {
        return FAIL_PRESENT_SELECT_DISPLAY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_PRESENT_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_FRESH) {
        return FAIL_PRESENT_LAUNCH_DISPLAY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(90))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, 90) {
        return FAIL_PRESENT_NOOP_DELIVERED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_FRESH) {
        return FAIL_PRESENT_NOOP_DISPLAY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(79))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, 79) {
        return FAIL_PRESENT_STEADY_DELIVERED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_STEADY) {
        return FAIL_PRESENT_STEADY_DISPLAY
    }

    return 0
}