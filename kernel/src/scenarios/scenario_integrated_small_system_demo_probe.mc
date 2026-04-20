import boot
import input_event
import journal_service
import kernel_dispatch
import launcher_service
import program_catalog
import review_board_ui
import scenario_assert
import scenario_helpers
import scenario_transport
import service_effect
import service_topology
import syscall
import update_store_service

const FAIL_SMALL_SYSTEM_SETUP: i32 = 29001
const FAIL_SMALL_SYSTEM_STATUS_EMPTY: i32 = 29002
const FAIL_SMALL_SYSTEM_APPLY_UPDATE: i32 = 29003
const FAIL_SMALL_SYSTEM_STATUS_ACTIVATED: i32 = 29004
const FAIL_SMALL_SYSTEM_SELECT_ISSUE_ROLLUP: i32 = 29005
const FAIL_SMALL_SYSTEM_LAUNCH_ISSUE_ROLLUP: i32 = 29006
const FAIL_SMALL_SYSTEM_DISPLAY_ISSUE_ROLLUP_FRESH: i32 = 29007
const FAIL_SMALL_SYSTEM_INPUT_ISSUE_ROLLUP: i32 = 29008
const FAIL_SMALL_SYSTEM_DISPLAY_ISSUE_ROLLUP_STEADY: i32 = 29009
const FAIL_SMALL_SYSTEM_SELECT_REVIEW_BOARD: i32 = 29010
const FAIL_SMALL_SYSTEM_LAUNCH_REVIEW_BOARD: i32 = 29011
const FAIL_SMALL_SYSTEM_DISPLAY_REVIEW_BOARD_AUDIT: i32 = 29012
const FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT0: i32 = 29013
const FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT1: i32 = 29014
const FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT2: i32 = 29015
const FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT3: i32 = 29016
const FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT4: i32 = 29017
const FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT5: i32 = 29018
const FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT6: i32 = 29019
const FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT7: i32 = 29020
const FAIL_SMALL_SYSTEM_DISPLAY_REVIEW_BOARD_PERSISTED: i32 = 29021
const FAIL_SMALL_SYSTEM_STATUS_AFTER_REBOOT: i32 = 29022
const FAIL_SMALL_SYSTEM_SELECT_REVIEW_BOARD_RELOADED: i32 = 29023
const FAIL_SMALL_SYSTEM_LAUNCH_REVIEW_BOARD_RELOADED: i32 = 29024
const FAIL_SMALL_SYSTEM_DISPLAY_REVIEW_BOARD_RELOADED: i32 = 29025

const DISPLAY_STATE_REVIEW_BOARD_AUDIT_BODY: [4]u8 = [4]u8{ 65, 83, 65, 66 }
const DISPLAY_STATE_REVIEW_BOARD_FOCUS_ESCALATED: [4]u8 = [4]u8{ 70, 69, 70, 83 }

func display_query_obs() syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.DISPLAY_ENDPOINT_ID, source_pid: 1, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: { 0, 0, 0, 0 } }
}

func expect_display(effect: service_effect.Effect, cells: [4]u8) bool {
    return scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, cells)
}

func expect_input_delivered(effect: service_effect.Effect, program: u8, value: u8) bool {
    return scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, value)
}

func expect_launch_reply(effect: service_effect.Effect, program: u8, classification: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    if payload[0] != program {
        return false
    }
    if payload[1] != program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE {
        return false
    }
    return payload[3] == classification
}

func launch_issue_rollup(state: *boot.KernelBootState, fail_select: i32, fail_launch: i32) i32 {
    effect := kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return fail_select
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_launch())
    if !expect_launch_reply(effect, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, launcher_service.LAUNCHER_RESUME_FRESH) {
        return fail_launch
    }
    return 0
}

func launch_review_board(state: *boot.KernelBootState, foreground: u8, fail_select: i32, fail_launch: i32) i32 {
    effect := kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_REVIEW_BOARD))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_REVIEW_BOARD, foreground, 0, 0) {
        return fail_select
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_launch())
    if !expect_launch_reply(effect, program_catalog.PROGRAM_ID_REVIEW_BOARD, launcher_service.LAUNCHER_RESUME_FRESH) {
        return fail_launch
    }
    return 0
}

func input_key(state: *boot.KernelBootState, program: u8, key: u8, fail: i32) i32 {
    effect := kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_input_key(key))
    if !expect_input_delivered(effect, program, key) {
        return fail
    }
    return 0
}

