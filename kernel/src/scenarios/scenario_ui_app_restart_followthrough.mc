import boot
import input_event
import kernel_dispatch
import program_catalog
import scenario_assert
import scenario_helpers
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall
import update_store_service

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

func expect_delivered(effect: service_effect.Effect, value: u8) bool {
    return scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, value)
}

func run_fresh_install_phase(state: *boot.KernelBootState) i32 {
    result := scenario_helpers.scenario_apply_issue_rollup_update(state, 7, 11, 22, 33, FAIL_UI_STAGE0)
    if result != 0 {
        return result
    }

    effect := kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_UI_SELECT_FRESH
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UI_LAUNCH_FRESH
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_FRESH) {
        return FAIL_UI_DISPLAY_FRESH
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_input_key(79))
    if !expect_delivered(effect, 79) {
        return FAIL_UI_INPUT_FRESH
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.display_query_obs())
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
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_UI_SELECT_RESUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UI_LAUNCH_RESUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_RESUMED) {
        return FAIL_UI_DISPLAY_RESUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_input_key(79))
    if !expect_delivered(effect, 79) {
        return FAIL_UI_INPUT_RESUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.display_query_obs())
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
    result := scenario_helpers.scenario_apply_issue_rollup_update(state, 8, 44, 55, 66, FAIL_UI_RESTAGE0)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LAUNCHER))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LAUNCHER, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_UI_RESTART_INVALIDATED
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_UI_SELECT_INVALIDATED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UI_LAUNCH_INVALIDATED
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_INVALIDATED) {
        return FAIL_UI_DISPLAY_INVALIDATED
    }

    return 0
}

func run_ui_app_restart_followthrough_probe() i32 {
    setup := scenario_helpers.scenario_setup_clean_kernel()
    if !setup.ok {
        return FAIL_UI_STAGE0
    }

    state := setup.state

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