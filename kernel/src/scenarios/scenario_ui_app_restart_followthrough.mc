import boot
import input_event
import kernel_dispatch
import program_catalog
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall
import update_store_service
import workflow/core

// First update application (fresh install path).
const FAIL_UI_STAGE0: i32 = 28001
const FAIL_UI_STAGE1: i32 = 28002
const FAIL_UI_STAGE2: i32 = 28003
const FAIL_UI_MANIFEST: i32 = 28004
const FAIL_UI_APPLY_SCHEDULE: i32 = 28005
const FAIL_UI_APPLY_WAITING: i32 = 28006
const FAIL_UI_APPLY_DONE: i32 = 28007
const FAIL_UI_SELECT_FRESH: i32 = 28008
const FAIL_UI_LAUNCH_FRESH: i32 = 28009
const FAIL_UI_DISPLAY_FRESH: i32 = 28010
const FAIL_UI_INPUT_FRESH: i32 = 28011
const FAIL_UI_DISPLAY_FRESH_STEADY: i32 = 28012

// Post-restart resumed path.
const FAIL_UI_RESTART_RESUMED: i32 = 28013
const FAIL_UI_SELECT_RESUMED: i32 = 28014
const FAIL_UI_LAUNCH_RESUMED: i32 = 28015
const FAIL_UI_DISPLAY_RESUMED: i32 = 28016
const FAIL_UI_INPUT_RESUMED: i32 = 28017
const FAIL_UI_DISPLAY_RESUMED_STEADY: i32 = 28018
const FAIL_UI_CLEAR_UPDATE: i32 = 28019

// Second update application (invalidation path).
const FAIL_UI_RESTAGE0: i32 = 28020
const FAIL_UI_RESTAGE1: i32 = 28021
const FAIL_UI_RESTAGE2: i32 = 28022
const FAIL_UI_REMANIFEST: i32 = 28023
const FAIL_UI_REAPPLY_SCHEDULE: i32 = 28024
const FAIL_UI_REAPPLY_WAITING: i32 = 28025
const FAIL_UI_REAPPLY_DONE: i32 = 28026
const FAIL_UI_RESTART_INVALIDATED: i32 = 28027
const FAIL_UI_SELECT_INVALIDATED: i32 = 28028
const FAIL_UI_LAUNCH_INVALIDATED: i32 = 28029
const FAIL_UI_DISPLAY_INVALIDATED: i32 = 28030

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

func apply_issue_rollup_update(state: *boot.KernelBootState, version: u8, b0: u8, b1: u8, b2: u8, fail_base: i32) i32 {
    effect := kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_update_stage(b0))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return fail_base
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_update_stage(b1))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return fail_base + 1
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_update_stage(b2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return fail_base + 2
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_update_manifest(version, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return fail_base + 3
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return fail_base + 4
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return fail_base + 5
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return fail_base + 6
    }

    return 0
}

func run_fresh_install_phase(state: *boot.KernelBootState) i32 {
    result := apply_issue_rollup_update(state, 7, 11, 22, 33, FAIL_UI_STAGE0)
    if result != 0 {
        return result
    }

    effect := kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_UI_SELECT_FRESH
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UI_LAUNCH_FRESH
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_FRESH) {
        return FAIL_UI_DISPLAY_FRESH
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_input_key(79))
    if !expect_delivered(effect, 79) {
        return FAIL_UI_INPUT_FRESH
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_STEADY) {
        return FAIL_UI_DISPLAY_FRESH_STEADY
    }

    return 0
}

func run_restart_resumed_phase(state: *boot.KernelBootState) i32 {
    effect := kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LAUNCHER))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LAUNCHER, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_UI_RESTART_RESUMED
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_UI_SELECT_RESUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UI_LAUNCH_RESUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_RESUMED) {
        return FAIL_UI_DISPLAY_RESUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_input_key(79))
    if !expect_delivered(effect, 79) {
        return FAIL_UI_INPUT_RESUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_STEADY) {
        return FAIL_UI_DISPLAY_RESUMED_STEADY
    }

    return 0
}

func run_update_invalidated_phase(state: *boot.KernelBootState) i32 {
    effect := kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_update_clear())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UI_CLEAR_UPDATE
    }
    result := apply_issue_rollup_update(state, 8, 44, 55, 66, FAIL_UI_RESTAGE0)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LAUNCHER))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LAUNCHER, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_UI_RESTART_INVALIDATED
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_UI_SELECT_INVALIDATED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UI_LAUNCH_INVALIDATED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_INVALIDATED) {
        return FAIL_UI_DISPLAY_INVALIDATED
    }

    return 0
}

func run_ui_app_restart_followthrough_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_UI_STAGE0
    }

    state := boot.kernel_init()

    result := run_fresh_install_phase(&state)
    if result != 0 {
        return result
    }

    result = run_restart_resumed_phase(&state)
    if result != 0 {
        return result
    }

    return run_update_invalidated_phase(&state)
}