func drive_review_board_state(state: *boot.KernelBootState) i32 {
    result := input_key(state, program_catalog.PROGRAM_ID_REVIEW_BOARD, review_board_ui.REVIEW_BOARD_KEY_OPEN, FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT0)
    if result != 0 {
        return result
    }
    result = input_key(state, program_catalog.PROGRAM_ID_REVIEW_BOARD, review_board_ui.REVIEW_BOARD_KEY_TOGGLE_REGION, FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT1)
    if result != 0 {
        return result
    }
    result = input_key(state, program_catalog.PROGRAM_ID_REVIEW_BOARD, review_board_ui.REVIEW_BOARD_KEY_FOCUS, FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT2)
    if result != 0 {
        return result
    }
    result = input_key(state, program_catalog.PROGRAM_ID_REVIEW_BOARD, review_board_ui.REVIEW_BOARD_KEY_TOGGLE_REGION, FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT3)
    if result != 0 {
        return result
    }
    result = input_key(state, program_catalog.PROGRAM_ID_REVIEW_BOARD, review_board_ui.REVIEW_BOARD_KEY_URGENT, FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT4)
    if result != 0 {
        return result
    }
    result = input_key(state, program_catalog.PROGRAM_ID_REVIEW_BOARD, review_board_ui.REVIEW_BOARD_KEY_URGENT, FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT5)
    if result != 0 {
        return result
    }
    result = input_key(state, program_catalog.PROGRAM_ID_REVIEW_BOARD, review_board_ui.REVIEW_BOARD_KEY_URGENT, FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT6)
    if result != 0 {
        return result
    }
    return input_key(state, program_catalog.PROGRAM_ID_REVIEW_BOARD, review_board_ui.REVIEW_BOARD_KEY_TOGGLE_REGION, FAIL_SMALL_SYSTEM_REVIEW_BOARD_INPUT7)
}

func run_integrated_small_system_demo_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_SMALL_SYSTEM_SETUP
    }
    if !journal_service.journal_persist(journal_service.journal_init(service_topology.JOURNAL_SLOT.pid, 1)) {
        return FAIL_SMALL_SYSTEM_SETUP
    }

    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_status())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, 0, 0, 0, launcher_service.LAUNCHER_STATUS_NONE) {
        return FAIL_SMALL_SYSTEM_STATUS_EMPTY
    }

    result := scenario_helpers.scenario_apply_issue_rollup_update(&state, 7, 11, 22, 33, FAIL_SMALL_SYSTEM_APPLY_UPDATE)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_status())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 0, launcher_service.LAUNCHER_STATUS_ACTIVATED) {
        return FAIL_SMALL_SYSTEM_STATUS_ACTIVATED
    }

    result = launch_issue_rollup(&state, FAIL_SMALL_SYSTEM_SELECT_ISSUE_ROLLUP, FAIL_SMALL_SYSTEM_LAUNCH_ISSUE_ROLLUP)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, scenario_assert.DISPLAY_STATE_FRESH) {
        return FAIL_SMALL_SYSTEM_DISPLAY_ISSUE_ROLLUP_FRESH
    }
    result = input_key(&state, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 79, FAIL_SMALL_SYSTEM_INPUT_ISSUE_ROLLUP)
    if result != 0 {
        return result
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, scenario_assert.DISPLAY_STATE_STEADY) {
        return FAIL_SMALL_SYSTEM_DISPLAY_ISSUE_ROLLUP_STEADY
    }

    result = launch_review_board(&state, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, FAIL_SMALL_SYSTEM_SELECT_REVIEW_BOARD, FAIL_SMALL_SYSTEM_LAUNCH_REVIEW_BOARD)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_REVIEW_BOARD_AUDIT_BODY) {
        return FAIL_SMALL_SYSTEM_DISPLAY_REVIEW_BOARD_AUDIT
    }

    result = drive_review_board_state(&state)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_REVIEW_BOARD_FOCUS_ESCALATED) {
        return FAIL_SMALL_SYSTEM_DISPLAY_REVIEW_BOARD_PERSISTED
    }

    restarted := boot.kernel_init()

    effect = kernel_dispatch.kernel_dispatch_step(&restarted, scenario_transport.cmd_launcher_status())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 0, launcher_service.LAUNCHER_STATUS_ACTIVATED) {
        return FAIL_SMALL_SYSTEM_STATUS_AFTER_REBOOT
    }

    result = launch_review_board(&restarted, program_catalog.PROGRAM_ID_NONE, FAIL_SMALL_SYSTEM_SELECT_REVIEW_BOARD_RELOADED, FAIL_SMALL_SYSTEM_LAUNCH_REVIEW_BOARD_RELOADED)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(&restarted, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_REVIEW_BOARD_FOCUS_ESCALATED) {
        return FAIL_SMALL_SYSTEM_DISPLAY_REVIEW_BOARD_RELOADED
    }

    return 0
}